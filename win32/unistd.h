/* stub unistd.h for use for MSVC compilers */
#pragma once
#ifndef UNISTD_H
#define UNISTD_H

#include <io.h>

#ifdef _WIN64
#define ssize_t __int64
#else
#define ssize_t long
#endif

#define R_OK    4       /* Test for read permission.  */
#define W_OK    2       /* Test for write permission.  */
#define X_OK    1       /* execute permission - unsupported in Windows, using it will crash */
#define F_OK    0       /* Test for existence.  */

#endif // UNISTD_H