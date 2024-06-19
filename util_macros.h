/*
 *  util_macros.h
 *  snapper
 *
 *  Created by Frank Fleschner on 5/22/09.
 *  Copyright 2009 ACS. All rights reserved.
 *
 */

#pragma mark Boolean niceness
typedef char Boolean;
#define true	1
#define false	0
#define YES		1
#define NO		0

#define MAX(x, y)	((x > y) ? x : y)
#define MIN(x, y)	((x > y) ? y : x)

#define CREATE(var, bytes)	do {\
	if ((bytes) > 0)\
	{\
		var = calloc((bytes), 1);\
		if (var == NULL)\
		{\
			LogError("Error while allocating memory for %s: %s\n", \
			#var, strerror(errno));\
			exit(1);\
		}\
	}\
	else\
	{\
		LogError("Requested %d bytes memory for %s\n", (int)(bytes), #var);\
	}\
} while(0)

#define RECREATE(var, bytes) do {\
	if ((bytes) > 0)\
	{\
		var = realloc(var, (bytes));\
		if (var == NULL)\
			{\
			LogError("Error while growing %s: %s\n", \
			#var, strerror(errno));\
			exit(1);\
		}\
	}\
	else\
	{\
		LogError("Requested to grow %s to %d\n", #var, (int)(bytes));\
	}\
} while(0)

#define IS_SET(flag, bit)	((flag) & (bit))
