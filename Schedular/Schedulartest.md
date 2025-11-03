# Ultra Light Scheduler - API 문서 및 튜토리얼

## 📋 목차
1. [개요](#개요)
2. [아키텍처](#아키텍처)
3. [API 레퍼런스](#api-레퍼런스)
4. [튜토리얼](#튜토리얼)
5. [실전 예제](#실전-예제)
6. [최적화 팁](#최적화-팁)

---

## 개요

Ultra Light Scheduler는 Arduino용 경량 협력형 멀티태스킹 시스템입니다. RTOS 없이도 복잡한 타이밍 요구사항을 처리할 수 있습니다.

### 주요 특징
- ✅ 하드웨어 Timer2 기반 정확한 1ms 틱
- ✅ 10ms/50ms 주기 태스크 지원
- ✅ 동적 워크 스케줄링 (원샷/반복)
- ✅ 최대 8개 동시 워크 실행
- ✅ 32비트 타임스탬프 래핑 안전

### 시스템 요구사항
- Arduino Uno, Nano 등 ATmega328P 기반 보드
- Timer2 사용 가능 (PWM 핀 3, 11 사용 불가)
- 메모리: RAM 약 100바이트 사용

---

## 아키텍처

### 타이밍 계층 구조

```
Hardware Timer2 (1ms ISR)
    ↓
g_tick_ms (전역 카운터)
    ↓
┌─────────────┬──────────────┬────────────────┐
│  10ms Flag  │  50ms Flag   │  Work Queue    │
│             │              │  (동적)        │
└─────────────┴──────────────┴────────────────┘
    ↓              ↓               ↓
[10ms 태스크]  [50ms 태스크]  [원샷/반복 워크]
```

### 실행 흐름

```cpp
setup()
  └─> timer2_setup_1ms()  // 타이머 초기화
  └─> 초기 워크 등록

loop() (무한 반복)
  ├─> work_run_due()      // 만기된 워크 실행
  ├─> 10ms 플래그 체크
  │    └─> 10ms 태스크들 순차 실행
  └─> 50ms 플래그 체크
       └─> 50ms 태스크들 순차 실행
```

---

## API 레퍼런스

### 1. 타임베이스 API

#### `g_tick_ms`
```cpp
volatile uint32_t g_tick_ms;
```
- **설명**: 부팅 후 경과 시간(밀리초)
- **주기**: 약 49.7일마다 래핑 (2^32 ms)
- **사용**: 현재 시각 확인, 타임스탬프 기록

**예제:**
```cpp
uint32_t start = g_tick_ms;
do_something();
uint32_t elapsed = g_tick_ms - start;
Serial.print("Elapsed: ");
Serial.println(elapsed);
```

---

### 2. 워크 스케줄러 API

#### `work_schedule_after()`
```cpp
work_t* work_schedule_after(work_fn_t fn, void* arg, uint32_t delay_ms);
```

**파라미터:**
- `fn`: 실행할 콜백 함수 (시그니처: `void callback(void* arg)`)
- `arg`: 콜백에 전달할 사용자 데이터 (NULL 가능)
- `delay_ms`: 현재 시각부터 지연 시간 (밀리초)

**반환값:**
- 성공: `work_t*` 핸들 (취소 시 사용)
- 실패: `NULL` (워크 큐 가득 참)

**용도:** 특정 시간 후 1회 실행

**예제:**
```cpp
void turn_off_motor(void* arg) {
  digitalWrite(MOTOR_PIN, LOW);
  Serial.println("Motor OFF");
}

void emergency_stop() {
  digitalWrite(MOTOR_PIN, HIGH);
  Serial.println("Motor ON for 500ms");
  
  // 500ms 후 자동 정지
  work_schedule_after(turn_off_motor, NULL, 500);
}
```

---

#### `work_schedule_at()`
```cpp
work_t* work_schedule_at(work_fn_t fn, void* arg, uint32_t abs_ms);
```

**파라미터:**
- `fn`: 실행할 콜백 함수
- `arg`: 사용자 데이터
- `abs_ms`: 절대 시각 (`g_tick_ms` 기준)

**반환값:** `work_t*` 또는 `NULL`

**용도:** 정확한 절대 시각에 실행

**예제:**
```cpp
void alarm_callback(void* arg) {
  Serial.println("ALARM!");
  digitalWrite(BUZZER_PIN, HIGH);
}

void set_alarm(uint32_t target_ms) {
  work_schedule_at(alarm_callback, NULL, target_ms);
}

// 현재 시각 기준 10초 후 알람
uint32_t alarm_time = g_tick_ms + 10000;
set_alarm(alarm_time);
```

---

#### `work_schedule_repeat()`
```cpp
work_t* work_schedule_repeat(work_fn_t fn, void* arg, 
                             uint32_t first_after_ms, uint16_t period_ms);
```

**파라미터:**
- `fn`: 실행할 콜백 함수
- `arg`: 사용자 데이터
- `first_after_ms`: 첫 실행까지 지연 시간
- `period_ms`: 반복 주기 (1~65535ms)

**반환값:** `work_t*` 또는 `NULL`

**용도:** 주기적 작업 (센서 폴링, 하트비트 등)

**예제:**
```cpp
void heartbeat(void* arg) {
  static bool state = false;
  digitalWrite(LED_PIN, state);
  state = !state;
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  
  // 즉시 시작, 500ms마다 토글
  work_schedule_repeat(heartbeat, NULL, 0, 500);
}
```

---

#### `work_cancel()`
```cpp
void work_cancel(work_t* w);
```

**파라미터:**
- `w`: 취소할 워크 핸들 (`work_schedule_*` 반환값)

**용도:** 예약된 워크 취소

**예제:**
```cpp
work_t* timeout_handle = NULL;

void start_operation() {
  do_something();
  
  // 5초 타임아웃 설정
  timeout_handle = work_schedule_after(timeout_handler, NULL, 5000);
}

void operation_completed() {
  // 정상 완료 시 타임아웃 취소
  work_cancel(timeout_handle);
  timeout_handle = NULL;
}

void timeout_handler(void* arg) {
  Serial.println("ERROR: Timeout!");
  timeout_handle = NULL;
}
```

---

### 3. 주기 태스크 시스템

#### 10ms 태스크 추가

```cpp
// 1. 태스크 함수 작성
static void my_10ms_task(void) {
  // 10ms마다 실행될 코드
}

// 2. 태스크 배열에 추가
static const task_fn_t g_tasks_10ms[] = { 
  t10_errb, 
  t10_led,
  my_10ms_task  // 추가
};
```

#### 50ms 태스크 추가

```cpp
// 1. 태스크 함수 작성
static void my_50ms_task(void) {
  // 50ms마다 실행될 코드
}

// 2. 태스크 배열에 추가
static const task_fn_t g_tasks_50ms[] = { 
  t50_adc, 
  t50_log,
  my_50ms_task  // 추가
};
```

**주의사항:**
- 태스크는 블로킹 없이 빠르게 반환해야 함
- 각 태스크는 1~2ms 이내 실행 권장
- `delay()` 절대 사용 금지

---

## 튜토리얼

### 튜토리얼 1: LED 깜빡이기 (기본)

```cpp
void blink_callback(void* arg) {
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  timer2_setup_1ms();
  
  // 500ms마다 토글
  work_schedule_repeat(blink_callback, NULL, 0, 500);
}

void loop() {
  work_run_due(g_tick_ms);
}
```

---

### 튜토리얼 2: 버튼으로 LED 3초간 켜기

```cpp
const uint8_t BUTTON_PIN = 2;
const uint8_t LED_PIN = 13;

void led_off(void* arg) {
  digitalWrite(LED_PIN, LOW);
  Serial.println("LED OFF");
}

void button_handler(void) {
  static uint8_t last_state = HIGH;
  uint8_t state = digitalRead(BUTTON_PIN);
  
  if (last_state == HIGH && state == LOW) {  // 버튼 눌림
    digitalWrite(LED_PIN, HIGH);
    Serial.println("LED ON");
    
    // 3초 후 자동 OFF
    work_schedule_after(led_off, NULL, 3000);
  }
  
  last_state = state;
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(57600);
  
  timer2_setup_1ms();
  work_schedule_repeat((work_fn_t)button_handler, NULL, 0, 10);  // 10ms마다 체크
}

void loop() {
  work_run_due(g_tick_ms);
}
```

---

### 튜토리얼 3: 온도 센서 5분 간격 로깅

```cpp
void log_temperature(void* arg) {
  int raw = analogRead(A0);
  float temp = (raw * 5.0 / 1024.0 - 0.5) * 100.0;  // TMP36 센서
  
  Serial.print("Temperature: ");
  Serial.print(temp);
  Serial.println(" C");
}

void setup() {
  Serial.begin(57600);
  timer2_setup_1ms();
  
  // 5분 = 300,000ms
  work_schedule_repeat(log_temperature, NULL, 0, 300000UL);
}

void loop() {
  work_run_due(g_tick_ms);
}
```

---

### 튜토리얼 4: 시퀀스 제어 (신호등)

```cpp
const uint8_t RED = 3, YELLOW = 4, GREEN = 5;

void set_yellow(void* arg);
void set_red(void* arg);
void set_green(void* arg);

void set_green(void* arg) {
  digitalWrite(RED, LOW);
  digitalWrite(YELLOW, LOW);
  digitalWrite(GREEN, HIGH);
  Serial.println("GREEN");
  
  work_schedule_after(set_yellow, NULL, 5000);  // 5초 후 노랑
}

void set_yellow(void* arg) {
  digitalWrite(GREEN, LOW);
  digitalWrite(YELLOW, HIGH);
  Serial.println("YELLOW");
  
  work_schedule_after(set_red, NULL, 2000);  // 2초 후 빨강
}

void set_red(void* arg) {
  digitalWrite(YELLOW, LOW);
  digitalWrite(RED, HIGH);
  Serial.println("RED");
  
  work_schedule_after(set_green, NULL, 5000);  // 5초 후 초록
}

void setup() {
  pinMode(RED, OUTPUT);
  pinMode(YELLOW, OUTPUT);
  pinMode(GREEN, OUTPUT);
  Serial.begin(57600);
  
  timer2_setup_1ms();
  set_green(NULL);  // 초록으로 시작
}

void loop() {
  work_run_due(g_tick_ms);
}
```

---

## 실전 예제

### 예제 1: 센서 폴링 + 임계값 알람

```cpp
const int TEMP_THRESHOLD = 50;  // 50도
work_t* alarm_work = NULL;

void check_sensor(void* arg) {
  int raw = analogRead(A0);
  float temp = (raw * 5.0 / 1024.0 - 0.5) * 100.0;
  
  if (temp > TEMP_THRESHOLD && !alarm_work) {
    Serial.println("ALARM: High temperature!");
    // 알람 10초간 지속
    alarm_work = work_schedule_after(stop_alarm, NULL, 10000);
    digitalWrite(BUZZER_PIN, HIGH);
  }
}

void stop_alarm(void* arg) {
  digitalWrite(BUZZER_PIN, LOW);
  alarm_work = NULL;
}

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.begin(57600);
  timer2_setup_1ms();
  
  // 100ms마다 센서 체크
  work_schedule_repeat(check_sensor, NULL, 0, 100);
}
```

---

### 예제 2: 모터 소프트 스타트

```cpp
typedef struct {
  uint8_t current_speed;
  uint8_t target_speed;
} motor_ctx_t;

void ramp_motor(void* arg) {
  motor_ctx_t* ctx = (motor_ctx_t*)arg;
  
  if (ctx->current_speed < ctx->target_speed) {
    ctx->current_speed += 5;  // 5씩 증가
    if (ctx->current_speed > ctx->target_speed) {
      ctx->current_speed = ctx->target_speed;
    }
  }
  
  analogWrite(MOTOR_PIN, ctx->current_speed);
  Serial.print("Speed: ");
  Serial.println(ctx->current_speed);
  
  if (ctx->current_speed < ctx->target_speed) {
    work_schedule_after(ramp_motor, arg, 50);  // 50ms 후 다시
  }
}

motor_ctx_t motor = {0, 0};

void start_motor(uint8_t speed) {
  motor.target_speed = speed;
  ramp_motor(&motor);
}

void setup() {
  pinMode(MOTOR_PIN, OUTPUT);
  Serial.begin(57600);
  timer2_setup_1ms();
  
  delay(1000);
  start_motor(255);  // 천천히 최대 속도로
}
```

---

### 예제 3: 통신 타임아웃 처리

```cpp
work_t* rx_timeout = NULL;

void uart_timeout_handler(void* arg) {
  Serial.println("ERROR: UART timeout");
  rx_timeout = NULL;
  // 에러 처리...
}

void start_uart_transaction() {
  Serial.println("Waiting for response...");
  
  // 500ms 타임아웃 설정
  rx_timeout = work_schedule_after(uart_timeout_handler, NULL, 500);
}

void on_uart_received() {
  // 데이터 수신 시 타임아웃 취소
  if (rx_timeout) {
    work_cancel(rx_timeout);
    rx_timeout = NULL;
    Serial.println("Response received");
  }
}
```

---

## 최적화 팁

### 1. 워크 큐 크기 조정

```cpp
// 기본값 8개 대신 16개로 확장
#define WORK_CAP 16
#include "ultra_light_sched_arduino_fixed.ino"
```

### 2. 인터럽트 안전성

```cpp
// 공유 변수 읽기 시 원자성 보장
uint32_t get_tick_safe() {
  uint32_t tick;
  noInterrupts();
  tick = g_tick_ms;
  interrupts();
  return tick;
}
```

### 3. 메모리 절약

```cpp
// 사용자 데이터를 구조체 대신 정수로 전달
void my_callback(void* arg) {
  int value = (int)arg;  // 포인터를 정수로 해석
  Serial.println(value);
}

work_schedule_after(my_callback, (void*)42, 1000);
```

### 4. 디버깅 헬퍼

```cpp
void print_work_queue() {
  Serial.println("=== Work Queue ===");
  for (int i = 0; i < WORK_CAP; i++) {
    if (s_workq[i].active) {
      Serial.print("Slot ");
      Serial.print(i);
      Serial.print(": due=");
      Serial.print(s_workq[i].next_due_ms);
      Serial.print(", mode=");
      Serial.println(s_workq[i].mode == WORK_ONESHOT ? "ONESHOT" : "REPEAT");
    }
  }
}
```

### 5. 타이밍 측정

```cpp
void profile_task() {
  uint32_t start = g_tick_ms;
  
  my_heavy_task();
  
  uint32_t elapsed = g_tick_ms - start;
  if (elapsed > 5) {  // 5ms 이상 걸리면 경고
    Serial.print("WARNING: Task took ");
    Serial.print(elapsed);
    Serial.println("ms");
  }
}
```

---

## FAQ

**Q: Timer2를 사용하면 PWM 핀 3, 11을 못 쓰나요?**  
A: 네, Timer2가 PWM을 담당하므로 사용 불가합니다. 대신 핀 5, 6, 9, 10을 사용하세요.

**Q: `delay()`를 쓰면 안 되나요?**  
A: 절대 안 됩니다. `delay()`는 모든 태스크를 블로킹합니다. 워크 스케줄러를 사용하세요.

**Q: 워크 큐가 가득 차면?**  
A: `work_schedule_*()` 함수가 `NULL`을 반환합니다. 반환값을 체크하세요.

**Q: 49일 후 래핑되면 문제가 생기나요?**  
A: 아니요, `time_after_eq()` 함수가 래핑을 안전하게 처리합니다.

**Q: ISR 안에서 워크를 스케줄할 수 있나요?**  
A: 가능하지만 권장하지 않습니다. ISR은 최소한의 작업만 수행해야 합니다.

---

## 라이센스 및 기여

이 코드는 교육 목적으로 제공됩니다. 자유롭게 수정하여 사용하세요.

**버그 리포트:** 이슈 발생 시 시리얼 로그와 함께 보고해주세요.

**최종 업데이트:** 2024