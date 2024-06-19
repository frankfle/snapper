/*
 *  snap_record.c
 *  snapper
 *
 *  Created by Frank Fleschner on 5/22/09.
 *  Copyright 2009 ACS. All rights reserved.
 *
 */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <limits.h>
#include <assert.h>

#include "comm.h"
#include "snap_record.h"
#include "util_macros.h"

#pragma mark Forward Declarations
// Prints the header string to the provided buffer.
int hprintbuf(snap_t *snap, char **buf, size_t maxlen);

// Print a record description to a buffer
int rprintbuf(snap_t *snap, file_record *record, char **buf, size_t maxlen);

// Simple function to return the next line of a file stream.  Returns negative
// upon failure, or size of line (including a possibilty of 0), upon success.
// Originally called getline(), but now that's part of the C standard library.
int _getline(FILE *file, char *buffer, size_t buflen);

int parse_snapper_file_line(int column_tracker[], 
							char *line, 
							file_record *record);

int _getline(FILE *file, char *buffer, size_t buflen)
{
	char c, *curr_input = buffer;
	
	assert(buflen < LONG_MAX);
	
	while ((c = fgetc(file)) != EOF &&
		   (curr_input - buffer < buflen))
	{
		if (c == '\n' || c == '\r')
		{
			*curr_input++ = '\0';
			break;
		}
		
		*curr_input++ = c;
	}
	
	// Make sure we didn't run out of room in the buffer...if we did, null term
	// and return as much as we have, and return the special code.
	if (curr_input - buffer == buflen && c != EOF && c != '\n' && c != '\r')
	{
		*(buffer + buflen-1) = '\0';
		return(-4);
	}
	
	if (c == EOF)
	{
		if (feof(file))
		{
			return(-1);
		}
		if (ferror(file))
		{
			return(-2);
		}
		else
		{
			return(-3); // Shouldn't be reached
		}
	}
	
	// Normal case, return how much we copied into the buffer.
	return(curr_input - buffer);
}

char *parse_delimiter_string(const char *string)
{
	char *local_buffer = NULL, *source_char = NULL, *dest_char = NULL;
	size_t maxlen = strlen(string) + 1;
	
	CREATE(local_buffer, maxlen * sizeof(char));
	
	// Initial conditions
	source_char = (char *)string;
	dest_char = local_buffer;
	
	for (; *source_char && 
		 (size_t)(dest_char - local_buffer) < maxlen - 1; )
	{
		// Copy over any non codes (even spaces)
		if (*source_char != '%')
		{
			*dest_char++ = *source_char++;
			continue;
		}
		
		// Otherwise, point to our code:
		source_char++;
		
		switch (*source_char) {
			case '%':
			case '\0':
				// Unexpected termination, or double escaped.
				*dest_char++ = '%';
				break;
			case 't':
				*dest_char++ = '\t';
				break;
			case 'r':
				*dest_char++ = '\r';
				break;
			case 'n':
				*dest_char++ = '\n';
				break;
			default:
				// Default, just copy the code over
				*dest_char++ = *source_char;
				break;
		}
		
		// Next!
		source_char++;
	}
	
	// Null terminate.
	*dest_char = '\0';
	
	return local_buffer;
}

// Subtraction doesn't work in our case, because some of our types are
// unsigned.

int init_snap_record(snap_t *snap)
{
	/* Initialize the master array of file_record_t's */
	snap->currentArraySize = 0;
	snap->currentArrayCapacity = INITIAL_ARRAY_SIZE;
	CREATE(snap->master_array, 
		   snap->currentArrayCapacity * sizeof(file_record *));
	
	snap->column_string = strdup("%p %m %c");
	set_snap_field_delimiter(snap, "%t");
	set_snap_record_delimiter(snap, "%n");
	
	return 0;
}

// Set the column string for the snap
int set_snap_column_string(snap_t *snap, char *column_string)
{
	snap->column_string = strdup(column_string);
	return 0;
}

// Set the field delimiter for the snap
int set_snap_field_delimiter(snap_t *snap, char *field_delimiter)
{
	snap->field_delimiter = parse_delimiter_string(field_delimiter);
	return 0;
}

// Set the record delimiter for the snap
int set_snap_record_delimiter(snap_t *snap, char *record_delimiter)
{
	snap->record_delimiter = parse_delimiter_string(record_delimiter);
	return 0;
}

int add_record_to_snap(snap_t *snap, file_record *file)
{
	// Self-growing array.  We keep track of the capacity, and reallocate
	// if we need more space.
	
	// First, check if we need to grow.
	if (snap->currentArraySize >= snap->currentArrayCapacity)
	{		
		RECREATE(snap->master_array, 
				 (snap->currentArrayCapacity + ARRAY_CHUNK_SIZE) * 
				 sizeof(file_record *));
		
		// Keep track of the capacity.
		snap->currentArrayCapacity += ARRAY_CHUNK_SIZE;
	}
	
	snap->master_array[snap->currentArraySize] = file;
	
	// Keep track of our size.
	snap->currentArraySize++;
	
	return 0;
}

// Prints the header string to the provided buffer.
#define MAX_HEADER	256
int hprintbuf(snap_t *snap, char **buf, size_t maxlen)
{
	char *source_char, *dest_char, *save_pos, *buffer_pointer;
	char local_buffer[MAX_HEADER+1];
	
	// Basically, source_char points to our current position in processing the
	// columnString, dest_char points to the insertion point in the provided
	// buffer, and save_pos hold our original position, for quick pointer
	// arithmetic for the new string length.
	
	// We find our indicator ('%') and then print the header name into the
	// buffer.
	
	// Set up our pointers
	source_char = snap->column_string;
	dest_char = save_pos = *buf;
	
	// While *sour_char holds a character, and our new string is less than
	// the provided maxlen:
	for (; *source_char && (size_t)(dest_char - save_pos) < maxlen;)
	{
		// No special code, copy in place and go.
		if (*source_char != '%')
		{
			//Strip out spaces
			if (*source_char == ' ')
			{
				source_char++;
				continue;
			}
			
			*dest_char++ = *source_char++;
			continue;
		}
		
		// Otherwise, we have a column code.
		
		// Look at next character:
		source_char++;
		
		// Otherwise, fill the bufer with the column header name.
		switch (*source_char)
		{
			case 'p':
				snprintf(local_buffer, MAX_HEADER, "%s", "Path");
				break;
			case 'a':
				snprintf(local_buffer, MAX_HEADER, "%s", "Last Accessed");
				break;
			case 'A':
				snprintf(local_buffer, MAX_HEADER, "%s", "atime");
				break;
			case 'm':
				snprintf(local_buffer, MAX_HEADER, "%s", "Last Modified");
				break;
			case 'M':
				snprintf(local_buffer, MAX_HEADER, "%s", "mtime");
				break;
			case 'c':
				snprintf(local_buffer, MAX_HEADER, "%s", "Last Mode Change");
				break;
			case 'C':
				snprintf(local_buffer, MAX_HEADER, "%s", "ctime");
				break;
			case 's':
				snprintf(local_buffer, MAX_HEADER, "%s", "Size");
				break;
			case 'S':
				snprintf(local_buffer, MAX_HEADER, "%s", "Size (raw)");
				break;
			case 'i':
				snprintf(local_buffer, MAX_HEADER, "%s", "inode");
				break;
			case 'o':
				snprintf(local_buffer, MAX_HEADER, "%s", "Owner");
				break;
			case 'g':
				snprintf(local_buffer, MAX_HEADER, "%s", "Group");
				break;
			case 't':
				snprintf(local_buffer, MAX_HEADER, "%s", "Type");
				break;
			case 'T': 
				snprintf(local_buffer, MAX_HEADER, "%s", "Type (raw)");
				break;
			case 'P':
				snprintf(local_buffer, MAX_HEADER, "%s", "Mode");
				break;
			case 'e':
				snprintf(local_buffer, MAX_HEADER, "%s", "Selected");
				break;
			case '%':
			case '\0':
			case ' ':
				break;
			default:
				LogError("Found %%%c\n", *source_char);
				snprintf(local_buffer, MAX_HEADER, "%c", *source_char);
				break;
		}	
		
		buffer_pointer = local_buffer;
		// If we have enough room...
		if ((size_t)(dest_char - save_pos) + strlen(local_buffer) < maxlen)
		{
			for (; *buffer_pointer; )
			{
				// Copy the code over, and increment.
				*dest_char++ = *buffer_pointer++;
			}
		}
		
		// Now, put in the field delimiter:
		snprintf(local_buffer, MAX_HEADER, "%s", snap->field_delimiter);
		
		buffer_pointer = local_buffer;
		if ((size_t)(dest_char - save_pos) + strlen(local_buffer) < maxlen)
		{
			for (; *buffer_pointer; )
			{
				*dest_char++ = *buffer_pointer++;
			}
		}
		
		source_char++;
	}
	
	// Now, put in the record delimiter:
	snprintf(local_buffer, MAX_HEADER, "%s", snap->record_delimiter);
	
	buffer_pointer = local_buffer;
	if ((size_t)(dest_char - save_pos) + strlen(local_buffer) < maxlen)
	{
		for (; *buffer_pointer; )
		{
			*dest_char++ = *buffer_pointer++;
		}
	}
	
	*dest_char = '\0';
	
	return (dest_char - save_pos);
}
#undef MAX_HEADER

int rprintbuf(snap_t *snap, file_record *record, char **buf, size_t maxlen)
{
	char *source_char, *dest_char, *save_pos, *buffer_pointer;
	char local_buffer[PATH_MAX+1];
	long unsigned int comp_octal; // for outputting permissions in octal
	
	// Basically, source_char points to our current position in processing the 
	// columnString, dest_char points to the current insertion point in the
	// provided buffer, and save_pos hold our original position in the buffer,
	// so that we can use pointer arithmetic to very quickly compute the current
	// length of the new string.
	
	// We find our indicator ('%') and then the next character will indicate
	// what we insert into the string.  We put that in a buffer, and then copy
	// it in.
	
	// Set up our pointers.
	source_char = snap->column_string;
	dest_char = save_pos = *buf;
	
	// While *source_char holds a character (isn't NULL), and our new string is
	// less than the provided maxlen:
	for (; *source_char && (size_t)(dest_char - save_pos) < maxlen;)
	{
		// No special parsing, copy it right over and go one..
		if (*source_char != '%')
		{
			//Strip out spaces
			if (*source_char == ' ')
			{
				source_char++;
				continue;
			}
			
			*dest_char++ = *source_char++;
			continue;
		}
		
		// else, we have a column code!
		
		// we look at the next character.
		source_char++;
		
		// Documentation for codes is provided above.
		switch (*source_char) {
			case '%':
			case '\0':
				snprintf(local_buffer, PATH_MAX, "%c", '%');
				break;
			case 'p':
				snprintf(local_buffer, PATH_MAX, "%s", record->re_path);
				break;
			case 'a':
				snprintf(local_buffer, PATH_MAX, "%s", 
						 ctime(&(record->re_atime)));
				break;
			case 'A':
				snprintf(local_buffer, PATH_MAX, "%ld", record->re_atime);
				break;
			case 'm':
				snprintf(local_buffer, PATH_MAX, "%s", 
						 ctime(&(record->re_mtime)));
				break;
			case 'M':
				snprintf(local_buffer, PATH_MAX, "%ld", record->re_mtime);
				break;
			case 'c':
				snprintf(local_buffer, PATH_MAX, "%s", 
						 ctime(&(record->re_ctime)));
				break;
			case 'C':
				snprintf(local_buffer, PATH_MAX, "%ld", record->re_ctime);
				break;
			case 's':
				if (record->re_size < 1024ll)
				{
					snprintf(local_buffer, PATH_MAX, "%d bytes", 
							 (int)record->re_size);
				}
				else if (record->re_size < 1048576ll)
				{
					snprintf(local_buffer, PATH_MAX, "%.1f KB",
							 ((float)record->re_size) / 1024.0f);
				}
				else if (record->re_size < 1073741824ll)
				{
					snprintf(local_buffer, PATH_MAX, "%.2f MB",
							 ((float)record->re_size) / 1048576.0f);
				}
				else if (record->re_size < 1099511627776ll)
				{
					snprintf(local_buffer, PATH_MAX, "%.2f GB",
							 ((float)record->re_size) / 1073741824.0f);
				}
				else
				{
					snprintf(local_buffer, PATH_MAX, "%.3f TB",
							 ((float)record->re_size) / 1099511627776.0f);
				}
				break;
			case 'S':
				snprintf(local_buffer, PATH_MAX, "%lld", 
						 (long long int)record->re_size);
				break;
			case 'i':
				snprintf(local_buffer, PATH_MAX, "%lu", 
						 (long unsigned int)record->re_ino);
				break;
			case 'o':
				snprintf(local_buffer, PATH_MAX, "%lu", 
						 (long unsigned int)record->re_uid);
				break;
			case 'g':
				snprintf(local_buffer, PATH_MAX, "%lu", 
						 (long unsigned int)record->re_gid);
				break;
			case 't':
				snprintf(local_buffer, PATH_MAX, "%c", record->re_type);
				break;
			case 'T': // T (type) and P (permission) are the same int.
				snprintf(local_buffer, PATH_MAX, "%hu", record->re_mode);
				break;
			case 'e': // Special "enabled"
				snprintf(local_buffer, PATH_MAX, "%c", record->re_selected);
				break;
			case 'P': 
				// P now comes out to the octal mode, as described in the
				// chmod manual page
				// Here's what we'll do for each bit/value:
				comp_octal = 0;
#define	ADD_OCTAL_VALUE(mode, bit, comp, value); if (IS_SET(mode, bit)) \
{ \
	comp += value; \
}
				//Special bits:
				ADD_OCTAL_VALUE(record->re_mode, S_ISUID, comp_octal, 4000);
				ADD_OCTAL_VALUE(record->re_mode, S_ISGID, comp_octal, 2000);
				ADD_OCTAL_VALUE(record->re_mode, S_ISVTX, comp_octal, 1000);
				
				//Owner bits:
				ADD_OCTAL_VALUE(record->re_mode, S_IRUSR, comp_octal, 400);
				ADD_OCTAL_VALUE(record->re_mode, S_IWUSR, comp_octal, 200);
				ADD_OCTAL_VALUE(record->re_mode, S_IXUSR, comp_octal, 100);
				
				//Group bits:
				ADD_OCTAL_VALUE(record->re_mode, S_IRGRP, comp_octal, 40);
				ADD_OCTAL_VALUE(record->re_mode, S_IWGRP, comp_octal, 20);
				ADD_OCTAL_VALUE(record->re_mode, S_IXGRP, comp_octal, 10);
				
				//Other bits:
				ADD_OCTAL_VALUE(record->re_mode, S_IROTH, comp_octal, 4);
				ADD_OCTAL_VALUE(record->re_mode, S_IWOTH, comp_octal, 2);
				ADD_OCTAL_VALUE(record->re_mode, S_IXOTH, comp_octal, 1);
				
#undef ADD_OCTAL_VALUE
				snprintf(local_buffer, PATH_MAX, "%.4lu", comp_octal);
				
				break;
				
			default:
				LogError("Found %%%c\n", *source_char);
				snprintf(local_buffer, PATH_MAX, "%c", *source_char);
				break;
		} // End switch (*source_char)
		
		
		buffer_pointer = local_buffer;
		// If we have enough room...
		if ((size_t)(dest_char - save_pos) + strlen(local_buffer) < maxlen)
		{
			for (; *buffer_pointer; )
			{
				if ((*source_char != 'n' && *buffer_pointer == '\n') ||
					(*source_char != 'r' && *buffer_pointer == '\r') ||
					(*source_char != 't' && *buffer_pointer == '\t'))
				{
					// If we have one of these special special chars outside our
					// own, strip it out.
					buffer_pointer++;
				}
				else
				{
					// Copy the code over, and increment.
					*dest_char++ = *buffer_pointer++;
				}
			}
		}
		
		// Now, put in the field delimiter:
		snprintf(local_buffer, PATH_MAX, "%s", snap->field_delimiter);
		
		buffer_pointer = local_buffer;
		if ((size_t)(dest_char - save_pos) + strlen(local_buffer) < maxlen)
		{
			for (; *buffer_pointer; )
			{
				*dest_char++ = *buffer_pointer++;
			}
		}
		
		source_char++;
	} // end for (; *source_char && (size_t)(dest_char - save_pos) < maxlen; )
	
	// Now, put in the record delimiter:
	snprintf(local_buffer, PATH_MAX, "%s", snap->record_delimiter);
	
	// Copy our one pass into the master buffer.
	buffer_pointer = local_buffer;
	if ((size_t)(dest_char - save_pos) + strlen(local_buffer) < maxlen)
	{
		for (; *buffer_pointer; )
		{
			*dest_char++ = *buffer_pointer++;
		}
	}
	
	// Null terminate (we made sure to reserve space above)
	*dest_char = '\0';
	
	return (dest_char - save_pos);
}

int write_snap_record_to_file(snap_t *snap, char *path)
{
	int i;
	FILE *myFile;
	struct file_record_t *current_record;
	char *buffer;
	
	// Create a buffer to hold the current record.
	CREATE(buffer, MAX_RECORD_LENGTH+1);
	
	// If we have a specified outputPath, attempt to open it.
	if (path)
	{
		myFile = fopen(path, "w");
		if (myFile == NULL)
		{
			LogError("Couldn't open %s for output: %s.\n"
					 "Defaulting to stdout.\n", 
					 path, 
					 strerror(errno));
			myFile = stdout;
		}
	}
	else
	{
		myFile = stdout;
	}
	// Now, myFile either points to a specified file, or stdout.  Either way,
	// we're going to write to it.
	
	// Print the header string to the buffer.
	hprintbuf(snap, &buffer, MAX_RECORD_LENGTH);
	
	// Ensure null-termination
	buffer[MAX_RECORD_LENGTH] = '\0';
	
	// Print to file.
	fprintf(myFile, "%s", buffer);
	
	// For all the records in the array:
	for (i = 0; i < snap->currentArraySize; i++)
	{
		current_record = snap->master_array[i];
		
		// Print the current record to our buffer, according to the columnString
		rprintbuf(snap, current_record, &buffer, MAX_RECORD_LENGTH);
		
		// Make sure it's null-terminated (probably already is)
		buffer[MAX_RECORD_LENGTH] = '\0';
		
		// Print it out.
		fprintf(myFile, "%s", buffer);
	}
	
	// If we have an actual file, fclose it (which calls it's on fflush),
	// otherwise, call fflush.  Just to make sure all the output gets written.
	if (myFile != stdout)
	{
		if (fclose(myFile) == EOF)
		{
			LogError("\nCouldn't close %s: %s\n", 
					 path, 
					 strerror(errno));
		}
		
		// Closed our actual file.  Most of the time this is run by root, so
		// let's be nice, and make this world writable.
		struct stat info;
		// Fail silently, since this isn't crucial...
		if (stat(path, &info) != -1)
		{
			mode_t new_mode = getmode(setmode("0666"), info.st_mode);
			chmod(path, new_mode);
		}
		
	}
	else
	{
		if (fflush(myFile) == EOF)
		{
			LogError("\nCouldn't fflush stdout: %s\n", strerror(errno));
		}
	}
	return 0;
}

// Some defines to keep track of column indicies.
#define MAX_COLUMNS		16
#define PATH_COLUMN		0
#define OWNER_COLUMN	1
#define GROUP_COLUMN	2
#define PERMS_COLUMN	3
#define ATIME_COLUMN	4
#define ACCESS_COLUMN	5
#define MTIME_COLUMN	6
#define MODIFY_COLUMN	7
#define CTIME_COLUMN	8
#define CHANGE_COLUMN	9
#define INODE_COLUMN	10
#define TYPE_COLUMN		11
#define MODE_T_COLUMN	12
#define SIZE_COLUMN		13
#define BYTES_COLUMN	14
#define SELECT_COLUMN	15

int read_snap_record_from_file(snap_t *snap, char *path)
{	
	int column_tracker[MAX_COLUMNS];

	column_tracker[PATH_COLUMN] = -1;
	column_tracker[OWNER_COLUMN] = -1;
	column_tracker[GROUP_COLUMN] = -1;
	column_tracker[PERMS_COLUMN] = -1;
	column_tracker[ATIME_COLUMN] = -1;
	column_tracker[MTIME_COLUMN] = -1;
	column_tracker[CTIME_COLUMN] = -1;
	column_tracker[INODE_COLUMN] = -1;
	column_tracker[TYPE_COLUMN] = -1;
	column_tracker[ACCESS_COLUMN] = -1;
	column_tracker[SIZE_COLUMN] = -1;
	column_tracker[MODIFY_COLUMN] = -1;
	column_tracker[CHANGE_COLUMN] = -1;
	column_tracker[BYTES_COLUMN] = -1;
	column_tracker[MODE_T_COLUMN] = -1;
	column_tracker[SELECT_COLUMN] = -1;
	
	FILE *snapper_file = fopen(path, "r");
	if (snapper_file == NULL)
	{
		LogError("Couldn't open snapper file %s: %s\n",
				 path, strerror(errno));
		return(1);
	}

	// Our buffer for getting lines from the snapper file.
	char buffer[PATH_MAX + 128]; // Basically, the longest path and 128 extra
								 // spaces.  This should suffice.


	// Get the header.
	bzero(buffer, PATH_MAX + 64);
	int lines = 0;
	int success = _getline(snapper_file, buffer, PATH_MAX + 64);
	switch (success) {
		case 0:
		case -1:
		case -2:
		case -3:
		case -4:
			LogError("Couldn't get header line in file %s.\n", path);
			return(-1);
		default:
			lines++;
			// Go through the header line, and get each string seperated by a
			// tab.  There should be no spaces to speak of (thanks to snapper)
			// We have lots of things to watch out for, lots of buffers to
			// accidentially walk out of, so we have to be careful.
			
			char header_buf[64]; // Header strings are very small.
			char *curr_pos = buffer, *curr_input = header_buf;
			bzero(header_buf, 64);
			
			// We'll build a derived column string for our snap as we go.
			// Column strings are very small, generally.  Created with
			// calloc, so we shouldn't need to zero.
			char *column_string = NULL;
			CREATE(column_string, 201);
			
			int i = 0;
			while((*curr_pos) &&
				  curr_pos - buffer < success &&
				  curr_input - header_buf < 64)
			{
				if (*curr_pos == '\t')
				{
					*curr_input = '\0';
					
					
					if (strncmp("Path", 
								header_buf, 
								MAX(strlen("Path"), 
									strlen(header_buf))) == 0)
					{
						column_tracker[PATH_COLUMN] = i;
						strncat(column_string, "%p", 200-strlen(column_string));
					}
					else if (strncmp("Owner", 
									 header_buf, 
									 MAX(strlen("Owner"), 
										 strlen(header_buf))) == 0)
					{
						column_tracker[OWNER_COLUMN] = i;
						strncat(column_string, "%o", 200-strlen(column_string));
					}
					else if (strncmp("Selected", 
									 header_buf, 
									 MAX(strlen("Selected"), 
										 strlen(header_buf))) == 0)
					{
						column_tracker[SELECT_COLUMN] = i;
						strncat(column_string, "%e", 200-strlen(column_string));
					}
					else if (strncmp("Group", 
									 header_buf, 
									 MAX(strlen("Group"), 
										 strlen(header_buf))) == 0)
					{
						column_tracker[GROUP_COLUMN] = i;
						strncat(column_string, "%g", 200-strlen(column_string));
					}
					else if (strncmp("Mode", 
									 header_buf, 
									 MAX(strlen("Mode"), 
										 strlen(header_buf))) == 0)
					{
						column_tracker[PERMS_COLUMN] = i;
						strncat(column_string, "%P", 200-strlen(column_string));
					}
					else if (strncmp("Last Accessed", 
									 header_buf, 
									 MAX(strlen("Last Accessed"), 
										 strlen(header_buf))) == 0)
					{
						column_tracker[ACCESS_COLUMN] = i;
						strncat(column_string, "%a", 200-strlen(column_string));
					}
					else if (strncmp("Last Modified", 
									 header_buf, 
									 MAX(strlen("Last Modified"), 
										 strlen(header_buf))) == 0)
					{
						column_tracker[MODIFY_COLUMN] = i;
						strncat(column_string, "%m", 200-strlen(column_string));
					}
					else if (strncmp("Last Mode Change", 
									 header_buf, 
									 MAX(strlen("Last Mode Change"), 
										 strlen(header_buf))) == 0)
					{
						column_tracker[CHANGE_COLUMN] = i;
						strncat(column_string, "%c", 200-strlen(column_string));
					}
					else if (strncmp("Size", 
									 header_buf, 
									 MAX(strlen("Size"), 
										 strlen(header_buf))) == 0)
					{
						column_tracker[SIZE_COLUMN] = i;
						strncat(column_string, "%s", 200-strlen(column_string));
					}
					else if (strncmp("inode", 
									 header_buf, 
									 MAX(strlen("inode"), 
										 strlen(header_buf))) == 0)
					{
						column_tracker[INODE_COLUMN] = i;
						strncat(column_string, "%i", 200-strlen(column_string));
					}
					else if (strncmp("Type", 
									 header_buf, 
									 MAX(strlen("Type"), 
										 strlen(header_buf))) == 0)
					{
						column_tracker[TYPE_COLUMN] = i;
						strncat(column_string, "%t", 200-strlen(column_string));
					}
					else if (strncmp("atime", 
									 header_buf, 
									 MAX(strlen("atime"), 
										 strlen(header_buf))) == 0)
					{
						column_tracker[ATIME_COLUMN] = i;
						strncat(column_string, "%A", 200-strlen(column_string));
					}
					else if (strncmp("ctime", 
									 header_buf, 
									 MAX(strlen("ctime"), 
										 strlen(header_buf))) == 0)
					{
						column_tracker[CTIME_COLUMN] = i;
						strncat(column_string, "%C", 200-strlen(column_string));
					}
					else if (strncmp("mtime", 
									 header_buf, 
									 MAX(strlen("mtime"), 
										 strlen(header_buf))) == 0)
					{
						column_tracker[MTIME_COLUMN] = i;
						strncat(column_string, "%M", 200-strlen(column_string));
					}
					else if (strncmp("Size (raw)", 
									 header_buf, 
									 MAX(strlen("Size (raw)"), 
										 strlen(header_buf))) == 0)
					{
						column_tracker[BYTES_COLUMN] = i;
						strncat(column_string, "%S", 200-strlen(column_string));
					}
					else if (strncmp("Type (raw)", 
									 header_buf, 
									 MAX(strlen("Type (raw)"), 
										 strlen(header_buf))) == 0)
					{
						column_tracker[MODE_T_COLUMN] = i;
						strncat(column_string, "%T", 200-strlen(column_string));
					}
					
					
					i++;  // Increment column counter.
					curr_pos++;  // Get ready for next iteration.
					curr_input = header_buf;
				}
				else
				{
					*curr_input++ = *curr_pos++;
				}
			}
			
			// Store the column string (allocated, will be free()d later)
			if (snap->column_string)
			{
				free(snap->column_string);
			}
			snap->column_string = column_string;
			
			break;
	}
	
	assert(column_tracker[PATH_COLUMN] != -1);

	while ((_getline(snapper_file, buffer, PATH_MAX + 64) >= 0))
	{
		file_record *new_rec;
		CREATE(new_rec, sizeof(file_record));
		
		parse_snapper_file_line(column_tracker, buffer, new_rec);
		add_record_to_snap(snap, new_rec);
	}
	
	/* Sort by path here!  This will enable binary searches */
	
	return 0;
}

int parse_snapper_file_line(int column_tracker[], 
							char *line, 
							file_record *record)
{
	char entry_buf[PATH_MAX+1];
	char *curr_pos = line, *curr_input = entry_buf;
	char *endptr; // for checking strtol validity.
	int i = 0;
	bzero(entry_buf, PATH_MAX+1);
	
	// Zero out (Just in case)
	record->re_path			=	NULL;
	record->re_atime		=	-1;
	record->re_atime_str	=	NULL;
	record->re_mtime		=	-1;
	record->re_mtime_str	=	NULL;
	record->re_ctime		=	-1;
	record->re_ctime_str	=	NULL;
	record->re_size			=	-1;
	record->re_ino			=	-1;
	record->re_uid			=	-1;
	record->re_gid			=	-1;
	record->re_mode			=	-1;
	record->re_type			=	'\0';
	record->re_selected		=	'u';

	while (*curr_pos && curr_input - entry_buf < PATH_MAX)
	{
		if (*curr_pos == '\t')
		{
			*curr_input = '\0'; // Null terminate for strdup.
			
			// If we're on one of the correct columns, store the entry.
			if (i == column_tracker[PATH_COLUMN])
			{
				record->re_path = strdup(entry_buf); // This must be freed.
			}
			else if (i == column_tracker[PERMS_COLUMN])
			{
				record->re_mode = strtol(entry_buf, &endptr, 8);
				
				if (*endptr != '\0')
				{
					LogError("Invalid permissions column.");
					record->re_mode = -1; // (Tag this as invalid).
				}
			}
			else if (i == column_tracker[OWNER_COLUMN])
			{
				record->re_uid = strtol(entry_buf, &endptr, 10);
				
				if (*endptr != '\0')
				{
					LogError("Invalid permissions column.");
					record->re_uid = -1; // (Tag this as invalid).
				}
				
			}
			else if (i == column_tracker[SELECT_COLUMN])
			{
				switch (*entry_buf) {
					case 'y':
					case 'n':
						record->re_selected = *entry_buf;
						break;
					default:
						record->re_selected = 'u';
				}
			}
			else if (i == column_tracker[GROUP_COLUMN])
			{
				record->re_gid = strtol(entry_buf, &endptr, 10);
				
				if (*endptr != '\0')
				{
					LogError("Invalid permissions column.");
					record->re_gid = -1; // (Tag this as invalid).
				}
				
			}
			else if (i == column_tracker[INODE_COLUMN])
			{
				record->re_ino = strtol(entry_buf, &endptr, 10);
				
				if (*endptr != '\0')
				{
					LogError("Invalid permissions column.");
					record->re_ino = -1; // (Tag this as invalid).
				}
			}
			else if (i == column_tracker[ATIME_COLUMN])
			{
				record->re_atime = strtol(entry_buf, &endptr, 10);
				
				if (*endptr != '\0')
				{
					record->re_atime = -1;
				}
			}
			else if (i == column_tracker[ACCESS_COLUMN])
			{
				if (*entry_buf)
				{
					record->re_atime_str = strdup(entry_buf);
				}
			}
			else if (i == column_tracker[MTIME_COLUMN])
			{
				record->re_mtime = strtol(entry_buf, &endptr, 10);
				
				if (*endptr != '\0')
				{
					record->re_mtime = -1;
				}
			}
			else if (i == column_tracker[MODIFY_COLUMN])
			{
				if (*entry_buf)
				{
					record->re_mtime_str = strdup(entry_buf);
				}
			}
			else if (i == column_tracker[CTIME_COLUMN])
			{
				record->re_ctime = strtol(entry_buf, &endptr, 10);
				
				if (*endptr != '\0')
				{
					record->re_ctime = -1;
				}
				
			}
			else if (i == column_tracker[CHANGE_COLUMN])
			{
				if (*entry_buf)
				{
					record->re_ctime_str = strdup(entry_buf);
				}
			}
			else if (i == column_tracker[SIZE_COLUMN])
			{
				record->re_size = strtol(entry_buf, &endptr, 10);
				
				if (*endptr != '\0')
				{
					record->re_size = -1;
				}
				
			}
			else if (i == column_tracker[TYPE_COLUMN])
			{
				// Get the first char in entry_buf
				record->re_type = *entry_buf;
			}

			
			curr_input = entry_buf; // Reset for next entry.
			curr_pos++; // And move on....
			i++;
		}
		else
		{
			*curr_input++ = *curr_pos++; // Simple copy over.
		}
	}	
	
	return 0;
}


int free_snap(snap_t *snap)
{
	
	int i;
	
	for (i = 0; i < snap->currentArraySize; i++)
	{
		if (snap->master_array[i])
		{
			if (snap->master_array[i]->re_path)
			{
				free(snap->master_array[i]->re_path);
			}
			if (snap->master_array[i]->re_atime_str)
			{
				free(snap->master_array[i]->re_atime_str);
			}
			if (snap->master_array[i]->re_mtime_str)
			{
				free(snap->master_array[i]->re_mtime_str);
			}
			if (snap->master_array[i]->re_ctime_str)
			{
				free(snap->master_array[i]->re_ctime_str);
			}
			free(snap->master_array[i]);
		}
	}
	
	free(snap->master_array);
	
	snap->master_array = NULL;
	
	free(snap->column_string);
	free(snap->field_delimiter);
	free(snap->record_delimiter);
	
	return 0;
}
