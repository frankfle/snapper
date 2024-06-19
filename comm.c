/*
 *  comm.c
 *  snapper
 *
 *  Created by Frank Fleschner on 5/22/09.
 *  Copyright 2009 ACS. All rights reserved.
 *
 */

#include <stdarg.h>
#include <stdio.h>

#include "comm.h"

// Logs errors to stderr.
void LogError(const char *format, ...)
{
	va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}

// For verbose output, goes to stderr.
void _LogV(char enable, const char *format, ...) 
{
	if (!enable) {
        return;
    }
    
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}

// For mega-verbose output, goes to stderr.
void _LogMV(char enable, const char *format, ...) 
{
	if (!enable) {
        return;
    }
    
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}
