#pragma once
#ifdef WIN32
#define localtime_r(timep, result) localtime_s(result, timep)
#define gmtime_r(timep, result) gmtime_s(result, timep)
#define strncasecmp _strnicmp
#define stat _stat
#endif