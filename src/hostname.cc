#ifdef _WIN32
//#include <windows.h>
//#include <winbase.h>
#else
#include <unistd.h>
#include <cstring>
#include <limits.h>
#include <errno.h>
#endif

#include "debug.hh"
#include "hostname.hh"

#define NAME_MAXLEN 512

std::string kvz_rtp::hostname::get_hostname()
{
#ifdef _WIN32
    char buffer[NAME_MAXLEN];
    DWORD bufCharCount = NAME_MAXLEN;

    if (!GetComputerName((TCHAR *)buffer, &bufCharCount)) {
        win_get_last_error();
    }

    return std::string(buffer);
#else
    char hostname[NAME_MAXLEN];

    if (gethostname(hostname, NAME_MAXLEN) != 0) {
        LOG_ERROR("%s", strerror(errno));
        return "";
    }

    return std::string(hostname);
#endif
}

std::string kvz_rtp::hostname::get_username()
{
#ifdef _WIN32
    char buffer[NAME_MAXLEN];
    DWORD bufCharCount = NAME_MAXLEN;

    if (!GetUserName((TCHAR *)buffer, &bufCharCount)) {
        win_get_last_error();
        return "";
    }

    return std::string(buffer);
#else
    char username[NAME_MAXLEN];

    if (getlogin_r(username, NAME_MAXLEN) != 0) {
        LOG_ERROR("%s", strerror(errno));
        return "";
    }

    return std::string(username);
#endif
}
