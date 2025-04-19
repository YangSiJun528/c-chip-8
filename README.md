# C Chip-8

내 프로젝트 중 C언어로 작성된 첫번째 프로젝트인 Chip-8 에뮬레이터.

최대한 외부 자료를 참고하지 않고, 직접 구현해볼 예정.   
하더라도 chip-8 코드가 아니라 특정 기능 구현에 참고할만한 코드를 찾아볼 것.

프로젝트가 진행되면서 충분히 내용이 길어지기 전까진 README에 의사결정 과정이나 참고 자료를 작성함.

# 구현 기록

## 1. 타이머 구현하기

일단 여러가지 요소가 있지만, 중요하면서 구현방법이 생각나지 않는 타이머를 먼저 구현하기로 함.

### 1.1 뭘 찾아야 할까?

예전에 PUBG의 개발 블로그에서 FPS 관련된 내용을 찾아봤던 기억이 남.

- https://blog.naver.com/whitesky9618/221476284242 - 원본을 찾을 수 없음
- 서버에서 프레임 단위로 연산을 수행한다는 사실을 알았고, 그게 연산 Cycle과 비슷하다는 느낌을 받음.

근데 더 찾아보니까 약간 결이 다른듯

- https://youtu.be/YHswt4VCeJs?si=-CY9b1UaGzAbje4f
- https://developer.valvesoftware.com/wiki/Latency_Compensating_Methods_in_Client/Server_In-game_Protocol_Design_and_Optimization

많은 자료가 클라이언트/서버 간의 불일치를 해결하고 보정하는데, 중점이 맞춰있는듯  
나는 프레임을 어떻게 구현하는지가 궁금한건데 그건 많이 없음

게임 서버가 아니라, 클라이언트 쪽이 더 애뮬레이터 개발과 비슷한 것 같지만,   
다들 Unity를 사용하지, 직접 구현하는 쪽은 없는 것 같아서 다른 식으로 찾아봐야 할 듯.

### 1.2 Fixed Timestep, Variable Delta Time

더 조사해본 결과, 게임에서 연산을 수행하는 사이클을 크게 2가지 방식으로 구현하는 것 같음

- 용어 정리
  - Δt(델타 티, delta time): 시간 간격, 두 시점 사이에 경과한 시간
    - 캐릭터 움직임, 물리 시뮬레이션 등 모든 시간 기반 계산에 사용됨
    - 보통 프레임 간의 시간 차이로 구할 수 있음

1. Fixed Timestep (고정 Δt)
   - 매 업데이트마다 동일한 Δt(예: 1/60초)로 물리 시뮬레이션을 수행
   - 결정론(determinism)이 보장되어 네트워크 락스텝, 리플레이 기능 구현에 유리
   - 렌더링 프레임 레이트와 독립적이므로, 낮은 성능 환경에서 잔상(stuttering)이 발생하거나
     과도한 계산 부하가 생길 수 있음
2. Variable Delta Time (가변 Δt)
   - 매 프레임 실제 경과 시간(hires timer)을 Δt로 측정해 업데이트
   - 프레임 드롭 시 Δt가 커져 물리 속도가 보정되므로 속도 변화가 덜 눈에 띔
   - 그러나 Δt 변화에 민감하여 물리 안정성(충돌 터널링, 발산 등)이 저하되고,
     프레임 레이트에 따라 게임 플레이 “감”이 달라질 수 있음
3. Semi‑Fixed Timestep & Accumulator 패턴
   - Variable Δt의 불안정 문제를 해결하기 위해 최대 Δt를 정해두고,
     남은 프레임 시간을 여러 개의 고정 Δt 스텝으로 분할 처리
   - accumulator에 남는 시간을 보존해 다음 프레임에 반영하므로
     시뮬레이션 속도는 일정하게 유지하면서 렌더링 변동성을 흡수
   - (옵션) 잔여 시간에 대해 이전 상태와 현재 상태를 선형/구면 보간(interpolation)하여
     렌더링하면 부드러운 움직임을 얻을 수 있음

근데? Accumulator 패턴은 이해가 잘 못함. 

그리고 간단하게 구현하는데, 낮은 성능 환경을 생각하면서 개발하기는 좀 과하다고 생각함.  
Fixed Timestep를 사용해서 구현하기로 함.

- 참고 자료
    - https://www.reddit.com/r/gamedev/comments/uxvlg7/implementing_ticks_properly/
        - https://gafferongames.com/post/fix_your_timestep/
        - https://gamedev.stackexchange.com/questions/80400/creating-a-tickrate-class
            - game-tick이 Fixed Timestep과 대응되는 표현인 듯

### 1.3 구현방법 생각해보기

https://gafferongames.com/post/fix_your_timestep/ 를 참고하여 구현

```c
double t  = 0.0;            // 시뮬레이션용 내부 시간
double dt = 1.0 / 60.0;     // 한 스텝에 고정할 Δt (1/60초)

while ( !quit )
{
    something(&cpu)              // 에뮬레이터 동작 수행
    t += dt;                     // 내부 시간을 dt만큼 증가
    //TODO: 다음 틱 시각 계산
    //TODO: 남은 시간만큼 sleep, 이미 다음 틱의 시간을 초과한 경우는 고려하지 않음.
}
```

성능 나쁜건 고려 안하고, 남은 시간동안 대기.

이러면 컨텍스트 스위칭이나 Sleep에서 깨는 시간 등으로 변동(jitter)이 생길거 같은데, 
남은 시간이 N%(대충 10%?) 정도면 sleep에서 깨워서 busy‑wait 하는 방식도 괜찮을거 같음.

지금은 간단하게 sleep하는 식으로 갈듯

이럼 이제 시간 처리하는 걸 어떻게 구현할 지 찾아봐야 함.

### 1.4 시간 처리 방법 찾기

내가 알기로는 컴퓨터의 시간이 두 종류가 있는데,
- 단조 시간(CLOCK_MONOTONIC) 
- NTP 시간(CLOCK_REALTIME) 

근데 Chip-8에서는 단조 시간을 사용해서 쓰는게 맞음.   
짦은 시간에서 정확해야하고, 남들과 공유하는 시간이 아니니까  

그럼 time.h 중 어떤 기능을 사용해야 할까?

https://en.cppreference.com/w/c/chrono/clock 여기에서  
`clock_gettime(CLOCK_MONOTONIC, &s_timespec);`을 사용하면 될 듯?  
CLOCK_MONOTONIC이 단조 시간을 나타내는 ID임.

GPT 사용해서 [test_tick_cycle](./test_tick_cycle.c) 코드를 작성함.
이거 맞는지 체크하고, 직접 구현해보면 될 듯?

다음 틱 예약(next_tick) 누적으로 내부 시뮬레이션 시간은 
drift(누적된 시간 차이) 없이 dt만큼 정확히 증가하고.      
매 프레임 ±3ms 정도 jitter는 일반적인 유저레벨 환경에서 기대할 수 있는 정상 범위라고 함.   
이 근거를 좀 찾아봤지만, 논문같은거 찾아보는거 아니면 실제 jitter 값이 어느정도인지는 모를듯?

일단 간단한 게임에서 이정도 차이는 괜찮아보임.
경쟁 게임인 발로란트에서도 높은 수준의 게임에서 20ms 차이가 승패에 영향을 준다고 했는데, 3ms정도면 뭐
- https://technology.riotgames.com/news/peeking-valorants-netcode

근데 보면서 깨달은게, 로깅이 필요하긴 할 듯? Debug or Trace 에서만 활성화되는 걸로

### 1.5 알게 된 용어 정리

지터(Jitter)란?
- 지터는 주기적인 신호나 작업의 타이밍에서 발생하는 불규칙한 변동을 의미합니다. 
- 컴퓨팅 시스템에서는 정확한 시간 간격으로 실행되어야 하는 작업이 예상 시간보다 일찍 또는 늦게 실행되는 현상을 말합니다.

- 프로그램이 정확히 매 16.67ms(60Hz)마다 작업을 수행하도록 설계되었다면:
  - 지터가 없는 이상적인 상황: 항상 정확히 16.67ms 간격으로 실행
- 실제 상황(지터 있음): 16.5ms, 16.9ms, 16.3ms, 16.8ms 등 약간의 변동이 발생





# 참고자료

- https://en.wikipedia.org/wiki/CHIP-8
- https://en.wikipedia.org/wiki/Emulator
- https://tobiasvl.github.io/blog/write-a-chip-8-emulator/
- http://devernay.free.fr/hacks/chip8/C8TECH10.HTM
