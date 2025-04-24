#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include "log.h"
#include "errcode.h"

/**
 * 전역 변수 선언
 */
bool quit = false; // 사이클 종료 플래그

// 함수 호출 결과나 그런 에러는 전역변수 안씀
// 이거는 발생하면 시스템 자체를 멈추는 전역 에러임
errcode_t system_errcode = ERR_NONE; // 시스템 에러

// 시뮬레이션 시간 간격 (초), 한 프레임(단계)이 실제 시간으로 얼마나 진행되는지를 결정
// Δt = Delta Time
const double DT = 1.0 / 60.0;

// POSIX 시간 관련 상수
#define NANOSECONDS_PER_SECOND (1000000000L)  // 1초 = 10^9 나노초 = 1e9

// struct timespec에 대한 타입 별칭 정의
typedef struct timespec timespec_t;

// 로그를 위한 상수 정의
#define LOG_INTERVAL 60      // 로그 출력 주기 (프레임 수)
#define PERF_THRESHOLD 0.002 // 성능 경고 임계값 (초)

void cycle(void);

timespec_t ts_sub(const timespec_t *, const timespec_t *);

timespec_t ts_add(const timespec_t *, const timespec_t *);

double ts_to_double(const timespec_t *);

/**
 * 프로그램 시작점
 */
int main(void) {
    // 로그 레벨 설정 (DEBUG 이상 레벨만 출력)
    log_set_level(LOG_DEBUG);

    log_info("[MAIN] 프로그램 시작 - 버전 1.0.0");
    log_debug("[MAIN] 초기 설정: DT=%.6f초 (%.1fHz)", DT, 1.0/DT);

    // 난수 생성기 초기화
    srand(time(NULL));
    log_trace("[MAIN] 난수 생성기 초기화 완료");

    // 메인 루프 실행
    cycle();

    // 프로그램 종료
    if (system_errcode != ERR_NONE) {
        log_error("[MAIN] system_errcode: %d 발생, 프로그램 종료", system_errcode);
        return system_errcode;
    }
    log_info("[MAIN] 프로그램 종료");
    return 0;
}

/**
 * 메인 시뮬레이션 루프
 *
 * 프레임 속도를 고정된 타임스텝으로 유지
 */
void cycle(void) {
    log_debug("[CYCLE] 메인 사이클 시작");

    // 각 틱(프레임) 간의 시간 간격 설정 (60Hz)
    const timespec_t tick_interval = {
        .tv_sec = 0,
        .tv_nsec = (long) (DT * NANOSECONDS_PER_SECOND) // DT를 나노초로 변환
    };

    log_trace("[CYCLE] 틱 간격 설정: %ld 나노초", tick_interval.tv_nsec);

    // 다음 틱 시간 초기화 (현재 시간 + 간격)
    timespec_t next_tick;
    clock_gettime(CLOCK_MONOTONIC, &next_tick); // 현재 시간 가져오기
    next_tick = ts_add(&next_tick, &tick_interval);

    // 시뮬레이션 상태 변수들 초기화
    double t_sim = 0.0; // 시뮬레이션 내부 시간 (초)
    int frame_count = 0; // 프레임 카운터
    double frame_time = 0.0; // 프레임 처리 시간
    double max_frame_time = 0.0; // 최대 프레임 처리 시간
    double avg_frame_time = 0.0; // 평균 프레임 처리 시간
    double total_frame_time = 0.0; // 누적 프레임 처리 시간
    int frame_drops = 0; // 드롭된 프레임 수

    log_debug("[CYCLE] 메인 루프 시작 - 목표 프레임 속도: %.1f FPS", 1.0/DT);

    while (!quit) {
        // 사이클 시작 시간 기록
        timespec_t cycle_start;
        clock_gettime(CLOCK_MONOTONIC, &cycle_start);

        log_trace("[FRAME:%d] 시작 (시뮬레이션 시간: %.3f초)",
                  frame_count, t_sim);

        // ────────────── 여기에 에뮬레이터 작업 넣기 ──────────────
        // ex) something(&cpu);
        // ───────────────────────────────────────────────────────

        // 시뮬레이션 내부 시간 고정 간격(DT)만큼 증가
        t_sim += DT;
        frame_count++;

        // 현재 시각 가져오기
        timespec_t now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        // 이 프레임의 처리 시간 계산
        timespec_t cycle_end;
        clock_gettime(CLOCK_MONOTONIC, &cycle_end);
        timespec_t frame_ts = ts_sub(&cycle_start, &cycle_end);
        frame_time = ts_to_double(&frame_ts);
        total_frame_time += frame_time;
        avg_frame_time = total_frame_time / frame_count;

        // 최대 프레임 처리 시간 업데이트
        if (frame_time > max_frame_time) {
            max_frame_time = frame_time;
            log_debug("[PERF] 최대 프레임 처리 시간 갱신: %.6f초 (프레임 #%d)",
                      max_frame_time, frame_count);
        }

        // 프레임 처리 시간이 임계값을 초과하면 경고
        if (frame_time > PERF_THRESHOLD) {
            log_warn("[PERF] 프레임 지연: %.6f초 (프레임 #%d, 목표: %.6f초)",
                     frame_time, frame_count, DT);
        }

        // 다음 틱까지 남은 시간 계산 - rem: remaining time
        timespec_t rem = ts_sub(&now, &next_tick);

        // 다음 틱 시간이 이미 지났는지 확인
        if (rem.tv_sec < 0 || (rem.tv_sec == 0 && rem.tv_nsec < 0)) {
            // 프레임 드롭 발생
            frame_drops++;
            log_warn("[TIMING] 프레임 드롭 #%d (프레임 #%d)",
                     frame_drops, frame_count);

            // 다음 틱 시간을 현재 시간 기준으로 재설정
            clock_gettime(CLOCK_MONOTONIC, &next_tick);
            next_tick = ts_add(&next_tick, &tick_interval);
        } else {
            // 다음 틱 시간이 현재보다 미래인 경우 (남은 시간이 있음)
            log_trace("[TIMING] 남은 대기 시간: %.6f초", ts_to_double(&rem));

            // 남은 시간만큼 대기 (CPU 사용률 절약)
            // 몇 나노초 일찍 꺠워서 busy-waiting하면 jitter가 줄어들지만
            // 우선 간단하게 구현
            const int rs = nanosleep(&rem, NULL);
            if (rs != 0) {
                //TODO: 이거 나중에 goto로 개선할수도 있을 듯?
                log_error("[ERROR] nanosleep 실패: %s", strerror(errno));
                system_errcode = errno;
                quit = true;
            }
        }

        // 주기적으로 성능 통계 정보 출력
        if (frame_count % LOG_INTERVAL == 0) {
            log_debug("[STATS] %d프레임: 평균=%.6f초, 최대=%.6f초, 드롭=%d",
                      frame_count, avg_frame_time, max_frame_time,
                      frame_drops);
        }

        // 다음 틱 시간 업데이트
        next_tick = ts_add(&next_tick, &tick_interval);
    }

    log_info("[CYCLE] 종료 - 총 %d 프레임 (시뮬레이션 시간: %.3f초)",
             frame_count, t_sim);
    log_info("[STATS] 요약: 평균=%.6f초/프레임, 최대=%.6f초, 드롭=%d",
             avg_frame_time, max_frame_time, frame_drops);
}

/**
 * 두 timespec 구조체를 더하는 함수
 *
 * @param a 첫 번째 timespec
 * @param b 두 번째 timespec
 * @return 두 시간의 합
 */
timespec_t ts_add(const timespec_t *a, const timespec_t *b) {
    log_trace("[TIME] ts_add: a={%ld.%09ld}, b={%ld.%09ld}",
              a->tv_sec, a->tv_nsec, b->tv_sec, b->tv_nsec);

    timespec_t result = {
        .tv_sec = a->tv_sec + b->tv_sec,
        .tv_nsec = a->tv_nsec + b->tv_nsec
    };

    // 나노초 부분이 1초를 넘어가면 초 부분으로 올림 처리
    if (result.tv_nsec >= NANOSECONDS_PER_SECOND) {
        ++result.tv_sec;
        result.tv_nsec -= NANOSECONDS_PER_SECOND;
    }

    log_trace("[TIME] ts_add 결과: {%ld.%09ld}",
              result.tv_sec, result.tv_nsec);
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
    log_trace("[TIME] ts_sub: a={%ld.%09ld}, b={%ld.%09ld}",
              a->tv_sec, a->tv_nsec, b->tv_sec, b->tv_nsec);

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

    log_trace("[TIME] ts_sub 결과: {%ld.%09ld}",
              result.tv_sec, result.tv_nsec);
    return result;
}

/**
 * timespec 구조체를 실수(초) 값으로 변환
 *
 * @param t 변환할 timespec
 * @return 초 단위의 실수 값
 */
double ts_to_double(const timespec_t *t) {
    const double result = (double) t->tv_sec + (
        (double) t->tv_nsec / NANOSECONDS_PER_SECOND);
    log_trace("[TIME] ts_to_double: {%ld.%09ld} -> %.9f초",
              t->tv_sec, t->tv_nsec, result);
    return result;
}