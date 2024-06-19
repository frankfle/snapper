/*
 *  snap_record.h
 *  snapper
 *
 *  Created by Frank Fleschner on 5/22/09.
 *  Copyright 2009 ACS. All rights reserved.
 *
 */

#include <unistd.h>
#include <sys/time.h>

#pragma mark Reallocation defines
// Initial size of the array.  A good number would be right around the number
// of files that you would expect to see out of the gate, or a few (relative)
// less.
#define INITIAL_ARRAY_SIZE	200000
// Chunks to add to the array to minimize use of the realloc call.
#define ARRAY_CHUNK_SIZE	50000

#define COLUMN_STRING_MAX	64
#define MAX_RECORD_LENGTH	(PATH_MAX + 100)

#pragma mark Data Types
struct file_record_t {
	char		*re_path;			// File's path
	time_t		re_atime;			// File's time of last access
	char		*re_atime_str;		// Human readible string of above.
	time_t		re_mtime;			// File's time of last modification
	char		*re_mtime_str;		// Human readible string of above.
	time_t		re_ctime;			// File's time of last status change
	char		*re_ctime_str;		// Human readible string of above.
	off_t		re_size;			// File size, in bytes
	ino_t		re_ino;				// File inode
	uid_t		re_uid;				// File's owner ID
	gid_t		re_gid;				// File's group ID
	mode_t		re_mode;			// File's mode (permissions)
	char		re_type;			// File's type (F for file, D for dir, etc)
	
	/* Internal: */
	char		re_selected;		// Indicates that this file is "selected"
									// For use with the 'cliff' tool.
									// This is added and controlled by the
									// 'snapedit' tool.
};

typedef struct file_record_t file_record;

struct snap_record_t {
	char		valid;
	
	char		*column_string;
	char		*field_delimiter;
	char		*record_delimiter;
	
	// Our record array:
	int			currentArraySize;			// Current size of the array.
	int			currentArrayCapacity;		// Current max size of the array.
	file_record **master_array;				// The master record array.
};

typedef struct snap_record_t snap_t;

#pragma mark Functions

// Initializes the snap_t.  Does not allocate.
int init_snap_record(snap_t *snap);

// Set the column string for the snap
int set_snap_column_string(snap_t *snap, char *column_string);

// Set the field delimiter for the snap
int set_snap_field_delimiter(snap_t *snap, char *field_delimiter);

// Set the record delimiter for the snap
int set_snap_record_delimiter(snap_t *snap, char *record_delimiter);

// Writes what's in the snap record to a file at path.
int write_snap_record_to_file(snap_t *snap, char *path);

// Reads a snap file from given path into a snap record.
int read_snap_record_from_file(snap_t *snap, char *path);

// Add a file entry to an array
int add_record_to_snap(snap_t *snap, file_record *record);

// Free's all the memory (but not the snap record itself);
int free_snap(snap_t *snap);
