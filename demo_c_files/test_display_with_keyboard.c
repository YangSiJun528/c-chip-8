#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <signal.h>

// 터미널 설정 저장
static struct termios orig_term;

// 터미널 raw 모드 진입
void enable_raw_mode() {
    // 파일 디스크립터 fildes에 연결된 터미널의 현재 속성을 읽어서 orig_term*에 저장
    tcgetattr(STDIN_FILENO, &orig_term);
    struct termios raw = orig_term;
    // ECHO: 입력한 문자를 화면에 에코(반복) 출력
    // ICANON: 캐논컬(라인 단위) 모드를 사용해 줄바꿈 전까지 입력을 버퍼에 저장
    // ISIG: Ctrl-C, Ctrl-Z 등 시그널 생성 키를 처리
    // 위 3가지 flag 비활성화
    // c_lflag: local flags
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    // c_cc: control chars
    raw.c_cc[VMIN] = 0; //VMIN: 비캐논컬 모드에서 read()가 반환하기 위한 최소 바이트 수
    raw.c_cc[VTIME] = 0; //VTIME: 비캐논컬 모드에서 read()가 타임아웃하기 전 대기 시간
    // raw 설정을 STDIN_FILENO에 적용 (TCSANOW: 즉시 적용 flag)
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

// 터미널 원복
void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
}

// 화면 클리어 및 커서 이동
void clear_screen() {
    write(STDOUT_FILENO, "\x1b[2J", 4);   // 전체 지우기
    write(STDOUT_FILENO, "\x1b[H", 3);    // 커서 홈
}

// 간단한 디스플레이 업데이트 예시
void draw(int frame) {
    clear_screen();
    printf("Frame: %d\n", frame);
    printf("Press 'q' to quit.\n");
    printf("You typed: ");
}

volatile sig_atomic_t running = 1;

void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

int main() {
    enable_raw_mode();
    // 프로그램 종료 시 터미널 설정 복원 콜백함수 등록
    atexit(disable_raw_mode);
    // SIGINT 시그널 발생 (주로 ctrl+c) 시 사용자 정의 처리
    signal(SIGINT, handle_sigint);

    int frame = 0;
    char last_key = '\0';

    while (running) {
        // 비동기 키 입력 대기 (타임아웃 0)
        // 내가 계획한건 이게 아니라 입력 전용 블로킹 스레드가 있는거,
        // 이 코드가 잘 동작하긴 하지만 뭔지 잘 모름.
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        struct timeval tv = {0, 0};
        int r = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
        if (r > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
            char c;
            if (read(STDIN_FILENO, &c, 1) > 0) {
                last_key = c;
                if (c == 'q' || c == 'Q') {
                    running = 0;
                    break;
                }
            }
        }

        // 화면 그리기
        draw(frame);
        if (last_key) {
            printf("%c\n", last_key);
        } else {
            printf("(none)\n");
        }

        // 프레임 정보
        usleep(100000); // 100ms
        frame++;
    }

    return 0;
}
