#ifndef INTERNAL_POSIX_H
#define INTERNAL_POSIX_H

#include <stdint.h>

/*
 * Contains various UNIX/POSIX definitions and types
 */

typedef int64_t ssize_t;
typedef int64_t off_t;

typedef uint64_t dev_t;
typedef uint64_t ino_t;
typedef int32_t mode_t;
typedef int32_t nlink_t;
typedef int64_t blksize_t;
typedef int64_t blkcnt_t;

typedef int32_t pid_t;
typedef int32_t tid_t;
typedef int32_t uid_t;
typedef int32_t gid_t;

typedef int64_t time_t;
typedef int64_t clockid_t;

struct timespec
{
  time_t tv_sec;
  long tv_nsec;
};


#endif // INTERNAL_POSIX_H
