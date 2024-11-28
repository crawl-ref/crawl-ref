/**
 * @file
 * @brief Wrappers for sys/libc calls, mostly for charset purposes.
**/

#pragma once

#include <sys/types.h>

bool lock_file(int fd, bool write, bool wait = false);
bool unlock_file(int fd);

bool read_urandom(char *buf, int len);

#ifdef TARGET_OS_WINDOWS
# ifndef UNIX
void alarm(unsigned int seconds);
#  ifndef CRAWL_HAVE_MKSTEMP
int mkstemp(char *dummy);
#  endif
# endif
#endif

#ifdef __ANDROID__
bool jni_keyboard_control(bool toggle);
#endif

#ifndef CRAWL_HAVE_FDATASYNC
int fdatasync(int fd);
#endif

#ifdef NEED_USLEEP
void usleep(unsigned long time);
#endif

int rename_u(const char *oldpath, const char *newpath);
int unlink_u(const char *pathname);
FILE *fopen_u(const char *path, const char *mode);
int mkdir_u(const char *pathname, mode_t mode);
int open_u(const char *pathname, int flags, mode_t mode);
