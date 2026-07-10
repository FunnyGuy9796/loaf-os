#ifndef SYSCALL_USER_H
#define SYSCALL_USER_H

#include <stdint.h>

struct tm {
    uint8_t tm_sec;
    uint8_t tm_min;
    uint8_t tm_hour;
    uint8_t tm_mday;
    uint8_t tm_mon;
    uint16_t tm_year;
};

#define SYS_EXIT 1
#define SYS_YIELD_HLT 2
#define SYS_PRINT 3
#define SYS_DATETIME 4
#define SYS_SBRK 5
#define SYS_OPEN 6
#define SYS_CLOSE 7
#define SYS_READ 8
#define SYS_WRITE 9
#define SYS_STAT 10
#define SYS_UNLINK 11
#define SYS_MKDIR 12
#define SYS_RMDIR 13
#define SYS_OPENDIR 14
#define SYS_READDIR 15
#define SYS_CLOSEDIR 16
#define SYS_SPAWN 17
#define SYS_KILL 18
#define SYS_WAIT 19
#define SYS_SLEEP 20
#define SYS_GETCHAR 21
#define SYS_GETCHAR_NB 22
#define SYS_GETUPTIME 23
#define SYS_PS 24
#define SYS_CHDIR 25
#define SYS_GETCWD 26

#endif