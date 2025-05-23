#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>


#include "log.h"
#include "errcode.h"
#include "chip8_struct.h"
#include "global_config.h"
#include "input.h"
#include "instruction.h"
#include "output.h"
#include "terminal_io.h"

/* 전역 상태 변수 */
static struct {
    bool quit; // 종료 플래그
    errcode_t error_code; // 종료 시 에러 코드
} g_state = {
    .quit = false,
    .error_code = ERR_NONE
};

static chip8_t chip8;

/* 함수 선언 */
errcode_t cycle(void);
static uint64_t get_current_time_ns(errcode_t *errcode);
void update_timers(uint64_t tick_interval);

/* 에러 처리 및 종료 매크로 */
#define SET_ERROR_AND_EXIT_CYCLE(err_code) do { \
g_state.error_code = (err_code); \
g_state.quit = true; \
goto exit_cycle_label; \
} while(0)


static errcode_t initialize_logging(FILE** log_file_ptr, const char* log_filename_suffix);
static void initialize_chip8_core(chip8_t* ch8);
static errcode_t load_rom_to_chip8_memory(chip8_t* ch8, const char* rom_filename);
static errcode_t initialize_platform_modules(chip8_t* ch8, bool* quit_flag);
static void shutdown_platform_modules(void); // terminal_io_shutdown()이 있다고 가정

int main(void) {
    errcode_t err = ERR_NONE;
    FILE *logfile = NULL;

    // 1. 로깅 초기화
    err = initialize_logging(&logfile, "mylog.txt");
    if (err != ERR_NONE) {
        // 로깅 파일 생성 실패 시 stderr로 출력
        fprintf(stderr, "Fatal: Failed to initialize logging: %d\n", err);
        goto cleanup; // logfile은 아직 NULL이므로 fclose 호출 안됨
    }
    log_info("Program started. Logging initialized.");

    // 2. 랜덤 시드 설정
    srand(time(NULL));
    log_debug("Random seed set.");

    // 3. Chip-8 코어 상태 초기화
    initialize_chip8_core(&chip8);
    log_info("Chip-8 core initialized.");

    // 4. ROM 로드
    err = load_rom_to_chip8_memory(&chip8, "Pong (1 player).ch8");
    if (err != ERR_NONE) {
        log_error("Failed to load ROM: %d", err);
        goto cleanup;
    }
    log_info("ROM loaded into Chip-8 memory.");

    // 5. 플랫폼별 모듈(입력, 터미널 I/O) 초기화
    err = initialize_platform_modules(&chip8, &g_state.quit);
    if (err != ERR_NONE) {
        log_error("Failed to initialize platform modules: %d", err);
        goto cleanup;
    }
    log_info("Platform modules (input, terminal I/O) initialized.");

    // 6. 에뮬레이션 루프를 위한 전역 상태 초기화
    g_state.quit = false;
    g_state.error_code = ERR_NONE;
    log_debug("Global state reset for emulation cycle.");

    // 7. 메인 에뮬레이션 사이클 실행
    log_info("Starting emulation cycle...");
    err = cycle();
    if (err != ERR_NONE) {
        log_error("Emulation cycle terminated with error: %d", err);
    } else {
        log_info("Emulation cycle completed.");
    }

cleanup:
    log_info("Starting shutdown sequence...");

    // 플랫폼 모듈 해제 (입력, 터미널 등)
    // initialize_platform_modules가 성공했을 경우에만 호출되어야 하나,
    // shutdown 함수들은 내부적으로 초기화 여부를 확인하거나, 여러 번 호출해도 안전해야 함.
    // 현재 구조에서는 initialize_platform_modules 실패 시에도 호출될 수 있으므로,
    // shutdown_platform_modules 내부에서 이를 안전하게 처리한다고 가정.
    // 또는, 더 세분화된 goto 레이블을 사용할 수 있음 (예: cleanup_platform, cleanup_chip8 등)

    // 순차대로 해제하거나 그런건데, 우리는 chip8 빼고 전역적으로 의존하는 그런건 없음.
    // chip8은 전역이니까 shutdown 구분이 중요하진 않음.

    shutdown_platform_modules();
    log_info("Platform modules shut down.");

    // Chip-8 관련 리소스 해제 (주로 메모리 해제지만, 여기서는 전역 변수라 별도 해제 불필요)
    // 동적 할당된 부분이 있다면 여기서 해제

    if (logfile) {
        log_info("Program exited with code: %d.", err);
        if (fclose(logfile) == EOF) {
            perror("Failed to close log file");
        }
    } else if (err != ERR_NONE) {
        // 로깅 초기화도 실패한 경우
        fprintf(stderr, "Program exited with error code: %d (logging was not available).\n", err);
    }

    return (err == ERR_NONE) ? EXIT_SUCCESS : err;
}

errcode_t cycle(void) {
    const uint64_t tick_interval = TICK_INTERVAL_NS;
    uint64_t max_cycle_ns = 0;
    uint32_t cycle_count = 0;
    uint32_t skip_count = 0;
    errcode_t err_local = ERR_NONE; 

    // 첫 tick 시간 설정
    uint64_t next_tick = get_current_time_ns(&err_local);
    if (err_local != ERR_NONE) {
        SET_ERROR_AND_EXIT_CYCLE(err_local);
    }

    while (!g_state.quit) {
        // 각 사이클 시작 시간 측정
        const uint64_t cycle_start = get_current_time_ns(&err_local);
        if (err_local != ERR_NONE) {
            SET_ERROR_AND_EXIT_CYCLE(err_local);
        }

        // 작업 처리
        err_local = execute_instruction(&chip8);
        if (err_local != ERR_NONE) {
            SET_ERROR_AND_EXIT_CYCLE(err_local);
        }

        // 사이클 종료 시간 측정
        const uint64_t cycle_end = get_current_time_ns(&err_local);
        if (err_local != ERR_NONE) {
            SET_ERROR_AND_EXIT_CYCLE(err_local);
        }

        const uint64_t cycle_time_ns = cycle_end - cycle_start;

        if (cycle_time_ns > max_cycle_ns) {
            max_cycle_ns = cycle_time_ns;
            log_info("Max cycle time: %llu ns", max_cycle_ns);
        }

        if (cycle_time_ns > tick_interval) {
            // 틱 간격보다 사이클 수행 시간이 더 긴 경우
            // 내부 작업은 시스템 콜을 포함하지 않으므로 이런 딜레이가 생기면 안되므로 바로 실패
            log_error("Frame overrun: %llu ns > %llu ns",
                      cycle_time_ns, tick_interval);
            SET_ERROR_AND_EXIT_CYCLE(ERR_TICK_TIMEOUT);
        }

        // 다음 틱 구하기 - 이거 여기 있는게 맞나
        update_timers(tick_interval);

        // 다음 tick 계산
        next_tick += tick_interval;

        // 현재 시간 확인
        uint64_t now = get_current_time_ns(&err_local);
        if (err_local != ERR_NONE) {
            SET_ERROR_AND_EXIT_CYCLE(err_local);
        }

        // 누락된 틱 처리
        if (now >= next_tick) {
            // 누락된 틱 카운트 추가
            uint64_t error_ns = now - next_tick;
            uint32_t missed = (uint32_t) (error_ns / tick_interval) + 1;
            skip_count += missed;
            log_error("Missed %u ticks (error: %llu ns). Total skips: %u",
                      missed, error_ns, skip_count);

            // 오차 누적 방지: next_tick 보정
            next_tick += missed * tick_interval;

            // 스킵된 사이클은 처리하지 않고 다음 사이클로
            // TODO: 이거 뺴는게 맞을듯? 보정만 하고 성공 처리는 하긴 해야지
            continue;
        }

        // 다음 틱 시간까지 busy-wait
        do {
            now = get_current_time_ns(&err_local);
            if (err_local != ERR_NONE) {
                SET_ERROR_AND_EXIT_CYCLE(err_local);
            }
        } while (now < next_tick);

        // 정상적으로 실행된 사이클 카운트
        ++cycle_count;
        if (cycle_count % LOG_INTERVAL_CYCLES == 0) {
            const uint64_t exec_ns = cycle_end - cycle_start;
            log_debug("cycle: %u \t max: %llu \t exec: %llu \t skips: %u",
                      cycle_count, max_cycle_ns, exec_ns, skip_count);
        }

        // 키패드 상태 업데이트: 눌린 키의 타이머 감소
        input_process_keys(&chip8);
    }

exit_cycle_label:
    return g_state.error_code;
}


static uint64_t get_current_time_ns(errcode_t *errcode) {
    assert(errcode != NULL);

    *errcode = ERR_NONE;

    struct timespec ts;
    const int rs_stat = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (rs_stat != 0) {
        log_error("clock_gettime error: %s", strerror(errno));
        *errcode = ERR_TIME_FUNC;
        return (uint64_t) -1; // 최대 값 사용
    }

    return (uint64_t) ts.tv_sec * NANOSECONDS_PER_SECOND + ts.tv_nsec;
}

void update_timers(const uint64_t tick_interval) {
    static u_int64_t accumulator = 0; // static이라 접근 제한된 전역 변수처럼 동작

    accumulator += tick_interval;

    while (accumulator >= TIMER_TICK_INTERVAL_NS) {
        if (chip8.sound_timer > 0) {
            --chip8.sound_timer;
            output_sound_beep();
        }
        if (chip8.delay_timer > 0) {
            --chip8.delay_timer;
        }
        output_clear_display();
        output_print_display(&chip8);
        accumulator -= TIMER_TICK_INTERVAL_NS;
    }
}

static errcode_t initialize_logging(FILE** log_file_ptr, const char* log_filename_suffix) {
    char log_path[512];
    int written = snprintf(log_path, sizeof(log_path), "%s%s", PROJECT_PATH, log_filename_suffix);
    if (written < 0 || (size_t)written >= sizeof(log_path)) {
        fprintf(stderr, "Error: Log path too long or snprintf error.\n");
        return ERR_PATH_TOO_LONG; // 적절한 에러 코드 사용
    }

    *log_file_ptr = fopen(log_path, "a");
    if (!*log_file_ptr) {
        perror("Log file open error");
        return ERR_FILE_OPEN_FAILED; // 적절한 에러 코드 사용
    }

    log_add_fp(*log_file_ptr, LOG_LEVEL); // LOG_LEVEL은 컴파일 시점에 결정
    log_set_level(LOG_INFO); // 또는 원하는 기본 레벨
    return ERR_NONE;
}

static void initialize_chip8_core(chip8_t* ch8) {
    assert(ch8 != NULL);

    memset(ch8, 0, sizeof(chip8_t));
    ch8->pc = PROGRAM_START_ADDR;
    memcpy(ch8->memory + FONTSET_ADDR, chip8_fontset, sizeof(chip8_fontset));
}

static errcode_t load_rom_to_chip8_memory(chip8_t* ch8, const char* rom_filename) {
    assert(ch8 != NULL); // Defensive check

    char rom_path[512];
    int written = snprintf(rom_path, sizeof(rom_path), "%s%s", ROM_PATH, rom_filename);
    if (written < 0 || (size_t)written >= sizeof(rom_path)) {
        log_error("ROM path too long or snprintf error.");
        return ERR_PATH_TOO_LONG;
    }

    FILE *rom_file = fopen(rom_path, "rb");
    if (!rom_file) {
        log_error("Failed to open ROM '%s': %s", rom_path, strerror(errno));
        return ERR_FILE_NOT_FOUND;
    }

    errcode_t err = ERR_NONE; // Assume success initially

    // Get ROM size
    if (fseek(rom_file, 0, SEEK_END) != 0) {
        log_error("Failed to seek to end of ROM file '%s': %s", rom_path, strerror(errno));
        err = ERR_FILE_READ_FAILED;
        goto close_rom_file;
    }
    long rom_size = ftell(rom_file);
    if (rom_size < 0) {
        log_error("Failed to get ROM size for '%s': %s", rom_path, strerror(errno));
        err = ERR_FILE_READ_FAILED;
        goto close_rom_file;
    }
    if (fseek(rom_file, 0, SEEK_SET) != 0) {
        log_error("Failed to seek to start of ROM file '%s': %s", rom_path, strerror(errno));
        err = ERR_FILE_READ_FAILED;
        goto close_rom_file;
    }

    // Validate ROM size
    if (rom_size == 0) {
        log_error("ROM file '%s' is empty.", rom_path);
        err = ERR_ROM_INVALID;
        goto close_rom_file;
    }
    if ((size_t)rom_size > (MEMORY_MAX_SIZE - PROGRAM_START_ADDR)) {
        log_error("ROM '%s' is too large: %ld bytes. Max allowed: %zu bytes.",
                  rom_path, rom_size, (MEMORY_MAX_SIZE - PROGRAM_START_ADDR));
        err = ERR_ROM_TOO_LARGE;
        goto close_rom_file;
    }

    // Read ROM into Chip-8 memory
    size_t bytes_read = fread(ch8->memory + PROGRAM_START_ADDR, 1, (size_t)rom_size, rom_file);
    if (bytes_read != (size_t)rom_size) {
        if (feof(rom_file)) {
            log_error("Failed to read ROM '%s': unexpected end of file. Expected %ld, got %zu.",
                      rom_path, rom_size, bytes_read);
        } else {
            log_error("Failed to read ROM '%s': %s. Expected %ld, got %zu.",
                      rom_path, strerror(errno), rom_size, bytes_read);
        }
        err = ERR_FILE_READ_FAILED;
        // No goto here, will proceed to close_rom_file
    }

close_rom_file:
    if (fclose(rom_file) == EOF) {
        log_warn("Failed to close ROM file '%s': %s", rom_path, strerror(errno));
        if (err == ERR_NONE) { // Only set error if no prior error occurred
            err = ERR_FILE_CLOSE_FAILED;
        }
    }
    return err;
}

static errcode_t initialize_platform_modules(chip8_t* ch8, bool* quit_flag) {
    errcode_t err = input_initialize();
    if (err != ERR_NONE) {
        log_error("Input module initialization failed: %d", err);
        return err;
    }

    // g_state.quit의 주소를 넘겨서 키보드 스레드가 종료 시점을 알 수 있도록 함
    err = terminal_io_init(ch8, quit_flag);
    if (err != ERR_NONE) {
        log_error("Terminal I/O initialization failed: %d", err);
        input_shutdown(); // input_initialize는 성공했으므로 해제 시도
        return err;
    }
    return ERR_NONE;
}

static void shutdown_platform_modules(void) {
    // terminal_io_shutdown() 함수가 있다고 가정하고 호출합니다.
    // 만약 없다면 이 줄은 주석 처리하거나 제거해야 합니다.
    // terminal_io_shutdown();
    // log_debug("Terminal I/O shut down."); // 실제 shutdown 함수가 있다면 로그 추가

    input_shutdown();
    log_debug("Input module shut down.");
}
