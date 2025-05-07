/*
 * 크게 1~8ms까지 나올 떄도 있음. 이정도면 프레임 드랍을 무조건 고려해야 하나?
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <unistd.h>

#define ITERATIONS       100000          // 반복 횟수
#define MIN_SLEEP_NS 1000000UL        // 최소 1ms
#define MAX_SLEEP_NS 50000000UL       // 최대 50ms

static uint64_t get_time_ns() {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

int main(void) {
    srand((unsigned)time(NULL));

    printf("iter, requested_ns, actual_ns, diff_ns\n");

    for (int i = 0; i < ITERATIONS; i++) {
        // 1) 랜덤한 요청 시간 생성
        uint64_t req_ns = MIN_SLEEP_NS +
            (uint64_t)(rand()) * (MAX_SLEEP_NS - MIN_SLEEP_NS) / RAND_MAX;

        // 2) 요청 전 시간
        uint64_t t0 = get_time_ns();

        // 3) nanosleep 수행
        struct timespec req = {
            .tv_sec  = req_ns / 1000000000ULL,
            .tv_nsec = req_ns % 1000000000ULL
        };
        struct timespec rem;
        while (nanosleep(&req, &rem) == -1 && errno == EINTR) {
            // 신호에 의해 중단된 경우 남은 시간만큼 재호출
            req = rem;
        }

        // 4) 요청 후 시간
        uint64_t t1 = get_time_ns();

        // 5) 실제 경과 및 차이 계산
        uint64_t actual_ns = t1 - t0;
        int64_t diff_ns = (int64_t)actual_ns - (int64_t)req_ns;

        // 6) CSV 형식 출력
        printf("%d, %10" PRIu64 ", %10" PRIu64 ", %10" PRId64 "\n",
               i+1, req_ns, actual_ns, diff_ns);
    }

    return 0;
}
