//
// configfile.h
//
// Simple configuration file implementation.
//
// Created by Frank Fleschner on 11/17/2008.
// Copyright 2008 ACS. All rights reserved.
//

struct key_value_record {
	char* key;
	char* value;
};

struct config_file_record {
	char *path;				// String that holds the path to the file.
	
	char read;				// So we know if we've read it or not.
	char valid;				// invalidate it if it hasn't been inited, or if we
							// are done with it.
	
	// Our array of keys and values.  Also ints to keep track of growing.
	struct key_value_record *key_value_array;
	unsigned int array_size;
	unsigned int array_capacity;
};

typedef struct config_file_record config_file_t;

// DEBUG: Printing
void debug_print_config_file_t(config_file_t *record);

// Init a config file.  Takes a config_file_t.  Does not allocate.
int init_config_file(config_file_t *new_record);

// Read the config file into the record.
int read_config_file(const char *path,
					 config_file_t *record);

// Get the value for a key.  Allocates memory as necessary, so value will need
// to be freed.
int value_for_key(config_file_t *record,
				  const char *key, 
				  char **value, 
				  size_t *size_of_value);

// Get an array of values for a key.  Allocates an array as necessary, so will
// need to be freed.
int array_of_values_for_key(config_file_t *record,
							const char *key, 
							char ***array, 
							size_t *size_of_array);

// Invalidates the record and frees memory in the key_value_array.
void done_with_config_file(config_file_t *record);

// Simple function that strips out lead in and lead out whitespace from a
// string.  Returns -1 on failure, 0 on success.
int trim(char *string, size_t length);
