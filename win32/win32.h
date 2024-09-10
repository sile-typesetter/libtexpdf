#pragma once
#define localtime_r(timep, result) localtime_s(result, timep)
#define gmtime_r(timep, result) gmtime_s(result, timep)
#define strncasecmp _strnicmp
#if defined(_MSC_VER)
#define off64_t int64_t
#define ftello _ftelli64
#define fseeko _fseeki64
#define stat _stat
#endif
