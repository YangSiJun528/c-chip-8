#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

/* 상수 정의 */
#define MS_TO_NS 1000000     // 밀리초를 나노초로 변환
#define NS_TO_MS 0.000001    // 나노초를 밀리초로 변환
#define S_TO_MS  1000.0      // 초를 밀리초로 변환
#define NS_PER_S 1000000000L // 1초당 나노초

/* 전역 변수 */
bool quit = false;
const double DT_MS = (1.0 / 60.0) * S_TO_MS; // 60Hz (16.666ms)
const long BUSY_WAIT_NS = MS_TO_NS * 3;

/* timespec 조작 함수 */
// 두 timespec 더하기
typedef struct timespec timespec;
timespec ts_add(timespec a, timespec b) {
    timespec r = { a.tv_sec + b.tv_sec, a.tv_nsec + b.tv_nsec };
    if (r.tv_nsec >= NS_PER_S) {
        r.tv_sec++;
        r.tv_nsec -= NS_PER_S;
    }
    return r;
}

// b - a 계산
timespec ts_sub(timespec a, timespec b) {
    timespec r = { b.tv_sec - a.tv_sec, b.tv_nsec - a.tv_nsec };
    if (r.tv_nsec < 0) {
        r.tv_sec--;
        r.tv_nsec += NS_PER_S;
    }
    return r;
}

// timespec 을 밀리초(double)로 변환
double ts_to_ms(timespec ts) {
    return (ts.tv_sec * S_TO_MS) + (ts.tv_nsec * NS_TO_MS);
}

// 경과 시간(ms)
double time_diff_ms(timespec start, timespec end) {
    return ts_to_ms(ts_sub(start, end));
}

/**
 * 몰랐던 내용 메모
 * inline
 *   - 컴파일러에 “가능하면 호출 대신 코드에 직접 삽입”을 제안(옵션따라 안해줄수도 있음)
 *   - 호출 오버헤드를 줄여 작은 함수에 유리
 *   - static이나 extern 키워드 필요
 *     - inline을 사용해도 external linkage이지만, inline definition으로 취급되어
 *       링커용 심볼을 생성하지 않아 링킹 단계에서 Undefined symbols 에러가 발생하기 때문에
 *       static/extern 키워드를 사용하여 internal/external linkage 지정 필요
 *
 * static
 *   - internal linkage 지정 → 이 파일 내에서만 사용
 *   - 외부 심볼을 생성하지 않아 링커 에러(미정의 심볼) 방지
 *
 * extern inline
 *   - external linkage 유지 + inline 제안
 *   - 심볼 생성 → 다른 T.U.에서 참조 가능
 */
static inline bool ts_before(const struct timespec *a,
                             const struct timespec *b) {
    return a->tv_sec  < b->tv_sec  ||
          (a->tv_sec == b->tv_sec && a->tv_nsec < b->tv_nsec);
}

// 프레임 처리 및 타이밍
void cycle(void) {
    struct timespec tick_interval = { 0, (long)(DT_MS * MS_TO_NS) };
    struct timespec next_tick;
    clock_gettime(CLOCK_MONOTONIC, &next_tick);
    next_tick = ts_add(next_tick, tick_interval);

    // 시뮬레이션 시작 시각 저장
    struct timespec sim_start;
    clock_gettime(CLOCK_MONOTONIC, &sim_start);

    double t_sim_ms = 0.0;
    int frame_count = 0;

    while (!quit) {
        struct timespec frame_start, now, sleep_time, frame_end;
        clock_gettime(CLOCK_MONOTONIC, &frame_start);

        // ─── 여기에 에뮬레이터 작업 삽입 ───
        int random_ms = rand() % 10;
        usleep(random_ms * 1000);
        // ───────────────────────────────

        t_sim_ms += DT_MS;

        clock_gettime(CLOCK_MONOTONIC, &now);

        // 전체 남은 시간 until next_tick
        timespec rem = ts_sub(now, next_tick);
        long rem_ns = rem.tv_sec * NS_PER_S + rem.tv_nsec;

        // 1) Sleep if there’s more than 1ms left
        if (rem_ns > BUSY_WAIT_NS) {
            // sleep_time = remaining_time - BUSY_WAIT_NS
            timespec sleep_ts = {
                .tv_sec  = (rem_ns - BUSY_WAIT_NS) / NS_PER_S,
                .tv_nsec = (rem_ns - BUSY_WAIT_NS) % NS_PER_S
            };
            nanosleep(&sleep_ts, NULL);
        }

        // 2) Busy‑wait for the final ~1ms
        do {
            clock_gettime(CLOCK_MONOTONIC, &now);
        } while (ts_before(&now, &next_tick));

        // Advance next_tick for the next frame
        next_tick = ts_add(next_tick, tick_interval);

        clock_gettime(CLOCK_MONOTONIC, &frame_end);

        double elapsed_ms = time_diff_ms(frame_start, frame_end);
        // 실제 시뮬레이션 경과 시간 (시작~현재)
        double real_ms = time_diff_ms(sim_start, frame_end);
        double diff_ms = real_ms - t_sim_ms;

        printf(
            "Frame %3d | Sim: %.2f ms | Work: %2d ms | Real: %.2f ms | "
            "Elapsed frame: %.2f ms | Target: %.2f ms | Drift: %+.2f ms\n",
            frame_count + 1,
            t_sim_ms,
            random_ms,
            real_ms,
            elapsed_ms,
            DT_MS,
            diff_ms
        );

        if (++frame_count >= 100) quit = true;
    }
}

int main(void) {
    printf("타이밍 테스트 시작\n");
    printf("목표 프레임 간격: %.2f ms (60 Hz)\n\n", DT_MS);

    srand(time(NULL));
    cycle();

    printf("\n테스트 완료\n");
    return 0;
}
