#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <signal.h>
#include <pthread.h>

// 전역 상태 변수들
static struct termios orig_term;
volatile sig_atomic_t running = 1;
char last_key = '\0';
int frame = 0; // 메인 스레드만 접근하므로 뮤텍스 필요 없음

// 뮤텍스 선언 - last_key 접근 시 사용
pthread_mutex_t input_mutex = PTHREAD_MUTEX_INITIALIZER;

// 터미널 raw 모드 진입 — VMIN을 1로 설정해서 read()가 1바이트를 받을 때까지 블록
void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_term);
    struct termios raw = orig_term;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN] = 1; // 최소 1바이트가 들어올 때까지 read() 블로킹
    raw.c_cc[VTIME] = 0; // 타임아웃 없음
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

// 터미널 원복
void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
}

// 화면 클리어 및 커서 이동
void clear_screen() {
    write(STDOUT_FILENO, "\x1b[2J", 4); // 전체 지우기
    write(STDOUT_FILENO, "\x1b[H", 3); // 커서 홈
}

// 간단한 디스플레이 업데이트 예시
void draw() {
    clear_screen();

    printf("Frame: %d\n", frame);
    printf("Press 'q' to quit.\n");
    printf("You typed: ");

    // last_key 소비(조회 후 삭제) 뮤텍스 사용
    pthread_mutex_lock(&input_mutex);
    char key = last_key; // 로컬 복사본 만들기
    last_key = '\0';       // 읽은 후 비활성화
    pthread_mutex_unlock(&input_mutex);

    if (key) {
        printf("%c\n", key);
    } else {
        printf("(none)\n");
    }

    fflush(stdout); // 화면에 즉시 출력 보장
}

// SIGINT 처리
void handle_sigint(int sig) {
    (void) sig;
    running = 0;
}

// 키보드 입력 처리 스레드 함수
void *keyboard_thread(void *arg) {
    (void) arg;
    while (running) {
        char c;
        // blocking read: 키가 들어올 때까지 대기
        if (read(STDIN_FILENO, &c, 1) > 0) {
            // last_key 업데이트할 때 뮤텍스 사용
            pthread_mutex_lock(&input_mutex);
            // 사실 여기선 필요는 없는데, mutex 사용 예시를 들기 위해 씀
            // 여러 작업을 함께 수행할 때 원자성을 보장하려면 필요함
            last_key = c;
            pthread_mutex_unlock(&input_mutex);

            if (c == 'q' || c == 'Q') {
                running = 0;
            }
        }
        // 여기서 루프가 다시 돌아가면서 또 read() 대기
    }
    return NULL;
}

int main() {
    // 터미널 설정
    enable_raw_mode();
    atexit(disable_raw_mode);
    signal(SIGINT, handle_sigint);

    // 키보드 입력 스레드 생성
    pthread_t kb_thread;
    if (pthread_create(&kb_thread, NULL, keyboard_thread, NULL) != 0) {
        perror("스레드 생성 실패");
        return 1;
    }

    int a = 0;
    for (int i = 0; i < 10; ++i) {
        ++a;
    }

    // 메인 루프 - 화면 갱신만 담당
    while (running) {
        // 화면 그리기
        draw();

        // 프레임 증가 (메인 스레드만 접근 & 단일 변수라 ㄱㅊ)
        frame++;

        // 약 10 FPS
        usleep(100000); // 0.1초
    }

    // 키보드 스레드 종료 대기
    pthread_join(kb_thread, NULL);

    // 뮤텍스 정리
    pthread_mutex_destroy(&input_mutex);

    printf("프로그램이 종료되었습니다.\n");
    return 0;
}
