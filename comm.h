/*
 *  comm.h
 *  snapper
 *
 *  Created by Frank Fleschner on 5/22/09.
 *  Copyright 2009 ACS. All rights reserved.
 *
 */

#define LogV(...)	_LogV(globals->verbose, __VA_ARGS__)
#define LogMV(...)	_LogMV(globals->megaVerbose, __VA_ARGS__)

// Logs errors to stderr.
void LogError(const char *format, ...) 
__attribute__((format(printf, 1, 2)));

// For verbose (-v) output, goes to stderr.
void _LogV(char enable, const char *format, ...) 
__attribute__((format(printf, 2, 3)));

// For mega-verbose (-V) output, goes to stderr.
void _LogMV(char enable, const char *format, ...) 
__attribute__((format(printf, 2, 3)));
