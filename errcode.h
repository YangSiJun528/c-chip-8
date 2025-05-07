#ifndef ERRCODE_H
#define ERRCODE_H

typedef enum {
    ERR_NONE = 0,
    ERR_SLEEP_FAILED,
    ERR_TICK_TIMEOUT,
    ERR_TIME_FUNC,
    ERR_INVALID_PARAMETER,
    ERR_UNKNOWN,
} errcode_t;

#endif // ERRCODE_H
