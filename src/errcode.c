#include "errcode.h"

const char* errcode_to_string_fallback(errcode_t err) {
    switch (err) {
        case ERR_NONE: return "ERR_NONE";
        case ERR_SLEEP_FAILED: return "ERR_SLEEP_FAILED";
        case ERR_TICK_TIMEOUT: return "ERR_TICK_TIMEOUT";
        case ERR_TIME_FUNC: return "ERR_TIME_FUNC";
        case ERR_INVALID_ARGUMENT: return "ERR_INVALID_ARGUMENT";
        case ERR_UNKNOWN: return "ERR_UNKNOWN";
        case ERR_NO_SUPPORTED_OPCODE: return "ERR_NO_SUPPORTED_OPCODE";
        case ERR_FILE_NOT_FOUND: return "ERR_FILE_NOT_FOUND";
        case ERR_ROM_TOO_LARGE: return "ERR_ROM_TOO_LARGE";
        case ERR_THREAD_CREATION_FAILED: return "ERR_THREAD_CREATION_FAILED";
        case ERR_FILE_READ_FAILED: return "ERR_FILE_READ_FAILED";
        case ERR_PATH_TOO_LONG: return "ERR_PATH_TOO_LONG";
        case ERR_ROM_INVALID: return "ERR_ROM_INVALID";
        case ERR_FILE_CLOSE_FAILED: return "ERR_FILE_CLOSE_FAILED";
        case ERR_FILE_OPEN_FAILED: return "ERR_FILE_OPEN_FAILED";
        default: return "Unknown error code";
    }
}
