//
// Written by Frank Fleschner
//
// Copyright (c) 2009, ACS
// All rights reserved.
//
// Program that clones the permissions of one folder to another.
//
// This doesn't depend on my Mac OS X specific libraries, and should be fairly
// easily portable to POSIX compliant systems.

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fts.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>

#include "comm.h"

#ifdef __APPLE__
#define PROGNAME getprogname()
#else
#define PROGNAME "clop"
#endif

#define VERSION "0.1"

typedef unsigned char Boolean;
#define YES 1
#define NO 0

/* Globals, in one (large(ish)) struct. */
struct globals_t {
    Boolean		verbose;			// Verbose output
	Boolean		megaVerbose;		// Mega-verbose output (scary)
	
	char		*source;			// Source path
	char		*destination;		// Destination path

	Boolean		warnOnMissing;		// Warn if something is in the source,
									// but not the target.
} _globals;

struct globals_t *globals = &_globals;

// destination path.
int create_new_path(const char *source_path, 
					size_t source_path_size,
					const char *destination_path_prefix,
					int level, 
					char *buffer,
					size_t buffer_size);

// Prints the usage to stderr
void usage(void);

int main (int argc, char * argv[]) {

	char absolutePath[PATH_MAX];
	struct stat info;

    /* DEFAULTS */
	globals->verbose					= NO;
	globals->megaVerbose				= NO;
	globals->warnOnMissing				= NO;
	globals->source						= NULL;
	globals->destination				= NULL;

	/* PARSE ARGUMENTS */
	
	int c; opterr = 0;
	while ((c = getopt(argc, argv, "iwvhVd:s:")) != -1)
	{
		switch (c) {
			case 'V':
				globals->megaVerbose = YES;
				/* FALLTHROUGH:	-V implies -v */
			case 'v':
				globals->verbose = YES;
				break;
			case 'd':
				if (realpath(optarg, absolutePath) == NULL)
				{
					// Can't find file ...
					LogError("Can't find destination file %s\n", optarg);
					exit(1);
				}
				
				if (stat(absolutePath, &info) == -1)
				{
					LogError("%s: %s\n", absolutePath, strerror(errno));
					exit(1);
				}
				
				if (!S_ISDIR(info.st_mode))
				{
					LogError("Destination Path must be a DIRECTORY.\n");
					exit(1);
				}
				
				globals->destination = strdup(absolutePath);
				
				break;
			case 's':
				if (realpath(optarg, absolutePath) == NULL)
				{
					// Can't find file ...
					LogError("Can't find source file %s\n", optarg);
					exit(1);
				}
				
				if (stat(absolutePath, &info) == -1)
				{
					LogError("%s: %s\n", absolutePath, strerror(errno));
					exit(1);
				}
				
				if (!S_ISDIR(info.st_mode))
				{
					LogError("Source Path must be a DIRECTORY.\n");
					exit(1);
				}
				
				globals->source = strdup(absolutePath);
				
				break;
			case 'h':
				usage();
				exit(0);
				break; /* Not Reached */
			case 'i':
				LogError("%s v%s, %s2009, ACS, Inc.\n", PROGNAME, VERSION, "©");
				exit(0);
				break; /* Not Reached */
			case 'w':
				globals->warnOnMissing = YES;
				break;
			case '?':
			default:
				if (optopt == 'd' || optopt == 's') {
					LogError("Option %c requires an argument.\n", optopt);
				}
				else {
					LogError("Unknown option: %c\n", 
							 isprint(optopt) ? optopt: '?');
				}
				break;
		}
	}
	
	if (!globals->source || !globals->destination)
	{
		LogError("Must supply a source and a destination!\n");
		usage();
		exit(0);
	}
	
	if (geteuid() != 0)
	{
		LogError("Warning: %s is much more effective (and destrucive) when run"
				 " as root!\n", PROGNAME);
	}
	
	FTS *ftsp;
	FTSENT *p;
	int filesChanged = 0, filesVisited = 0, fts_options = 0;
	char *pathargv[] = {globals->source, NULL};
	
	fts_options |= FTS_PHYSICAL;
	if ((ftsp = fts_open(pathargv, fts_options, NULL)) == NULL)
	{
		LogError("fts_open: %s\n", strerror(errno));
		exit(1);
	}
	
	while ((p = fts_read(ftsp)) != NULL)
	{
		
		// Skip certain/special/error files, etc.
		switch (p->fts_info) {
			case FTS_D:
				break;
			case FTS_DNR:
				LogError("%s: %s\n", p->fts_path, strerror(p->fts_errno));
				break;
			case FTS_DP:
				continue;
			case FTS_ERR:
			case FTS_NS:
				LogError("%s: %s\n", p->fts_path, strerror(p->fts_errno));
				continue;
			case FTS_SL:
			case FTS_SLNONE:
			default:
				break;
		}
		
		filesVisited++;
		char newPath[PATH_MAX+1];
		bzero(newPath, PATH_MAX+1);
		create_new_path(p->fts_path, 
						p->fts_pathlen, 
						globals->destination, 
						p->fts_level, 
						newPath, 
						PATH_MAX);
		
		if (stat(newPath, &info) == -1)
		{
			// Fail silently if the file doesn't exist, since we expect that to
			// happen a fair amount.
			if (errno != ENOENT)
			{
				LogError("%s: %s\n", newPath, strerror(errno));
			}
			else if (globals->warnOnMissing)
			{
				LogError("[MISSING_T]: %s\n", newPath);
			}
			else
			{
				LogV("[MISSING_T]: %s\n", newPath);
			}
			
			continue;
		}
		
		int change = 0;
		if (p->fts_statp->st_uid != info.st_uid)
		{
			LogError("Changing owner on %s\n", newPath);
			
			if (chown(newPath, p->fts_statp->st_uid, -1) == -1)
			{
				LogError("%s: %s\n", newPath, strerror(errno));
			}
			change++;
		}
		if (p->fts_statp->st_gid != info.st_gid)
		{
			LogError("Changing group on %s\n", newPath);
			
			if (chown(newPath, -1, p->fts_statp->st_gid) == -1)
			{
				LogError("%s: %s\n", newPath, strerror(errno));
			}
			change++;
		}		
		if (p->fts_statp->st_mode != info.st_mode)
		{
			LogError("Changing mode on %s\n", newPath);
			
			if (chmod(newPath, p->fts_statp->st_mode) == -1)
			{
				LogError("%s: %s\n", newPath, strerror(errno));
			}
			change++;
		}
		
		filesChanged += (change != 0);
	}
	
	// Cleanup
	fts_close(ftsp);
	
	if (globals->destination)
	{
		free(globals->destination);
	}
	
	if (globals->source)
	{
		free(globals->source);
	}
	
	LogV("Visited %d files, changed %d files.\n", filesVisited, filesChanged);
	
    return 0;
}

// Function that takes the path from the source, and converts it to the
// destination path.
int create_new_path(const char *source_path, 
					size_t source_path_size,
					const char *destination_path_prefix,
					int level, 
					char *buffer,
					size_t buffer_size)
{
	char *pointer = NULL;
	int i = 0;
	
	for (pointer = (char *)source_path + source_path_size - 1;
		 pointer > source_path && *pointer && i < level;
		 pointer--)
	{
		if (*pointer == '/')
		{
			i++;
		}
	}
	
	// We skip one past the '/', so we advance back over that one.
	pointer++;
	
	snprintf(buffer, buffer_size, "%s%s", destination_path_prefix, pointer);
	return 0;
}

// Prints the usage to stderr
void usage(void)
{
	fprintf(stderr, "%s -s </path/to/source> -d </path/to/destination> [-w -v -V -h -i]\n"
			"w = warn on missing file in destination\nv = verbose\n"
			"V = mega-verbose\nh = print usage\ni = print version info\n", 
			getprogname());
}
