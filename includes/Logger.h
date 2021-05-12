/* 
 * File:   Logger.h
 * Author: Dmitry Shurbin
 *
 * Created on May 12, 2021, 5:39 PM
 * This is the simple logger definition
 */

#ifndef LOGGER_H
#define LOGGER_H

#define MAX_LOG_BUFFER_LENGTH   4096
#define LOGGER_INFO                1
#define LOGGER_WARNING             2
#define LOGGER_DEBUG               4
#define LOGGER_ERROR               8

#define LOGGER_DEVICE_NULL         0
#define LOGGER_DEVICE_CONSOLE      1
#define LOGGER_DEVICE_SYSLOG       2

#include <stdio.h>
#include <sys/cdefs.h>
#include <stdarg.h>
#include <syslog.h>
#include <iostream>
#include <string>

extern int loggerLevel;
extern int loggerDevice;
extern int logMessage(int level, const char* fmt, ...);

#endif /* LOGGER_H */

