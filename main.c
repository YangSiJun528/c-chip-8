#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * 전역 변수 선언
 */
bool quit = false; // 사이클 종료 플래그

// 시뮬레이션 시간 간격 (초), 한 프레임(단계)이 실제 시간으로 얼마나 진행되는지를 결정
// Δt = Delta Time
const double DT = 1.0 / 60.0;

// POSIX 시간 관련 상수
#define NANOSECONDS_PER_SECOND (1000000000L)  // 1초 = 10^9 나노초 = 1e9

// struct timespec에 대한 타입 별칭 정의
typedef struct timespec timespec_t;

void cycle(void);

timespec_t ts_sub(const timespec_t *, const timespec_t *);

timespec_t ts_add(const timespec_t *, const timespec_t *);

double ts_to_double(const timespec_t *);

double time_diff(const timespec_t *start, const timespec_t *end);

/**
 * 프로그램 시작점
 */
int main(void) {
    printf("Hello, World!\n");
    srand(time(NULL)); // 난수 생성기 초기화
    cycle(); // 메인 루프 실행
    return 0;
}

/**
 * 메인 시뮬레이션 루프
 *
 * 프레임 속도를 고정된 타임스텝으로 유지
 */
void cycle(void) {
    // 각 틱(프레임) 간의 시간 간격 설정 (60Hz)
    timespec_t tick_interval = {
        .tv_sec = 0,
        .tv_nsec = (long) (DT * NANOSECONDS_PER_SECOND) // DT를 나노초로 변환
    };

    // 다음 틱 시간 초기화 (현재 시간 + 간격)
    timespec_t next_tick;
    clock_gettime(CLOCK_MONOTONIC, &next_tick); // 현재 시간 가져오기
    next_tick = ts_add(&next_tick, &tick_interval);

    double t_sim = 0.0; // 시뮬레이션 내부 시간 (초)

    while (!quit) {
        // ────────────── 여기에 에뮬레이터 작업 넣기 ──────────────
        // ex) something(&cpu);
        // ───────────────────────────────────────────────────────

        // 시뮬레이션 내부 시간 고정 간격(DT)만큼 증가
        t_sim += DT;

        // 현재 시각 가져오기
        timespec_t now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        // 다음 틱까지 남은 시간 계산 - rem: remaining time
        timespec_t rem = ts_sub(&now, &next_tick);

        // 다음 틱 시간이 현재보다 미래인 경우 (남은 시간이 있음)
        if (rem.tv_sec > 0 || (rem.tv_sec == 0 && rem.tv_nsec > 0)) {
            // 남은 시간만큼 대기 (CPU 사용률 절약)
            // 몇 나노초 일찍 꺠워서 busy-waiting하면 jitter가 줄어들지만 우선 간단하게
            nanosleep(&rem, NULL);
        }

        // 다음 틱 시간 업데이트
        next_tick = ts_add(&next_tick, &tick_interval);
    }
}

/**
 * 두 timespec 구조체를 더하는 함수
 *
 * @param a 첫 번째 timespec
 * @param b 두 번째 timespec
 * @return 두 시간의 합
 */
timespec_t ts_add(const timespec_t *a, const timespec_t *b) {
    timespec_t result = {
        .tv_sec = a->tv_sec + b->tv_sec,
        .tv_nsec = a->tv_nsec + b->tv_nsec
    };

    // 나노초 부분이 1초를 넘어가면 초 부분으로 올림 처리
    if (result.tv_nsec >= NANOSECONDS_PER_SECOND) {
        ++result.tv_sec;
        result.tv_nsec -= NANOSECONDS_PER_SECOND;
    }

    return result;
}

/**
 * timespec 구조체 간의 차이를 계산하는 함수 (b - a)
 *
 * b가 a보다 커야 함
 *
 * @param a 이전 시간 (빼는 값)
 * @param b 현재 시간 (빼지는 값)
 * @return b - a 시간 차이
 */
timespec_t ts_sub(const timespec_t *a, const timespec_t *b) {
    // b가 a보다 나중(더 큰) 시간인지 확인
    assert((b->tv_sec > a->tv_sec) ||
        (b->tv_sec == a->tv_sec && b->tv_nsec >= a->tv_nsec));

    timespec_t result = {
        .tv_sec = b->tv_sec - a->tv_sec,
        .tv_nsec = b->tv_nsec - a->tv_nsec
    };

    // 나노초 부분이 음수면 초 부분에서 빌려옴
    if (result.tv_nsec < 0) {
        --result.tv_sec;
        result.tv_nsec += NANOSECONDS_PER_SECOND;
    }

    return result;
}

/**
 * timespec 구조체를 실수(초) 값으로 변환
 *
 * @param t 변환할 timespec
 * @return 초 단위의 실수 값
 */
double ts_to_double(const timespec_t t) {
    return (double) t.tv_sec + ((double) t.tv_nsec / NANOSECONDS_PER_SECOND);
}

/**
 * 두 시간 사이의 경과 시간을 초 단위로 계산
 *
 * @param start 시작 시간
 * @param end 종료 시간
 * @return 경과 시간(초)
 */
double time_diff(const timespec_t *start, const timespec_t *end) {
    return ts_to_double(ts_sub(start, end));
}
