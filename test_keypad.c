#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

// 터미널 설정을 저장할 변수들
static struct termios old_tio;
static struct termios new_tio;

// 터미널을 raw 모드로 설정
void init_terminal() {
    // 현재 터미널 설정 저장
    tcgetattr(STDIN_FILENO, &old_tio);

    // 새 설정 준비 (기존 설정 복사)
    new_tio = old_tio;

    // 입력 모드 플래그 설정
    // ICANON : 캐노니컬 모드 비활성화 (라인 단위 입력 → 문자 단위 입력)
    // ECHO   : 입력 문자 화면 출력 비활성화
    // ISIG   : Ctrl-C, Ctrl-Z 등의 특수 키 시그널 비활성화
    new_tio.c_lflag &= ~(ICANON | ECHO | ISIG);

    // VMIN = 0  : 읽을 최소 문자 수를 0으로 설정 (즉시 반환)
    // VTIME = 0 : 읽기 타임아웃을 0으로 설정 (블록킹 없음)
    new_tio.c_cc[VMIN] = 0;
    new_tio.c_cc[VTIME] = 0;

    // 새 터미널 설정 적용
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

    printf("터미널이 논블로킹 모드로 설정되었습니다.\n");
}

// 종료 시 원래 터미널 설정으로 복원
void reset_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
    printf("터미널 설정이 복원되었습니다.\n");
}

// 키 입력 확인 (논블로킹)
int kbhit() {
    unsigned char ch;
    int nread;

    // 문자 읽기 시도 (논블로킹)
    nread = read(STDIN_FILENO, &ch, 1);

    // 읽은 문자가 있으면
    if (nread == 1) {
        // 읽은 문자를 다시 stdin으로 되돌림
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

// 키 읽기
int getch() {
    int ch;

    // 표준 입력에서 한 문자 읽기
    ch = getchar();

    return ch;
}

int main() {
    int ch;

    // 터미널 설정
    init_terminal();

    // 프로그램 종료 시 터미널 설정 복원
    atexit(reset_terminal);

    printf("키 입력 모니터링 시작... (ESC 키를 누르면 종료)\n");

    while (1) {
        // 키 입력 확인
        if (kbhit()) {
            ch = getch();

            // ESC 키로 종료
            if (ch == 27) {
                // 화살표 키 등의 확장 키인지 확인 (ESC + [ + 추가 문자)
                usleep(10000); // 약간의 대기 시간으로 후속 문자 확인

                if (kbhit()) {
                    int ch2 = getch();
                    if (ch2 == '[') {
                        if (kbhit()) {
                            int ch3 = getch();
                            printf("특수 키 감지: ESC [ %c (코드: %d)\n", ch3, ch3);

                            // 특수 키 처리
                            switch(ch3) {
                                case 'A': printf("위쪽 화살표 키\n"); break;
                                case 'B': printf("아래쪽 화살표 키\n"); break;
                                case 'C': printf("오른쪽 화살표 키\n"); break;
                                case 'D': printf("왼쪽 화살표 키\n"); break;
                            }
                        }
                    } else {
                        printf("Alt+%c 조합 키 (코드: ESC+%d)\n", ch2, ch2);
                    }
                } else {
                    // 진짜 ESC 키만 눌린 경우
                    printf("ESC 키 감지. 프로그램을 종료합니다.\n");
                    break;
                }
            } else {
                // 일반 키 처리
                printf("키 입력: '%c' (ASCII: %d)\n", ch, ch);

                // 스캔코드 표시 (macOS에서는 직접적인 스캔코드 접근 불가)
                printf("참고: 실제 스캔코드는 아니며, ASCII 값만 표시됩니다.\n");
            }
        }

        // CPU 과부하 방지를 위한 짧은 대기
        usleep(1000); // 1ms
    }

    return 0;
}