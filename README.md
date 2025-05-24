
> 이 프로젝트는 C언어를 처음 배우면서 진행한 첫 번째 프로젝트로, 안전하지 않은 부분이 포함될 수 있습니다.   
> Apple Silicon 기반의 macOS 14.4.1에서 제작하였고, 다른 환경에서의 동작은 확인하지 않았습니다.

> 이 README 파일은 LLM(Manus AI)의 답변을 기반으로 작성되었습니다.

# C-CHIP-8 에뮬레이터

C언어로 작성된 CHIP-8 에뮬레이터입니다. 이 프로젝트는 C언어를 처음 배우면서 만든 첫 번째 프로젝트로, 외부 라이브러리 의존성을 최소화하고 터미널 환경에서 동작하는 CHIP-8 에뮬레이터를 구현했습니다.

https://github.com/user-attachments/assets/5a32d85b-1685-42cb-98b4-dfeb2fcfb3d1

https://github.com/user-attachments/assets/847f2bfb-0ee8-4e85-8c2b-a3846283be91


## 프로젝트 소개

CHIP-8은 1970년대에 개발된 간단한 가상 머신으로, 게임 개발을 위해 설계되었습니다. 이 프로젝트는 CHIP-8의 명세를 따라 에뮬레이터를 구현하여 Pong, Tetris 등의 고전 게임을 실행할 수 있게 합니다.

주요 특징:
- 순수 C99 표준 사용
- 외부 그래픽 라이브러리 없이 터미널 기반 디스플레이 구현
- 멀티스레딩을 활용한 비동기 키보드 입력 처리
- 고정 타임스텝(Fixed Timestep) 방식의 타이머 구현
- 로깅 시스템 통합

## 주요 기능 및 구현 방식

### 1. 타이머 구현

- 고정 타임스텝(Fixed Timestep) 방식 사용
- CLOCK_MONOTONIC 기반의 정확한 시간 측정
- 누산기(Accumulator) 패턴을 활용한 타이머 카운트다운
- 60Hz(16.67ms) 주기로 타이머 레지스터 업데이트

```c
// 타이머 업데이트 예시
void update_timers(uint64_t tick_interval) {
    static uint64_t timer_accumulator = 0;
    
    timer_accumulator += tick_interval;
    
    // 16.67ms(60Hz) 마다 타이머 감소
    while (timer_accumulator >= TIMER_TICK_INTERVAL_NS) {
        if (chip8.delay_timer > 0) {
            --chip8.delay_timer;
        }
        
        if (chip8.sound_timer > 0) {
            --chip8.sound_timer;
            // 사운드 타이머가 0이 아니면 비프음 재생
            sound_beep();
        }
        
        timer_accumulator -= TIMER_TICK_INTERVAL_NS;
    }
}
```

### 2. 디스플레이 출력

- 터미널 기반 텍스트 출력으로 그래픽 구현
- 1비트 픽셀 정보를 저장하는 디스플레이 버퍼 관리
- 60Hz 주기로 화면 갱신
- 유니코드 문자를 활용한 픽셀 표현

```c
// 디스플레이 출력 예시
void print_display(const struct chip8 *chip) {
    print_border();
    
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        printf("│");
        for (int x = 0; x < DISPLAY_WIDTH; x++) {
            // 픽셀 상태 확인
            const uint16_t byte_index = (y * 64 + x) / 8;
            const uint8_t bit_mask = 1 << (7 - (x % 8));
            const bool pixel_on = (chip->display[byte_index] & bit_mask) != 0;
            
            // 픽셀 상태에 따라 다른 문자 출력
            printf("%s", pixel_on ? PIXEL_ON_STR : PIXEL_OFF_STR);
        }
        printf("│\n");
    }
    
    print_border();
}
```

### 3. 키보드 입력 처리

- 멀티스레딩을 활용한 비동기 키보드 입력 처리
- termios API를 사용한 raw 모드 터미널 입력
- 뮤텍스를 활용한 스레드 간 안전한 데이터 공유
- 16개 키패드 매핑 및 상태 관리

```c
// 키보드 입력 스레드 예시
void *keyboard_thread(void *arg) {
    char c;
    while (!g_state.quit) {
        // 키 입력 대기
        if (read(STDIN_FILENO, &c, 1) == 1) {
            int key_idx = get_key_index(c);
            
            // 유효한 키 입력인 경우 처리
            if (key_idx >= 0) {
                pthread_mutex_lock(&input_mutex);
                g_state.keypad[key_idx] = INPUT_TICK;
                pthread_mutex_unlock(&input_mutex);
            }
        }
    }
    return NULL;
}
```

### 4. CPU 명령어 처리

- CHIP-8 명세에 따른 35개 명령어 구현
- 명령어 Fetch-Decode-Execute 사이클 구현
- 스택, 레지스터, 메모리 관리
- 랜덤 숫자 생성, 충돌 감지 등 게임 로직 지원

```c
// 명령어 처리 예시 (일부)
switch (opcode & 0xF000) {
    case 0xD000: {
        // Dxyn - DRW Vx, Vy, nibble: draw n-byte sprite at (Vx, Vy)
        const uint8_t vx = (opcode & 0x0F00) >> 8;
        const uint8_t vy = (opcode & 0x00F0) >> 4;
        const uint8_t n = (opcode & 0x000F);
        
        const uint8_t x = chip8.v[vx];
        const uint8_t y = chip8.v[vy];
        
        // 스프라이트 그리기 및 충돌 감지 로직
        // ...
    }
    // 다른 명령어 처리
}
```

### 5. 에러 처리 및 로깅

- 전역 에러 코드 관리
- 다양한 로그 레벨 지원 (TRACE, DEBUG, INFO, WARN, ERROR, FATAL)
- 파일 기반 로깅으로 디스플레이 출력과 분리
- 성능 모니터링 및 디버깅 지원

```c
// 에러 처리 예시
#define SET_ERROR_AND_EXIT(err_code) do { \
    g_state.error_code = (err_code); \
    g_state.quit = true; \
    goto exit_cycle; \
} while(0)

// 로깅 예시
log_debug("cycle: %u \t max: %llu \t exec: %llu \t skips: %u",
          cycle_count, max_cycle_ns, exec_ns, skip_count);
```

## 프로젝트 구조

```
.
├── CMakeLists.txt          # 빌드 설정 파일
├── LICENSE                 # 라이센스 파일
├── NOTE.md                 # 개발 과정 및 의사결정 기록
├── README.md               # 프로젝트 설명 문서
├── demo_c_files            # 기능 테스트용 데모 파일
│   └── ... 
├── doc                     # 참고한 LLM의 답변을 정리한 문서 
│   └── ...
├── roms                    # CHIP-8 ROM 파일
│   ├── IBM_Logo.ch8
│   ├── Pong (1 player).ch8
│   └── ...
└── src                     # 소스 코드
    ├── chip8.h             # CHIP-8 구조체 및 상수 정의
    ├── errcode.h           # 에러 코드 정의
    ├── log.c               # 로깅 시스템 구현
    ├── log.h               # 로깅 인터페이스
    └── main.c              # 메인 프로그램 및 에뮬레이터 로직
```

## 빌드 및 실행 방법

### 요구사항

- C99 호환 컴파일러 (GCC, Clang 등)
- CMake 3.30 이상
- POSIX 호환 시스템 (Linux, macOS)
- pthread 라이브러리

### 빌드 방법

```bash
# 프로젝트 클론
git clone https://github.com/yourusername/c-chip-8.git
cd c-chip-8

# 빌드 디렉토리 생성 및 이동
mkdir build
cd build

# CMake 설정 및 빌드
cmake ..
make
```

### 실행 방법

```bash
# 기본 실행 (IBM Logo ROM)
./c_chip_8

# 특정 ROM 실행
./c_chip_8 ../roms/Pong\ \(1\ player\).ch8
```

## 키 매핑

CHIP-8 키패드는 다음과 같이 매핑되어 있습니다:

```
CHIP-8 키패드    키보드 매핑
+-+-+-+-+       +-+-+-+-+
|1|2|3|C|       |1|2|3|4|
+-+-+-+-+       +-+-+-+-+
|4|5|6|D|       |Q|W|E|R|
+-+-+-+-+  -->  +-+-+-+-+
|7|8|9|E|       |A|S|D|F|
+-+-+-+-+       +-+-+-+-+
|A|0|B|F|       |Z|X|C|V|
+-+-+-+-+       +-+-+-+-+
```

## 개발 과정 및 학습 내용

제가 이 프로젝트를 진행하며 한 고민이나 생각은 [NOTE.md](./NOTE.md)에서 확인할 수 있습니다.

## 참고 자료

- [CHIP-8 Technical Reference](http://devernay.free.fr/hacks/chip8/C8TECH10.HTM)
- [Fixed Timestep Implementation](https://gafferongames.com/post/fix_your_timestep/)
- [Building a CHIP-8 Emulator [C++] - Austin Morlan](https://austinmorlan.com/posts/chip8_emulator/)
- [Latency Compensating Methods in Client/Server In-game Protocol Design](https://developer.valvesoftware.com/wiki/Latency_Compensating_Methods_in_Client/Server_In-game_Protocol_Design_and_Optimization)

## 라이센스

이 프로젝트는 MIT 라이센스 하에 배포됩니다. 자세한 내용은 [LICENSE](LICENSE) 파일을 참조하세요.
