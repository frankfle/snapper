//
// configfile.h
//
// Simple configuration file implementation.
//
// Created by Frank Fleschner on 11/17/2008.
// Copyright 2008 ACS. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "configfile.h"

#define MAX(x, y) ((x > y) ? x : y)
#define MIN(x, y) ((x > y) ? y : x)

#define CREATE(var, bytes) do {\
	if ((bytes) > 0)\
	{\
		var = calloc((bytes), 1);\
		if (var == NULL)\
		{\
			fprintf(stderr, "Error while allocating memory for %s: %s\n", \
					#var, strerror(errno));\
			exit(1);\
		}\
	}\
	else\
	{\
		fprintf(stderr, "Requested %d bytes memory for %s\n", \
				(int) (bytes), #var);\
	}\
} while(0)

#define RECREATE(var, bytes) do {\
	if ((bytes) > 0)\
	{\
		var = realloc(var, (bytes));\
		if (var == NULL)\
		{\
			fprintf(stderr, "Error while growing %s: %s\n", \
					#var, strerror(errno));\
			exit(1);\
		}\
	}\
	else\
	{\
		fprintf(stderr, "Requested to grow %s to %d\n", #var, (int) (bytes));\
	}\
} while(0)

#pragma mark Local Prototypes (private functions)
int add_key_value_record_to_config_file(struct key_value_record key_record,
										config_file_t *file_record);
										
#pragma mark Function Implementations
void debug_print_config_file_t(config_file_t *record)
{
	
	// Print out the values in our record
	fprintf(stderr, "DEBUG: config_file_t 0x%lx\n", (unsigned long) record);
	fprintf(stderr, "DEBUG: path = %s\n", record->path);
	fprintf(stderr, "DEBUG: read = %d\n", record->read);
	fprintf(stderr, "DEBUG: valid = %d\n", record->valid);
	
	fprintf(stderr, "DEBUG: array_size = %d\n", record->array_size);
	fprintf(stderr, "DEBUG: array_capacity = %d\n", record->array_capacity);

	fprintf(stderr, "DEBUG: keys and values:\n");
	int i;
	
	// Print the keys and values in the array
	for (i = 0; i < record->array_size; i++)
	{
		fprintf(stderr, "DEBUG: 	(%d) %s = %s\n", i+1,
				record->key_value_array[i].key,
				record->key_value_array[i].value);
	}
}
// Init a config file.  Takes a config_file_t.  Does not allocate.
int init_config_file(config_file_t *new_record)
{
	// Must be given a record to init.
	assert(new_record);
	
	// Default values.
	new_record->path				= NULL;	
	new_record->read				= 0;
	new_record->valid				= 1;
	new_record->key_value_array		= 0;
	new_record->array_size			= 0;
	new_record->array_capacity		= 30;
	
	// Creat initial array
	CREATE(new_record->key_value_array, sizeof(struct key_value_record) * 
		   new_record->array_capacity);
	
	// Always return success -- CREATE will bail out in case of memory troubles.
	return 0;
}

// Get the value for a key.  Allocates memory as necessary, so value will need
// to be freed.
int value_for_key(config_file_t *record,
				  const char *key, 
				  char **value, 
				  size_t *size_of_value)
{
	int i;
	
	// Error checking
	assert(key);
	assert(record);

	// Go through our array.
	for (i = 0; i < record->array_size; i++)
	{
		// If we find a match
		if (!strncmp(key, record->key_value_array[i].key, 
					 MAX(strlen(key), strlen(record->key_value_array[i].key))))
		{
			// Set the by-reference value.
			*value = strdup(record->key_value_array[i].value);
			
			// If caller is interested in the size, this will be non-null, so
			// we set it also.
			if (size_of_value)
				*size_of_value = strlen(record->key_value_array[i].value);
			
			// Find one then get out.
			return 0;
		}
	}
			
	// Return failure if not found.
	return -1;
}

// Get an array of values for a key.  Allocates an array as necessary, so will
// need to be freed.
int array_of_values_for_key(config_file_t *record,
							const char *key, 
							char ***array, 
							size_t *size_of_array)
{
	int retval = -1;
	int i;
	size_t current_size = 0;
	char **theArray = NULL;
	
	// Error checking.
	assert(record);
	assert(array);
	assert(key);
	assert(size_of_array);
	assert(record->valid == 1);
	assert(record->read == 1);
	
	// Walk through our array, if we find a key match, then grow our array, and
	// add it.
	for (i = 0; i < record->array_size; i++)
	{
		if (!strncmp(key, record->key_value_array[i].key, 
					 MAX(strlen(key), strlen(record->key_value_array[i].key))))
		{
			if (current_size == 0)
			{
				++current_size;
				CREATE(theArray, sizeof(char *) * current_size);
			}
			else
			{
				++current_size;
				RECREATE(theArray, sizeof(char *) * current_size);
			}
			theArray[current_size-1] = strdup(record->key_value_array[i].value);
			retval = 0;
		}
	}
	
	// Set the value of our by-reference returns
	*size_of_array = current_size;
	*array = theArray;
	
	return retval;
}

// Invalidates the record and frees memory in the key_value_array.
void done_with_config_file(config_file_t *record)
{
	// Error checking
	assert(record);
	
	// Free allocated memory
	int i;
	for (i = 0; i < record->array_size; i++)
	{
		if (record->key_value_array[i].key)
			free(record->key_value_array[i].key);
		
		if (record->key_value_array[i].value)
			free(record->key_value_array[i].value);
	}
	
	// Free memory used.
	free(record->key_value_array);
	free(record->path);
	
	// Invalidate our record
	record->valid = 0;
	record->array_size = 0;
	record->array_capacity = 0;
}

int add_key_value_record_to_config_file(const struct key_value_record key_rec,
										config_file_t *file_record)
{
	// Error checking
	assert(file_record);
	assert(file_record->read == 0);
	assert(file_record->valid == 1);
	
	// If we are out of array space, grow the array.
	if (!(file_record->array_size < file_record->array_capacity))
	{
		RECREATE(file_record->key_value_array, file_record->array_capacity +
				 sizeof(struct key_value_record) * 10);
		file_record->array_capacity += 10;
	}
	
	// Store the key and the value
	file_record->key_value_array[file_record->array_size].key = 
		strdup(key_rec.key);
	file_record->key_value_array[file_record->array_size].value = 
		strdup(key_rec.value);
	
	// Increment the array size
	file_record->array_size++;
	
	return 0;
}

// Parse a single line, and add it to the record.
int parse_config_line(const char *line, config_file_t *record)
{
	char key[2048], value[2048];
	int foundDelimiter = 0;
	char *current_input, *pos;
	size_t line_length = strlen(line);
	struct key_value_record new_record;

	// Error checking
	assert(record);
	
	// Set up initial conditions.
	pos = (char *) line;
	current_input = key;
	
	//TODO: possible to walk past bounds.
	while (*pos != '\0' && (pos - line) < line_length)
	{
		// Copy The key/value to the current input.
		if (*pos != '=')
		{
			*current_input++ = *pos++;
			continue;
		}
		
		// We have found the delimiter
		
		// If we've already found the delimiter, then error out.
		if (foundDelimiter != 0)
		{
			fprintf(stderr, "Error: found more than one delimiter ="
							" in line %s\n",
					line);
			return -1;
		}
		
		// Note that we've found delimiter.
		foundDelimiter = 1;
		
		// Null terminate.
		*current_input = '\0';
		
		// Switch to copying to other buffer.
		current_input = value;
		
		// Skip over delmiter.
		pos++;
	}
	
	// Null terminate.
	*current_input = '\0';

	// Trim whitespace from beginning and ending of values:
	trim(key, strlen(key));
	trim(value, strlen(value));
	
	// Configure the new record
	new_record.key = key;
	new_record.value = value;
	
	// Add the record to our config record.
	add_key_value_record_to_config_file(new_record, record);
	
	return 0;
}

// Read the config file into the record.
int read_config_file(const char *path,
					 config_file_t *record)
{
	FILE *config_file = NULL;
	char buffer[2048]; // This should be large enough.
	char *curr_input;
	char c;
	
	// Error checking.
	assert(path);
	
	if (record->valid != 1)
	{
		fprintf(stderr, "configfile: reading invalid file record.\n");
		return -1;
	}	

	// Open config file
	config_file = fopen(path, "r");
	if (config_file == NULL)
	{
		fprintf(stderr, "Couldn't open config file %s: %s\n", 
				path, strerror(errno));
		return -1;
	}
	
	// Remember the path
	record->path = strdup(path);
	
	// Our main loop, goes through the contents of the file until it gets an EOF
	// or until we've already 1024 characters (currently)
	curr_input = buffer;
	while ((c = fgetc(config_file)) != EOF && 
		   (curr_input - buffer < 1024))
	{
		// If it's not a newline or a carriage return, copy it into our buffer,
		// and move to the next.
		if (c != '\n' && c != '\r')
		{
			*curr_input++ = c;
			continue;
		}
		
		// We're at a newline/comment  Proccess the line.
		
		// Null terminate:
		*curr_input = '\0';
		
		// Trim the white space off each ends.
		trim(buffer, strlen(buffer));
		
		// If we start with a #, then it's a comment, and ignore the whole thing
		// Also, if it's an empty line (just a newline), then ignore it, also.
		if (buffer[0] != '#' && buffer[0] != '\0')
		{
			parse_config_line(buffer, record);
		}
		
		// Reset the current input to the begining of our buffer
		curr_input = buffer;
	}
	
	// If we get an error, bail out.
	if (ferror(config_file) != 0)
	{
		fprintf(stderr, "Error while reading config file %sn", 
				path);
		return -1;
	}
	
	// We now have an EOF without an error.
	
	// We got an EOF in the middle of a line...
	if (curr_input != buffer)
	{
		fprintf(stderr, "Warning: The config file %s should end with a blank "
						"line, like ALL ASCII files.\n", record->path);
		*curr_input = '\0';
		if (buffer[0] != '#')
		{
			parse_config_line(buffer, record);
		}
	}

	// Mark the record as read.
	record->read = 1;
	
	return 0;
}

// Simple function that strips out lead in and lead out whitespace from a
// string.  Returns -1 on failure, 0 on success.
int trim(char *string, size_t length)
{
	size_t local_length;
	char *p = string;
	int i, found;
	
	// Some error checking.
	assert(string);
	
	// Are we done already?
	if (length < 1)
		return 0;
	
	// Trim space at the beginining:
	for (i = 0; i < length; i++)
	{
		if (!(*(string+i) == ' '))
		{
			break;
		}
	}
	found = i;
	local_length = length - found;
	if (found > 0)
	{
		p = string;
		for (i = 0; i < local_length; i++)
		{
			*p = *(p+found);
			p++;
		}
		*p = '\0';
	}
	
	// Trim space at the end:
	p = string+local_length-1;
	for (i = local_length - 1; i > 0; i--)
	{
		if (!(*p == ' '))
		{
			break;
		}
		p--;
	}
	found = (local_length - 1) - i;
	local_length -= found; // Never used again....
	if (found > 0)
	{
		*(p+1) = '\0';
	}
	
	// No current way to fail.
	return 0;
}
