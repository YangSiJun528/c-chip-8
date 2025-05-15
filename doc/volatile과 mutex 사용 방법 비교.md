# volatile과 mutex 사용 방법 비교

세 가지 접근 방식(volatile 만 사용, mutex만 사용, 둘 다 사용)에 대해 구현 방법과 장단점을 설명해 드리겠습니다.

## 1. volatile만 사용한 방법

### 구현
```c
// 전역 상태 변수
static struct {
    bool quit;
    errcode_t error_code;
    volatile uint8_t keypad[16]; // volatile 키워드 추가
} g_state = {
    .quit = false,
    .error_code = ERR_NONE,
    .keypad = {0}
};

// 키보드 입력 처리 스레드
void *keyboard_thread(void *arg) {
    while (!g_state.quit) {
        char c;
        if (read(STDIN_FILENO, &c, 1) > 0) {
            const int key_idx = get_key_index(c);
            if (key_idx >= 0) {
                // 뮤텍스 없이 직접 변수 수정
                g_state.keypad[key_idx] = INPUT_TICK;
                log_trace("key pressed: %c (ASCII: %d)", c, (int)c);
            }
        }
    }
    return NULL;
}

// 메인 스레드에서 읽기
// Ex9E - SKP Vx 명령어 처리
const uint8_t vx = (opcode & 0x0F00) >> 8;
if (g_state.keypad[vx] > 0) { // 뮤텍스 없이 직접 읽기
    chip8.pc += 2;
}

// 메인 스레드에서 감소
for (int i = 0; i < 16; i++) {
    if (g_state.keypad[i] > 0) { // 뮤텍스 없이 직접 읽고 수정
        g_state.keypad[i]--;
    }
}
```

### 장점
1. **단순성**: 코드가 간결하고 구현이 쉽습니다.
2. **성능**: 락 획득/해제의 오버헤드가 없어 성능이 더 좋습니다.
3. **데드락 없음**: 뮤텍스를 사용하지 않으므로 데드락 위험이 없습니다.

### 단점
1. **원자성 부재**: `volatile`은 변수 접근의 원자성을 보장하지 않습니다. 특히 메인 스레드에서 감소 작업을 수행할 때, 읽고-수정하고-쓰는 연산이 원자적이지 않을 수 있습니다.
2. **메모리 순서 보장 없음**: 다른 CPU 코어에서 실행 중인 스레드 간에 메모리 순서가 보장되지 않습니다.
3. **가시성 문제**: CPU 캐시로 인해 한 스레드의 변경사항이 다른 스레드에 즉시 보이지 않을 수 있습니다.

## 2. mutex만 사용한 방법

### 구현
```c
static pthread_mutex_t input_mutex = PTHREAD_MUTEX_INITIALIZER;

// 전역 상태 변수
static struct {
    bool quit;
    errcode_t error_code;
    uint8_t keypad[16]; // volatile 없음
} g_state = {
    .quit = false,
    .error_code = ERR_NONE,
    .keypad = {0}
};

// 키보드 입력 처리 스레드
void *keyboard_thread(void *arg) {
    while (!g_state.quit) {
        char c;
        if (read(STDIN_FILENO, &c, 1) > 0) {
            const int key_idx = get_key_index(c);
            if (key_idx >= 0) {
                pthread_mutex_lock(&input_mutex);
                g_state.keypad[key_idx] = INPUT_TICK;
                pthread_mutex_unlock(&input_mutex);
                log_trace("key pressed: %c (ASCII: %d)", c, (int)c);
            }
        }
    }
    return NULL;
}

// 메인 스레드에서 읽기
// Ex9E - SKP Vx 명령어 처리
const uint8_t vx = (opcode & 0x0F00) >> 8;
pthread_mutex_lock(&input_mutex);
bool key_pressed = g_state.keypad[vx] > 0;
pthread_mutex_unlock(&input_mutex);
if (key_pressed) {
    chip8.pc += 2;
}

// 메인 스레드에서 감소
pthread_mutex_lock(&input_mutex);
for (int i = 0; i < 16; i++) {
    if (g_state.keypad[i] > 0) {
        g_state.keypad[i]--;
    }
}
pthread_mutex_unlock(&input_mutex);
```

### 장점
1. **원자성**: 뮤텍스는 임계 영역 내의 모든 연산이 원자적으로 수행되도록 보장합니다.
2. **메모리 일관성**: 뮤텍스는 메모리 순서와 가시성을 보장합니다.
3. **예측 가능한 동작**: 스레드 간 동작이 더 예측 가능하고 정의됩니다.

### 단점
1. **성능 오버헤드**: 락 획득/해제에 따른 오버헤드가 있어 성능이 저하될 수 있습니다.
2. **데드락 가능성**: 여러 뮤텍스를 사용하거나 락 획득/해제를 잘못 처리하면 데드락이 발생할 수 있습니다.
3. **복잡성 증가**: 코드가 더 복잡해지고 이해하기 어려울 수 있습니다.

## 3. volatile과 mutex 모두 사용한 방법

### 구현
```c
static pthread_mutex_t input_mutex = PTHREAD_MUTEX_INITIALIZER;

// 전역 상태 변수
static struct {
    bool quit;
    errcode_t error_code;
    volatile uint8_t keypad[16]; // volatile 키워드 추가
} g_state = {
    .quit = false,
    .error_code = ERR_NONE,
    .keypad = {0}
};

// 키보드 입력 처리 스레드
void *keyboard_thread(void *arg) {
    while (!g_state.quit) {
        char c;
        if (read(STDIN_FILENO, &c, 1) > 0) {
            const int key_idx = get_key_index(c);
            if (key_idx >= 0) {
                pthread_mutex_lock(&input_mutex);
                g_state.keypad[key_idx] = INPUT_TICK;
                pthread_mutex_unlock(&input_mutex);
                log_trace("key pressed: %c (ASCII: %d)", c, (int)c);
            }
        }
    }
    return NULL;
}

// 메인 스레드에서 읽기
// Ex9E - SKP Vx 명령어 처리
const uint8_t vx = (opcode & 0x0F00) >> 8;
pthread_mutex_lock(&input_mutex);
bool key_pressed = g_state.keypad[vx] > 0;
pthread_mutex_unlock(&input_mutex);
if (key_pressed) {
    chip8.pc += 2;
}

// 메인 스레드에서 감소
pthread_mutex_lock(&input_mutex);
for (int i = 0; i < 16; i++) {
    if (g_state.keypad[i] > 0) {
        g_state.keypad[i]--;
    }
}
pthread_mutex_unlock(&input_mutex);
```

### 장점
1. **이중 안전장치**: volatile은 컴파일러 최적화를 방지하고, 뮤텍스는 스레드 간 동기화를 제공합니다.
2. **명확한 의도**: 코드를 읽는 다른 개발자에게 이 변수가 스레드 간에 공유되고 동기화가 필요하다는 의도를 명확히 전달합니다.
3. **디버깅 용이성**: 컴파일러 최적화로 인한 문제를 줄여 디버깅이 더 쉬워집니다.

### 단점
1. **불필요한 중복**: 뮤텍스가 이미 충분한 동기화를 제공하므로 volatile은 불필요할 수 있습니다.
2. **혼란 가능성**: 일부 개발자는 volatile과 뮤텍스의 역할이 겹치는 것에 혼란을 느낄 수 있습니다.
3. **성능 저하**: 뮤텍스로 인한 성능 오버헤드는 여전히 존재합니다.
