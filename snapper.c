//
// Written by Frank Fleschner
//
// Copyright (c) 2008, ACS
// All rights reserved.
//
// Program that scans a filesystem (or, optionally, a folder), and outputs
// information about each file.  Specific information and formatting can be 
// configured, and ignore paths can be supplied to ignore paths and their
// children.
//
// This doesn't depend on my Mac OS X specific libraries, and should be fairly
// easily portable to POSIX compliant systems.
//
// Usage:
//		Flags:
//		-v Verbose output.
//		-V Mega-verbose output.  You'll be sorry if you do this one....
//		[-V implies -v]
//		-h Print usage statement.
//		-o Path to an output file (optional, prints to stdout if not specified)
//		-i Ignore string:
//			o If the string begins with a '/', then the string is treated as an
//			  absolute path.
//			o If it doesn't, then the string is matched against the name (the
//			  last path component) of scanned files.
//			o If the matched (ignored) file is a folder, any children will also
//			  be ignored.
//			o You can specify as many of these as you like.
//			EXAMPLES:
//			-i /dev  # This will ignore ONLY /dev, but not a folder such as 
//					 # /Users/dev or /Applications/develeper_tool.
//			-i .svn	 # This will ignore any files or folders named .svn, but
//					 # only if they're named EXACTLY .svn
//		-p Path to scan ("/" is used if none is provided).
//		-a Scan accross devices (ie, external HDs, server volumes, etc)
//		-H Print headers for the columns
//		-D Skip directories (only report files, still recurses, however)
//		-q Quiet mode: suppresses all output except for errors, etc.
//		-c Column String:
//			o The column string lets you specify which fields to print, what
//			  order, and what field and record delimiters to use.
//			o Spaces are stripped out so that column strings are easier to
//			  read when created.
//			o Codes are indicated by the '%' character.
//			o Some codes support lower case for human readable output, and
//			  capital case for raw (usually decimal) output.
//			o Codes are as follows:
//			- %		Use to print a '%' in the result (a double % is escaped)
//			- p		The file's absolute path
//			- A/a	The time of last access
//			- M/m	The time of last modification
//			- C/c	The time of last status change
//			- S/s	The size of the file (in bytes or KB/MB/GB)
//			- i		The files inode
//			- o		The file's owner (UID)
//			- g		The file's group (GID)
//			- P		The file's permissions (octal format)
//			- T/t	The file type:
//					D - Directory
//					L - Link
//					S - Socket
//					U - FIFO
//					B - Block special
//					C - Character special
//					F - Regular file
//					X - Something unexpected this way comes.
//		-s Sort token, takes one of the column codes above to sort by (just the
//		   character, not the preceding '%') (defaults to default FTS sorting,
//		   which is path based).  Capital case is descending and lower case is 
//		   ascending.  (Big letter signifies big values first, and vise versa.)
//		-f Field delimiter, one or more characters (defaults to \t)
//		-r Record delimiter, one or more characters (defaults to \n)
//		-C Path to configuration file.  Options configured in configuration file
//		   override anything specified in arguments.  But items specified in
//		   arguments and not in the configuration file are still honored.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/errno.h>
#include <string.h>
#include <stdarg.h>
#include <fts.h>
#include <sys/stat.h>
#include <limits.h>
#include <fcntl.h>
#include <time.h>
#include <assert.h>
#include <malloc/malloc.h>

#include "configfile.h"
#include "comm.h"
#include "snap_record.h"
#include "util_macros.h"

#define VERSION "0.9.6"

#ifdef __APPLE__
#define PROGNAME getprogname()
#else
#define PROGNAME "snapper"
#endif

#pragma mark Local data types

struct ignore_record_t {
	char		*ig_path;			// Ignored path
	size_t		ig_len;				// Ignored path length (to avoid strlen's)
};

#pragma mark Globals
struct globals_t {
	/* Stuff related to options */
	Boolean		verbose;					// Verbose output
	Boolean		megaVerbose;				// Mega-verbose output (scary)
	Boolean		printHeaders;				// do we print headers to the file?
	Boolean		quietMode;					// Suppress all output except errors
	Boolean		skipDirs;					// Skip directory output.
	
	char		*sortToken;					// How to sort the records.

	char		*pathToScan;				// Path to scan.
	char		*outputPath;				// Path to the output file.
	char		*configurationFilePath;		// Path to the config file.
	
	// Snap
	snap_t		snap;						// The snap object.
	
	// Processing related stuff
	int			fts_options;				// Options flags for fts_open()
		
	// Our array of ignored paths/names:
	struct ignore_record_t **ignore_array;	// The global ignored paths array.
	int			currentIgnoreSize;			// Current size of the ignore_array
	int			currentIgnoreCapacity;		// Current max size of the
	
	// OutPut niceness
	int			OutPut_printed;				// How much OutPut last printed.
} _globals;

struct globals_t *globals = &_globals;

#pragma mark function prototypes
// Prints standard OutPut to stderr.  Use quiet mode to supress.  
// is_status_update is for use of printing progress reports in the main loop.
static void OutPut(Boolean is_status_update, const char *format, ...) 
__attribute__((format(printf, 2, 3)));

// Path to add to array of ignored paths.
Boolean add_to_ignore_array(const char *path);

// Returns true if the path should be ignored (and children skipped), false
// otherwise.
Boolean should_be_ignored(const char *path, size_t pathlen, 
						  const char *name, size_t namelen);

// Deallocs all the allocated memory used in the ignore_array
void free_ignore_array();

// Our qsort compare function.
static int qsort_compare(const void * left, const void * right);

// Print usage
void usage(void);

#pragma mark function definitions
int main (int argc, char * argv[]) {
	FTS *ftsp;
	FTSENT *p;
	int filesVisited = 0, filesSkipped = 0;
	int c; opterr = 0;
	struct file_record_t *current_record = NULL;
	time_t start_time, end_time;
	config_file_t myConfigFile;

	start_time = time(0);
	
	/* Set defaults: */
	globals->fts_options			= FTS_PHYSICAL | FTS_XDEV;
	globals->verbose				= false;
	globals->megaVerbose			= false;
	globals->quietMode				= false;
	globals->printHeaders			= false;
	globals->skipDirs				= false;
	globals->sortToken				= NULL;
	globals->pathToScan				= strdup("/");
	globals->outputPath				= NULL;
	globals->configurationFilePath	= NULL;
	
	/* Initialize the snap */
	init_snap_record(&(globals->snap));

	/* Initialize the ignored path's array */
	globals->currentIgnoreCapacity = 10;
	globals->currentIgnoreSize = 0;
	CREATE(globals->ignore_array, 
		   globals->currentIgnoreCapacity * sizeof(struct ignore_record_t *));
	
	/* Parse options/input */
	while ((c = getopt(argc, argv, "vVDaqhHI:C:o:i:p:c:f:r:s:")) != -1)
	{
		switch (c) {
			case 'a':
				// Clear the bit which restricts fts to one device
				globals->fts_options &= ~(FTS_XDEV);
				break;
			case 'V':
				globals->megaVerbose = true;
				/* FALLTHROUGH:	-V implies -v */
			case 'v':
				globals->verbose = true;
				break;
			case 'q':
				globals->quietMode = true;
				break;
			case 'D':
				globals->skipDirs = true;
				break;
			case 'H':
				globals->printHeaders = true;
				break;
			case 'h':
				usage();
				exit(0);
			case 'r':
				set_snap_record_delimiter(&(globals->snap), optarg);
				break;
			case 'f':
				set_snap_field_delimiter(&(globals->snap), optarg);
				break;
			case 's':
				globals->sortToken = strdup(optarg);
				break;
			case 'o':
				globals->outputPath = strdup(optarg);
				break;
			case 'i':
				add_to_ignore_array(optarg);
				break;
			case 'p':
				//TODO: Error checking.
				globals->pathToScan = strdup(optarg);
				break;
			case 'c':
				// We got it, now save it to our global.
				set_snap_column_string(&(globals->snap), optarg);
				break;
			case 'C':
				globals->configurationFilePath = strdup(optarg);
				break;
			case '?':
			default:
				if (optopt == 'o' || optopt == 'i' || optopt == 'p' ||
					optopt == 'c' || optopt == 'r' || optopt == 'f' ||
					optopt == 'C' || optopt == 'I') {
					LogError("Option %c requires an argument.\n", optopt);
				}
				else {
					LogError("Unknown option: %c\n", 
							 isprint(optopt) ? optopt: '?');
				}
				break;
		}
	}
	
	/* Print some normal info to terminal */
	OutPut(false, "%s v%s, %s2008 ACS, Inc.\n", PROGNAME, VERSION, "©");
	
	// Parse config file (if it exists)
	if (globals->configurationFilePath)
	{
		char *myValStr;
		char **myValArray;
		size_t array_size;
		int i;

		LogV("Parsing configfile: %s\n", globals->configurationFilePath);
	
		init_config_file(&myConfigFile);
	
		read_config_file(globals->configurationFilePath, &myConfigFile);
		
		// For debugging config file:
		//debug_print_config_file_t(&myConfigFile);
		
		// Any Ignore files should be added to the ignore array.
		if (array_of_values_for_key(&myConfigFile, "ignore", 
									&myValArray, &array_size) != -1)
		{
			for (i = 0; i < array_size; i++)
			{
				assert(myValArray[i]);
				add_to_ignore_array(myValArray[i]);
			}
			free(myValArray);
		}
		
		if (value_for_key(&myConfigFile, "verbose", &myValStr, NULL) != -1)
		{
			if (!strncmp(myValStr, "1", MAX(strlen(myValStr), (size_t) 1)) ||
				!strncmp(myValStr, "yes", MAX(strlen(myValStr), (size_t) 3)) ||
				!strncmp(myValStr, "true", MAX(strlen(myValStr), (size_t) 4)) ||
				!strncmp(myValStr, "on", MAX(strlen(myValStr), (size_t) 2)))
			{
				globals->verbose = true;
			}
			else
				globals->verbose = false;
			free(myValStr);
		}

		if (value_for_key(&myConfigFile, "skipDirectories", 
						  &myValStr, NULL) != -1)
		{
			if (!strncmp(myValStr, "1", MAX(strlen(myValStr), (size_t) 1)) ||
				!strncmp(myValStr, "yes", MAX(strlen(myValStr), (size_t) 3)) ||
				!strncmp(myValStr, "true", MAX(strlen(myValStr), (size_t) 4)) ||
				!strncmp(myValStr, "on", MAX(strlen(myValStr), (size_t) 2)))
			{
				globals->skipDirs = true;
			}
			else
				globals->skipDirs = false;
			free(myValStr);
		}

		if (value_for_key(&myConfigFile, "printHeaders", &myValStr, NULL) != -1)
		{
			if (!strncmp(myValStr, "1", MAX(strlen(myValStr), (size_t) 1)) ||
				!strncmp(myValStr, "yes", MAX(strlen(myValStr), (size_t) 3)) ||
				!strncmp(myValStr, "true", MAX(strlen(myValStr), (size_t) 4)) ||
				!strncmp(myValStr, "on", MAX(strlen(myValStr), (size_t) 2)))
			{
				globals->printHeaders = true;
			}
			else
				globals->printHeaders = false;
			free(myValStr);
		}

		if (value_for_key(&myConfigFile, "quietMode", &myValStr, NULL) != -1)
		{
			if (!strncmp(myValStr, "1", MAX(strlen(myValStr), (size_t) 1)) ||
				!strncmp(myValStr, "yes", MAX(strlen(myValStr), (size_t) 3)) ||
				!strncmp(myValStr, "true", MAX(strlen(myValStr), (size_t) 4)) ||
				!strncmp(myValStr, "on", MAX(strlen(myValStr), (size_t) 2)))
			{
				globals->quietMode = true;
			}
			else
				globals->quietMode = false;
			free(myValStr);
		}

		if (value_for_key(&myConfigFile, "allDisks", &myValStr, NULL) != -1)
		{
			if (!strncmp(myValStr, "1", MAX(strlen(myValStr), (size_t) 1)) ||
				!strncmp(myValStr, "yes", MAX(strlen(myValStr), (size_t) 3)) ||
				!strncmp(myValStr, "true", MAX(strlen(myValStr), (size_t) 4)) ||
				!strncmp(myValStr, "on", MAX(strlen(myValStr), (size_t) 2)))
			{
				globals->fts_options &= ~(FTS_XDEV);
			}
			else
				globals->fts_options |= FTS_XDEV;
			free(myValStr);
		}
		
		if (value_for_key(&myConfigFile, "fieldDelimiter", 
						  &myValStr, NULL) != -1)
		{
			if (myValStr && *myValStr)
			{
				set_snap_field_delimiter(&(globals->snap), myValStr);
			}

			free(myValStr);
		}
		
		if (value_for_key(&myConfigFile, "recordDelimiter", 
						  &myValStr, NULL) != -1)
		{
			if (myValStr && *myValStr)
			{
				set_snap_record_delimiter(&(globals->snap), myValStr);
			}
			else
				
				free(myValStr);
		}

		if (value_for_key(&myConfigFile, "pathToScan", &myValStr, NULL) != -1)
		{
			if (myValStr && *myValStr)
			{
				free(globals->pathToScan);
				globals->pathToScan = myValStr;
			}
		}

		if (value_for_key(&myConfigFile, "outputPath", &myValStr, NULL) != -1)
		{
			if (myValStr && *myValStr)
			{
				if (globals->outputPath)
					free(globals->outputPath);
				
				globals->outputPath = myValStr;
			}
		}

		if (value_for_key(&myConfigFile, "sortTolken", &myValStr, NULL) != -1)
		{
			if (myValStr && *myValStr)
			{
				if (globals->sortToken)
					free(globals->sortToken);
				
				globals->sortToken = myValStr;
			}
		}
		
		if (value_for_key(&myConfigFile, "columnString", &myValStr, NULL) != -1)
		{
			if (myValStr && *myValStr)
			{
				set_snap_column_string(&(globals->snap), myValStr);
			}
		}

		if (value_for_key(&myConfigFile, "printHeaders", &myValStr, NULL) != -1)
		{
			if (!strncmp(myValStr, "1", MAX(strlen(myValStr), (size_t) 1)) ||
				!strncmp(myValStr, "yes", MAX(strlen(myValStr), (size_t) 3)) ||
				!strncmp(myValStr, "true", MAX(strlen(myValStr), (size_t) 4)) ||
				!strncmp(myValStr, "on", MAX(strlen(myValStr), (size_t) 2)))
			{
				globals->printHeaders = true;
			}
			else
				globals->printHeaders = false;
			free(myValStr);
		}
		
		// Get rid of all the crap!
		done_with_config_file(&myConfigFile);
	}
	
	
	/* Print some verbose messages */
	if (geteuid() != 0)
	{
		LogError("\nWARNING: %s should be run as ROOT for best results!\n",
				 PROGNAME);
	}
	
	LogV("Verbose mode is %s\n" "Mega Verbose mode is %s\n"
		 "Scan accross disks is %s\n" "Skip directories is %s\n" 
		 "Output path is %s\n" "Scan path is %s\n" "Column string is %s\n" 
		 "MAX_RECORD_LENGTH is %d\n" "INITIAL_ARRAY_SIZE is %d\n" 
		 "ARRAY_CHUNK_SIZE is %d\n",
		 (globals->verbose) ? "ON" : "OFF",
		 (globals->megaVerbose) ? "ON" : "OFF",
		 (globals->fts_options & FTS_XDEV) ? "OFF" : "ON",
		 (globals->skipDirs) ? "ON" : "OFF",
		 globals->outputPath, globals->pathToScan, globals->snap.column_string,
		 MAX_RECORD_LENGTH, INITIAL_ARRAY_SIZE, ARRAY_CHUNK_SIZE);
	
	/* Traverse the hierarchy (do the work) */
	char *pathargv[] = {globals->pathToScan, NULL};
	if ((ftsp = fts_open(pathargv, globals->fts_options, NULL)) == NULL) {
		LogError("fts_open: %s\n", strerror(errno));
		exit(1);
	}

	OutPut(false, "Beginning scan:\n");
	
	while ((p = fts_read(ftsp)) != NULL) {
		
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
		if (!(filesVisited % 10000))
		{
			OutPut(true, "%dk files scanned...", filesVisited/1000);
		}
		
		if (should_be_ignored(p->fts_path, p->fts_pathlen,
							  p->fts_name, p->fts_namelen))
		{
			LogV("Found %s, which is on the ignore list.  "
				 "Ignoring it and its children.\n", p->fts_path);
			filesSkipped++;
			fts_set(ftsp, p, FTS_SKIP);
			continue;
		}
		
		// If we're skipping directories, and this is a directory, continue
		// to the next iteration.
		if (globals->skipDirs && S_ISDIR(p->fts_statp->st_mode))
		{
			filesSkipped++;
			continue;
		}
		
		// Create our record, and add it to the array.
		CREATE(current_record, sizeof(file_record));
		current_record->re_path = strdup(p->fts_path);
		current_record->re_atime = p->fts_statp->st_atime;
		current_record->re_atime_str = NULL;
		current_record->re_mtime = p->fts_statp->st_mtime;
		current_record->re_mtime_str = NULL;
		current_record->re_ctime = p->fts_statp->st_ctime;
		current_record->re_ctime_str = NULL;
		current_record->re_size = p->fts_statp->st_size;
		current_record->re_ino = p->fts_statp->st_ino;
		current_record->re_uid = p->fts_statp->st_uid;
		current_record->re_gid = p->fts_statp->st_gid;
		current_record->re_mode = p->fts_statp->st_mode;
		
		if (S_ISDIR(current_record->re_mode))
		{
			current_record->re_type = 'D';
		}
		else if (S_ISLNK(current_record->re_mode))
		{
			current_record->re_type = 'L';
		}
		else if (S_ISSOCK(current_record->re_mode))
		{
			current_record->re_type = 'S';
		}
		else if (S_ISFIFO(current_record->re_mode))
		{
			current_record->re_type = 'U';
		}
		else if (S_ISBLK(current_record->re_mode))
		{
			current_record->re_type = 'B';
		}
		else if (S_ISCHR(current_record->re_mode))
		{
			current_record->re_type = 'C';
		}
		else if (S_ISREG(current_record->re_mode))
		{
			current_record->re_type = 'F';
		}
		else
		{
			current_record->re_type = 'X';
		}

		add_record_to_snap(&(globals->snap), current_record);
		
		LogMV("Visiting: %s\n", p->fts_path);
	}
	
	fts_close(ftsp);
	
	// Sort if we need to.
	if (globals->sortToken &&
		!(*globals->sortToken == 'p' || *globals->sortToken == 'P'))
	{
		OutPut(false, "\nSorting...");
		qsort(globals->snap.master_array, globals->snap.currentArraySize,
			  sizeof(file_record *), qsort_compare);
		OutPut(false, "Done!");
	}
		
	/* Hierarchy traversal complete, post process */
	LogV("\nVisited %d file%s.\n", filesVisited, 
		 (filesVisited != 1) ? "s" : "");
	LogV("Skipped %d file%s.\n", filesSkipped, (filesSkipped != 1) ? "s" : "");
	
	OutPut(false, "\nWritting file...");

	write_snap_record_to_file(&(globals->snap), globals->outputPath);
	
	OutPut(false, "Done.\n");
	
	// Free what we need to free.
	free_snap(&(globals->snap));
	free_ignore_array();
	free(globals->pathToScan);
	if (globals->outputPath)
		free(globals->outputPath);
	if (globals->sortToken)
		free(globals->sortToken);
	if (globals->configurationFilePath)
		free(globals->configurationFilePath);
	
	end_time = time(0);
	
	OutPut(false, "Scanned %d files in %ld seconds "
		   "for an effective rate of %.1f files/s\n", 
		   filesVisited, end_time - start_time,
		   (float)filesVisited / ((float)end_time - (float)start_time));
	
	return 0;
}

static int qsort_compare(const void * left, const void * right)
{
	struct file_record_t **left_r = (struct file_record_t **) left;
	struct file_record_t **right_r = (struct file_record_t **) right;
	
#define COMPARE(left, right, member) \
(((*left)->member) > ((*right)->member)) ? 1 : \
((((*left)->member) < ((*right)->member)) ? -1 : 0)
	
	switch (*(globals->sortToken)) {
		case 's':
			return COMPARE(left_r, right_r, re_size);
			break;
		case 'S':
			return COMPARE(right_r, left_r, re_size);
			break;
		case 'a':
			return COMPARE(left_r, right_r, re_atime);
			break;
		case 'A':
			return COMPARE(right_r, left_r, re_atime);
			break;
		case 'm':
			return COMPARE(left_r, right_r, re_mtime);
			break;
		case 'M':
			return COMPARE(right_r, left_r, re_mtime);
			break;
		case 'c':
			return COMPARE(left_r, right_r, re_ctime);
			break;
		case 'C':
			return COMPARE(right_r, left_r, re_ctime);
			break;
		case 'i':
			return COMPARE(left_r, right_r, re_ino);
			break;
		case 'I':
			return COMPARE(right_r, left_r, re_ino);
			break;
		case 'o':
			return COMPARE(left_r, right_r, re_uid);
			break;
		case 'O':
			return COMPARE(right_r, left_r, re_uid);
			break;
		case 'g':
			return COMPARE(left_r, right_r, re_gid);
			break;
		case 'G':
			return COMPARE(right_r, left_r, re_gid);
			break;
		default:
			return 0; // Could be conceivably reached....
			break;
	}
	return 0; // Shouldn't be reached.
}
#undef COMPARE


// Path to add to array of ignored paths.
Boolean add_to_ignore_array(const char *path)
{
	// Create and initialize our new record.
	struct ignore_record_t *newRecord;
	CREATE(newRecord, sizeof(struct ignore_record_t));
	
	newRecord->ig_path = strdup(path);
	newRecord->ig_len = strlen(path);
	
	// Grow the array, if necessary.
	if (globals->currentIgnoreSize >= globals->currentIgnoreCapacity)
	{
		LogMV("Growing Ignore array from %d to %d\n",
			  globals->currentIgnoreCapacity,
			  globals->currentIgnoreCapacity + 5);
		
		RECREATE(globals->ignore_array,
				 (globals->currentIgnoreCapacity + 5) * 
				 sizeof(struct ignore_record_t *));
		
		// Keep track of our capacity
		globals->currentIgnoreCapacity += 5;
	}
	
	// Assign.
	globals->ignore_array[globals->currentIgnoreSize] = newRecord;
	
	// Keep track of our size.
	globals->currentIgnoreSize++;
	
	// Return successful.
	return true;
}

// Returns true if the path should be ignored (and children skipped), false
// otherwise.
Boolean should_be_ignored(const char *path, size_t pathlen, 
						  const char *name, size_t namelen)
{
	// Iterate through our ignore_array.  If the first character is a '/', then
	// we check against the (absolute) path.  Otherwise, we only check against
	// the name.
	int i;
	for (i = 0; i < globals->currentIgnoreSize; i++) {
		if (globals->ignore_array[i]->ig_path[0] == '/')
		{
			if (!strncmp(path, globals->ignore_array[i]->ig_path,
						 MAX(pathlen, globals->ignore_array[i]->ig_len)))
			{
				return true;
			}
		}
		else
		{
			if (!strncmp(name, globals->ignore_array[i]->ig_path, 
						 MAX(namelen, globals->ignore_array[i]->ig_len)))
			{
				return true;
			}
		}
	}
	return false;
}

// Deallocs all the allocated memory used in the ignore_array
void free_ignore_array()
{
	// Walk through the ignore_array and free everything we strdup'ed, then
	// free the array.
	int i;
	for (i = 0; i < globals->currentIgnoreSize; i++) {
		free(globals->ignore_array[i]->ig_path);
	}
	
	free(globals->ignore_array);
	globals->ignore_array = NULL;
}

// For our regular output.  Set quiet mode to supress
void OutPut(Boolean is_status_update, const char *format, ...)
{	
	char backspace_buffer[50], final_format[1024];
 	int printed = 0, i = 0;
	
	// If quiet mode, we don't print anything.
	if (globals->quietMode) {
        return;
    }
	
	// if this is a status message, fill our backspace buffer.
	if (is_status_update && globals->OutPut_printed > 0)
	{
		// Construct a null-terminated string of backspaces to remove our
		// last OutPut:
		for (i = 0; i < globals->OutPut_printed && i < 50; i++)
		{
			backspace_buffer[i] = '\b';
		}
		backspace_buffer[i] = '\0';
	}
	
	// If we have a backspace buffer, prepend that to our final output.
	snprintf(final_format, 1024, "%s%s",
			 (i > 0) ? backspace_buffer : "",
			 format);
	
	// Go one like normal:
	
	va_list ap;
    va_start(ap, format);
    printed = vfprintf(stderr, final_format, ap);
    va_end(ap);
	
	// If this is a status update, keep track of how much we printed, so we can
	// overwrite it next time we have a status update.
	if (is_status_update == true)
	{
		globals->OutPut_printed = printed - globals->OutPut_printed;
	}
	else
	{
		globals->OutPut_printed = 0;
	}
}

void usage(void)
{
	OutPut(false, "%s v%s, %s2008 ACS, Inc.\n", PROGNAME, VERSION, "©");
	
	OutPut(false, "%s",
"	Flags:\n"
"	-v Verbose output.\n"
"	-V Mega-verbose output.  You'll be sorry if you do this one....\n"
"	[-V implies -v]\n"
"	-h Print usage statement.\n"
"	-o Path to an output file (optional, prints to stdout if not specified)\n"
"	-i Ignore string:\n"
"		o If the string begins with a \'/\', then the string is treated as an\n"
"		  absolute path.\n"
"		o If it doesn't, then the string is matched against the name (the\n"
"		  last path component) of scanned files.\n"
"		o If the matched (ignored) file is a folder, any children will also\n"
"		  be ignored.\n"
"		o You can specify as many of these as you like.\n"
"		EXAMPLES:\n"
"		-i /dev	# This will ignore ONLY /dev, but not a folder such as\n"
"					# /Users/dev or /Applications/develeper_tool.\n"
"		-i .svn	# This will ignore any files or folders named .svn, but\n"
"					# only if they're named EXACTLY .svn\n"
"	-p Path to scan (\"/\" is used if none is provided).\n"
"	-a Scan accross devices (ie, external HDs, server volumes, etc)\n"
"	-H Print headers for the columns\n"
"	-D Skip directories (only report files, still recurses, however)\n"
"	-q Quiet mode: suppresses all output except for errors, etc.\n"
"	-c Column String:\n"
"		o The column string lets you specify which fields to print, what\n"
"		  order, and what field and record delimiters to use.\n"
"		o Spaces are stripped out so that column strings are easier to\n"
"		  read when created.\n"
"		o Codes are indicated by the \'%\' character.\n"
"		o Some codes support lower case for human readable output, and\n"
"		  capital case for raw (usually decimal) output.\n"
"		o Codes are as follows:\n"
"		- %	Use to print a \'%\' in the result (a double % is escaped)\n"
"		- p	The file's absolute path\n"
"		- A/a	The time of last access\n"
"		- M/m	The time of last modification\n"
"		- C/c	The time of last status change\n"
"		- S/s	The size of the file (in bytes or KB/MB/GB)\n"
"		- i	The files inode\n"
"		- o	The file's owner (UID)\n"
"		- g	The file's group (GID)\n"
"		- P	The file's permissions (octal format)\n"
"		- T/t	The file type:\n"
"				D - Directory\n"
"				L - Link\n"
"				S - Socket\n"
"				U - FIFO\n"
"				B - Block special\n"
"				C - Character special\n"
"				F - Regular file\n"
"				X - Something unexpected this way comes.\n"
"	-s Sort token, takes one of the column codes above to sort by (just the\n"
"	   character, not the preceding '%') (defaults to default FTS sorting,\n"
"	   which is path based).  Capital case is descending and lower case is\n"
"	   ascending.  (Big letter signifies big values first, and vise versa.)\n"
"	-f Field delimiter, one or more characters (defaults to \\t)\n"
"	-r Record delimiter, one or more characters (defaults to \\n)\n"
"	-C Path to configuration file.  Options configured in configuration file\n"
"	   override anything specified in arguments.  But items specified in\n"
"	   arguments and not in the configuration file are still honored.\n"
		   );
}
