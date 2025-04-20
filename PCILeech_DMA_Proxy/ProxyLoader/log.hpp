#pragma once
#ifndef MG_LOG
#define MG_LOG
#include <stdio.h>

#define RESET_COLOR 	"\x1B[0m"
#define INFO_COLOR 	"\x1B[1;34m"
#define DEBUG_COLOR 	"\x1B[1;30m"
#define WARNING_COLOR 	"\x1B[1;33m"
#define ERROR_COLOR 	"\x1B[31m"

template <typename ...Args>
static constexpr void log(const char* type, const char* color, const char* s, Args ...a) {
    printf("%s[", color);
    printf("%s]%s ", type, RESET_COLOR);
    printf(s, a...);
    fflush(stdout);
}
template <typename ...Args> static constexpr void error(const char* s, Args ...a) { log("ERROR", ERROR_COLOR, s, a...); if (GetLastError() > 0) log("ERROR", ERROR_COLOR, "%d\n", GetLastError()); }
template <typename ...Args> static constexpr void warning(const char* s, Args ...a) { log("WARNING", WARNING_COLOR, s, a...); }
template <typename ...Args> static constexpr void info(const char* s, Args ...a) { log("INFO", INFO_COLOR, s, a...); }
template <typename ...Args> static constexpr void debug(const char* s, Args ...a) { log("DEBUG", DEBUG_COLOR, s, a...); }


template <typename ...Args> static constexpr void exit(int status, const char* s, Args ...a) {
    error(s, a...);
    //getchar();
    ExitProcess(status);
}

#endif // !MG_LOG
