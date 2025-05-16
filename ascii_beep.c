#include <stdio.h>
#include <unistd.h>

int main() {
    for (int i = 0; i < 5; i++) {
        printf("\a"); // 비프 음 내기
        fflush(stdout); // 버퍼 비우기 — 즉시 출력
        usleep(500000); // 0.5초 대기
    }
    return 0;
}
