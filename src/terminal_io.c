// terminal_io.c
#include "terminal_io.h"
#include "input.h" // input_set_key_down 사용
#include "log.h"
#include "errcode.h"
#include "global_config.h" // KEY_MAPPING을 여기서 정의하거나 chip8_config.h에서 가져올 수 있음

#include <termios.h>
#include <unistd.h>  // STDIN_FILENO, read
#include <pthread.h>
#include <stdio.h>   // perror
#include <string.h>  // strerror
#include <errno.h>   // errno
#include <stdlib.h>  // atexit (필요시)

static struct termios orig_term;
static pthread_t kb_thread_tid;
static chip8_t *g_chip8_ptr = NULL;      // init에서 설정
static volatile bool *g_quit_flag_ptr = NULL; // init에서 설정

// KEY_MAPPING은 terminal_io의 책임이므로 여기로 이동하거나,
// chip8_config.h에 두고 include하여 사용합니다. 여기서는 직접 정의.
static const char KEY_MAPPING[16] = {
    '1', '2', '3', '4', // 0, 1, 2, 3
    'q', 'w', 'e', 'r', // 4, 5, 6, 7
    'a', 's', 'd', 'f', // 8, 9, A, B
    'z', 'x', 'c', 'v'  // C, D, E, F
};

static void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_term) == -1) {
        log_error("tcgetattr failed: %s", strerror(errno));
        return; // 또는 에러 처리
    }
    // 파일 디스크립터 fildes에 연결된 터미널의 현재 속성을 읽어서 orig_term*에 저장
    struct termios raw = orig_term;

    // ECHO: 입력한 문자를 화면에 에코(반복) 출력
    // ICANON: 캐논컬(라인 단위) 모드를 사용해 줄바꿈 전까지 입력을 버퍼에 저장
    // ISIG: Ctrl-C, Ctrl-Z 등 시그널 생성 키를 처리
    // 위 3가지 flag 비활성화
    // c_lflag: local flags
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);

    raw.c_cc[VMIN] = 0; //VMIN: 비캐논컬 모드에서 read()가 반환하기 위한 최소 바이트 수
    raw.c_cc[VTIME] = 0;  //VTIME: 비캐논컬 모드에서 read()가 타임아웃하기 전 대기 시간

    // raw 설정을 STDIN_FILENO에 적용 (TCSANOW: 즉시 적용 flag)
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == -1) {
        log_error("tcsetattr failed: %s", strerror(errno));
    }
    log_info("Raw mode enabled.");
}

static void disable_raw_mode(void) {
    if (tcsetattr(STDIN_FILENO, TCSANOW, &orig_term) == -1) {
        log_error("tcsetattr failed to restore original terminal settings: %s", strerror(errno));
    } else {
        log_info("Raw mode disabled, terminal restored.");
    }
}

// 입력된 키에 해당하는 CHIP-8 키패드 인덱스를 반환
static int get_key_index(char key) {
    // 대문자인 경우에만 소문자로 변환 (비트 OR 연산 사용)
    if (key >= 'A' && key <= 'Z') {
        key |= 0x20;
    }

    for (int i = 0; i < 16; i++) {
        char mapped_key = KEY_MAPPING[i];
        // KEY_MAPPING에 대문자가 있을 수 있으므로, mapped_key도 소문자로 변환하여 비교
        if (mapped_key >= 'A' && mapped_key <= 'Z') {
            mapped_key |= 0x20;
        }
        if (mapped_key == key) {
            return i;
        }
    }
    return -1; // 매핑되는 키 없음
}

static void *keyboard_thread(void *arg) {
    (void)arg; // Unused parameter
    log_info("Keyboard thread started.");

    while (g_quit_flag_ptr && !(*g_quit_flag_ptr)) {
        char c = '\0';
        ssize_t bytes_read = read(STDIN_FILENO, &c, 1);

        if (bytes_read == -1 && errno != EAGAIN) {
            log_error("read error in keyboard_thread: %s", strerror(errno));
            if (g_quit_flag_ptr) *g_quit_flag_ptr = true; // 심각한 오류 시 종료 유도
            break;
        }

        if (bytes_read > 0) {
            int key_idx = get_key_index(c);
            if (key_idx != -1) {
                if (g_chip8_ptr) {
                    input_set_key_down(g_chip8_ptr, key_idx);
                }
            } else {
                // ESC 키 (ASCII 27) 등으로 종료 처리 가능
                if (c == 27) { // ESC key
                    log_info("ESC key pressed, setting quit flag.");
                    if (g_quit_flag_ptr) *g_quit_flag_ptr = true;
                    break; 
                }
                log_trace("Unmapped key pressed: %c (ASCII: %d)", c, (int)c);
            }
        }
        // CPU 사용량을 줄이기 위해 약간의 sleep 추가 (선택 사항)
        // VTIME=0, VMIN=0 일때는 read가 즉시 리턴하므로 루프가 매우 빠르게 돌 수 있음
        // usleep(1000); // 1ms sleep
    }
    log_info("Keyboard thread exiting.");
    return NULL;
}

errcode_t terminal_io_init(chip8_t *chip8_instance, volatile bool *quit_flag_ptr) {
    if (!chip8_instance || !quit_flag_ptr) {
        log_error("NULL pointer passed to terminal_io_init");
        return ERR_INVALID_ARGUMENT;
    }
    g_chip8_ptr = chip8_instance;
    g_quit_flag_ptr = quit_flag_ptr;

    enable_raw_mode();

    if (pthread_create(&kb_thread_tid, NULL, keyboard_thread, NULL) != 0) {
        log_error("Failed to create keyboard thread: %s", strerror(errno));
        disable_raw_mode(); // 스레드 생성 실패 시 raw 모드 복원
        return ERR_THREAD_CREATION_FAILED;
    }
    log_info("Terminal I/O initialized successfully.");
    return ERR_NONE;
}

void terminal_io_shutdown(void) {
    log_info("Shutting down terminal I/O...");
    if (g_quit_flag_ptr && !(*g_quit_flag_ptr)) {
        // 스레드가 아직 종료되지 않았다면, quit 플래그를 설정하여 종료 유도
        // 이는 keyboard_thread가 g_quit_flag_ptr를 주기적으로 확인한다고 가정
        *g_quit_flag_ptr = true;
    }

    // 키보드 스레드가 정상적으로 종료될 때까지 대기
    if (kb_thread_tid != 0) { // 스레드가 생성된 경우에만 join
        if (pthread_join(kb_thread_tid, NULL) != 0) {
            log_warn("Failed to join keyboard thread: %s", strerror(errno));
        } else {
            log_info("Keyboard thread joined successfully.");
        }
        kb_thread_tid = 0; // 스레드 ID 초기화
    }
    
    disable_raw_mode(); // 터미널 모드 복원
    log_info("Terminal I/O shutdown complete.");
}
