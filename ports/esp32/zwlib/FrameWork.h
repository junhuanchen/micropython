#ifndef FRAMEWORK_HPP
#define FRAMEWORK_HPP

#define _CRT_SECURE_NO_WARNINGS

#ifdef INFO_OUT

#ifdef _MFC_
extern void _cdecl TrayPrintf(const char * _Format, ...);
#define PutInfo(format, ...) TrayPrintf(format, ##__VA_ARGS__)
#define PutError(error) TrayPrintf("%s, ´íÎó´úÂë : %d¡£\n", error, GetLastError())
#else
#include <stdio.h>
#define PutInfo(format, ...) printf(format, ##__VA_ARGS__)
#define LogError(format, ...) printf(format, ##__VA_ARGS__)
#define PutError(error) printf("%s, ´íÎó´úÂë : %lu¡£\n", error, GetLastError())
#endif
#else
#define PutInfo(format, ...)
#define PutError(error)
#endif

#ifdef LOG_OUT

static bool LogInit( )
{
    return true;
}

#define LogPut(format, ...) fprintf(stdout, format, ##__VA_ARGS__)
#define LogExec(level, func) do { if (level) { ( func ); } } while (0)

#else

#define LogPut(format, ...)

#define LogExec(level, func) 

#endif

#endif // FRAMEWORK_HPP

