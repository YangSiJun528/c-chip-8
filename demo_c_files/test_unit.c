/*
 * 간단한 코드 테스트할 때 쓰는 코드, 검증용이나 구현 잘 되어있는지 체크하는데 씀.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <unistd.h>

// uint8_t 값을 8비트 이진수로 출력하는 함수
void print_binary_u8(uint8_t num) {
    for (int i = 7; i >= 0; i--) {
        // 상위 비트(MSB)부터 출력
        putchar((num & (1 << i)) ? '1' : '0');
    }
}

// uint16_t 값을 16비트 이진수로 출력하는 함수
void print_binary_u16(uint16_t num) {
    for (int i = 15; i >= 0; i--) {
        putchar((num & (1 << i)) ? '1' : '0');
    }
}

static void test_join_uint8_t(void) {
    const uint8_t a = 15; // 0000 1111
    const uint8_t b = 8; // 0000 1000
    const uint16_t c = (a << 8) | b; // 0000 1111 0000 1000 (15 << 8 + 8)

    printf("a (DEC)=%" PRIu8 ", a (BIN)=", a);
    print_binary_u8(a);
    printf("\n");

    printf("b (DEC)=%" PRIu8 ", b (BIN)=", b);
    print_binary_u8(b);
    printf("\n");

    printf("c (DEC)=%" PRIu16 ", c (BIN)=", c);
    print_binary_u16(c);
    printf("\n");
}

static void test_Nxkk(void) {
    const uint16_t opcode = 0x3122;
    const uint8_t vx = (opcode & 0x0F00) >> 8;
    const uint8_t nn = (opcode & 0x00FF); // 사실 & 없어도 될거같긴 함.
    printf("vx=%" PRIu8 ", nn=%" PRIu8 "\n", vx, nn);
}

static void test_NxyN(void) {
    const uint16_t opcode = 0x3122;
    const uint8_t vx = (opcode & 0x0F00) >> 8;
    const uint8_t vy = (opcode & 0x00F0) >> 4;
    printf("vx=%" PRIu8 ", vy=%" PRIu8 "\n", vx, vy);
}

int main(void) {
    test_join_uint8_t();
    test_Nxkk();
    test_NxyN();
    return 0;
}
