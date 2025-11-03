# 드라이버 동적 등록 가이드
## Ultra Light Scheduler - 런타임 드라이버 등록 시스템

---

## 📋 목차
1. [동적 등록 시스템 개요](#동적-등록-시스템-개요)
2. [API 레퍼런스](#api-레퍼런스)
3. [드라이버 작성 가이드](#드라이버-작성-가이드)
4. [실전 예제](#실전-예제)
5. [고급 기능](#고급-기능)

---

## 동적 등록 시스템 개요

### 핵심 아이디어

**드라이버를 런타임에 동적으로 등록하여 유연한 시스템 구성을 가능하게 합니다.**

```
setup() {
  timer_init();
  
  // 드라이버 동적 등록
  driver_register("I2C_Sensor", i2c_init, i2c_task, 50);
  driver_register("UART", uart_init, uart_task, 10);
  driver_register("LED", led_init, led_task, 10);
}

loop() {
  driver_manager_run();  // 등록된 모든 드라이버 자동 실행
}
```

### 시스템 구조

```
┌─────────────────────────────────────────┐
│  Application (setup/loop)               │
└─────────────┬───────────────────────────┘
              │ driver_register()
              ↓
┌─────────────────────────────────────────┐
│  Driver Manager                         │
│  ┌─────────────────────────────────┐   │
│  │ Descriptor Table (동적)         │   │
│  │  [0] I2C_Sensor  - 50ms         │   │
│  │  [1] UART        - 10ms         │   │
│  │  [2] LED         - 10ms         │   │
│  │  [3] (empty)                    │   │
│  └─────────────────────────────────┘   │
└─────────────┬───────────────────────────┘
              │
              ↓
┌─────────────────────────────────────────┐
│  Scheduler (10ms/50ms 플래그)           │
└─────────────────────────────────────────┘
```

---

## API 레퍼런스

### 핵심 데이터 구조

```c
/* driver_manager.h */
#ifndef DRIVER_MANAGER_H
#define DRIVER_MANAGER_H

#include <stdint.h>

// 드라이버 초기화 함수 타입
typedef int (*driver_init_fn_t)(void);

// 드라이버 태스크 함수 타입
typedef void (*driver_task_fn_t)(void);

// 드라이버 디스크립터
typedef struct {
  const char*       name;         // 드라이버 이름 (디버깅용)
  driver_init_fn_t  init_fn;      // 초기화 함수 (NULL 가능)
  driver_task_fn_t  task_fn;      // 주기 태스크 함수 (NULL 가능)
  uint8_t           period_ms;    // 실행 주기 (10 or 50)
  uint8_t           enabled;      // 활성화 상태
  uint8_t           initialized;  // 초기화 완료 여부
} driver_descriptor_t;

#endif
```

### 공개 API

```c
/**
 * @brief 드라이버 등록
 * 
 * @param name        드라이버 이름 (최대 15자)
 * @param init_fn     초기화 함수 포인터 (NULL 가능)
 * @param task_fn     주기 태스크 함수 포인터 (NULL 가능)
 * @param period_ms   실행 주기 (10 또는 50)
 * 
 * @return 0: 성공, -1: 슬롯 부족, -2: 잘못된 파라미터, -3: 초기화 실패
 */
int driver_register(const char* name, 
                   driver_init_fn_t init_fn,
                   driver_task_fn_t task_fn,
                   uint8_t period_ms);

/**
 * @brief 드라이버 등록 해제
 * 
 * @param name  드라이버 이름
 * @return 0: 성공, -1: 찾을 수 없음
 */
int driver_unregister(const char* name);

/**
 * @brief 드라이버 활성화/비활성화
 * 
 * @param name     드라이버 이름
 * @param enable   true: 활성화, false: 비활성화
 * @return 0: 성공, -1: 찾을 수 없음
 */
int driver_set_enable(const char* name, bool enable);

/**
 * @brief 드라이버 매니저 실행 (loop에서 호출)
 * 
 * 10ms/50ms 플래그를 체크하여 해당 주기의 드라이버를 실행합니다.
 */
void driver_manager_run(void);

/**
 * @brief 등록된 드라이버 목록 출력 (디버깅용)
 */
void driver_manager_list(void);
```

---

## 구현 코드

### driver_manager.c

```c
/* driver_manager.c */
#include "driver_manager.h"
#include <string.h>
#include <Arduino.h>

// 최대 드라이버 수
#ifndef MAX_DRIVERS
#define MAX_DRIVERS 16
#endif

// 전역 드라이버 테이블
static driver_descriptor_t g_drivers[MAX_DRIVERS];
static int g_driver_count = 0;

// 외부 스케줄러 변수 (ultra_light_sched에서 제공)
extern volatile uint8_t g_flag_10ms;
extern volatile uint8_t g_flag_50ms;

// ===== 내부 함수 =====

static driver_descriptor_t* find_driver(const char* name)
{
  for (int i = 0; i < g_driver_count; i++) {
    if (strcmp(g_drivers[i].name, name) == 0) {
      return &g_drivers[i];
    }
  }
  return NULL;
}

// ===== 공개 API 구현 =====

int driver_register(const char* name,
                   driver_init_fn_t init_fn,
                   driver_task_fn_t task_fn,
                   uint8_t period_ms)
{
  // 파라미터 검증
  if (!name) {
    Serial.println(F("[DRV] ERROR: name is NULL"));
    return -2;
  }
  
  if (period_ms != 10 && period_ms != 50) {
    Serial.print(F("[DRV] ERROR: Invalid period "));
    Serial.println(period_ms);
    return -2;
  }
  
  // 슬롯 확인
  if (g_driver_count >= MAX_DRIVERS) {
    Serial.println(F("[DRV] ERROR: Driver slots full"));
    return -1;
  }
  
  // 중복 확인
  if (find_driver(name)) {
    Serial.print(F("[DRV] WARNING: Driver '"));
    Serial.print(name);
    Serial.println(F("' already registered"));
    return -2;
  }
  
  // 드라이버 등록
  driver_descriptor_t* drv = &g_drivers[g_driver_count];
  drv->name = name;
  drv->init_fn = init_fn;
  drv->task_fn = task_fn;
  drv->period_ms = period_ms;
  drv->enabled = 0;        // 기본 비활성
  drv->initialized = 0;
  
  Serial.print(F("[DRV] Registering '"));
  Serial.print(name);
  Serial.print(F("' @ "));
  Serial.print(period_ms);
  Serial.print(F("ms"));
  
  // 초기화 함수 실행
  if (init_fn) {
    int ret = init_fn();
    if (ret != 0) {
      Serial.print(F(" - Init FAILED ("));
      Serial.print(ret);
      Serial.println(F(")"));
      return -3;
    }
    drv->initialized = 1;
  }
  
  // 등록 완료 후 자동 활성화
  drv->enabled = 1;
  g_driver_count++;
  
  Serial.println(F(" - OK"));
  return 0;
}

int driver_unregister(const char* name)
{
  for (int i = 0; i < g_driver_count; i++) {
    if (strcmp(g_drivers[i].name, name) == 0) {
      // 배열에서 제거 (뒤에 있는 것들을 앞으로 이동)
      for (int j = i; j < g_driver_count - 1; j++) {
        g_drivers[j] = g_drivers[j + 1];
      }
      g_driver_count--;
      
      Serial.print(F("[DRV] Unregistered '"));
      Serial.print(name);
      Serial.println(F("'"));
      return 0;
    }
  }
  
  return -1;
}

int driver_set_enable(const char* name, bool enable)
{
  driver_descriptor_t* drv = find_driver(name);
  if (!drv) return -1;
  
  drv->enabled = enable ? 1 : 0;
  
  Serial.print(F("[DRV] '"));
  Serial.print(name);
  Serial.print(F("' "));
  Serial.println(enable ? F("ENABLED") : F("DISABLED"));
  
  return 0;
}

void driver_manager_run(void)
{
  // 10ms 태스크 실행
  if (g_flag_10ms) {
    g_flag_10ms = 0;
    
    for (int i = 0; i < g_driver_count; i++) {
      driver_descriptor_t* drv = &g_drivers[i];
      if (drv->enabled && drv->task_fn && drv->period_ms == 10) {
        drv->task_fn();
      }
    }
  }
  
  // 50ms 태스크 실행
  if (g_flag_50ms) {
    g_flag_50ms = 0;
    
    for (int i = 0; i < g_driver_count; i++) {
      driver_descriptor_t* drv = &g_drivers[i];
      if (drv->enabled && drv->task_fn && drv->period_ms == 50) {
        drv->task_fn();
      }
    }
  }
}

void driver_manager_list(void)
{
  Serial.println(F("\n===== Driver List ====="));
  Serial.print(F("Total: "));
  Serial.print(g_driver_count);
  Serial.print(F(" / "));
  Serial.println(MAX_DRIVERS);
  
  for (int i = 0; i < g_driver_count; i++) {
    driver_descriptor_t* drv = &g_drivers[i];
    
    Serial.print(F("["));
    Serial.print(i);
    Serial.print(F("] "));
    Serial.print(drv->name);
    Serial.print(F(" - "));
    Serial.print(drv->period_ms);
    Serial.print(F("ms - "));
    Serial.print(drv->enabled ? F("ENABLED") : F("DISABLED"));
    Serial.print(F(" - "));
    Serial.println(drv->initialized ? F("INIT OK") : F("NO INIT"));
  }
  
  Serial.println(F("=======================\n"));
}
```

---

## 드라이버 작성 가이드

### 템플릿 구조

```c
/* my_driver.h */
#ifndef MY_DRIVER_H
#define MY_DRIVER_H

// 초기화 함수 (필수)
int my_driver_init(void);

// 태스크 함수 (선택)
void my_driver_task(void);

// 사용자 API (선택)
void my_driver_do_something(void);

#endif
```

```c
/* my_driver.c */
#include "my_driver.h"
#include <Arduino.h>

// 드라이버 내부 상태
static struct {
  uint8_t initialized;
  uint32_t counter;
  // ... 필요한 상태 변수들
} drv_state;

int my_driver_init(void)
{
  // 하드웨어 초기화
  pinMode(LED_BUILTIN, OUTPUT);
  
  // 상태 초기화
  drv_state.initialized = 1;
  drv_state.counter = 0;
  
  Serial.println(F("[MyDriver] Initialized"));
  return 0;  // 성공 시 0 반환
}

void my_driver_task(void)
{
  // 주기적으로 실행될 작업
  drv_state.counter++;
  
  if (drv_state.counter % 100 == 0) {  // 100번마다
    Serial.print(F("[MyDriver] Counter: "));
    Serial.println(drv_state.counter);
  }
}

void my_driver_do_something(void)
{
  // 사용자가 호출할 수 있는 함수
  digitalWrite(LED_BUILTIN, HIGH);
}
```

### 드라이버 등록 방법

```c
/* main.ino */
#include "my_driver.h"

void setup()
{
  Serial.begin(57600);
  while (!Serial) { ; }
  
  timer2_setup_1ms();  // 스케줄러 초기화
  
  // 드라이버 등록
  driver_register("MyDriver", my_driver_init, my_driver_task, 10);
  //                 ↑이름      ↑초기화        ↑태스크          ↑10ms 주기
  
  driver_manager_list();  // 등록 확인
}

void loop()
{
  work_run_due(g_tick_ms);  // 워크 스케줄러
  driver_manager_run();      // 드라이버 매니저
}
```

---

## 실전 예제

### 예제 1: LED 깜빡임 드라이버

```c
/* led_driver.h */
#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#include <stdint.h>

int led_driver_init(void);
void led_driver_task(void);
void led_set_blink_rate(uint16_t rate_ms);

#endif
```

```c
/* led_driver.c */
#include "led_driver.h"
#include <Arduino.h>

extern volatile uint32_t g_tick_ms;

static struct {
  uint16_t blink_rate_ms;
  uint32_t last_toggle_ms;
  uint8_t state;
} led_ctx;

int led_driver_init(void)
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  led_ctx.blink_rate_ms = 500;  // 기본 500ms
  led_ctx.last_toggle_ms = 0;
  led_ctx.state = 0;
  
  Serial.println(F("[LED] Init OK"));
  return 0;
}

void led_driver_task(void)
{
  // 10ms마다 호출됨
  uint32_t now = g_tick_ms;
  
  if (now - led_ctx.last_toggle_ms >= led_ctx.blink_rate_ms) {
    led_ctx.last_toggle_ms = now;
    led_ctx.state = !led_ctx.state;
    digitalWrite(LED_BUILTIN, led_ctx.state);
  }
}

void led_set_blink_rate(uint16_t rate_ms)
{
  led_ctx.blink_rate_ms = rate_ms;
  Serial.print(F("[LED] Blink rate: "));
  Serial.println(rate_ms);
}
```

**등록:**
```c
void setup() {
  Serial.begin(57600);
  timer2_setup_1ms();
  
  driver_register("LED", led_driver_init, led_driver_task, 10);
  
  // 실행 중에 깜빡임 속도 변경
  delay(2000);
  led_set_blink_rate(100);  // 100ms로 변경
}
```

---

### 예제 2: 버튼 디바운스 드라이버

```c
/* button_driver.h */
#ifndef BUTTON_DRIVER_H
#define BUTTON_DRIVER_H

#include <stdint.h>

typedef void (*button_callback_t)(uint8_t button_id, uint8_t pressed);

int button_driver_init(void);
void button_driver_task(void);
void button_register_callback(button_callback_t cb);

#endif
```

```c
/* button_driver.c */
#include "button_driver.h"
#include <Arduino.h>

#define BUTTON_PIN 2
#define DEBOUNCE_COUNT 3

static struct {
  uint8_t raw_state;
  uint8_t stable_state;
  uint8_t count;
  button_callback_t callback;
} btn_ctx;

int button_driver_init(void)
{
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  btn_ctx.raw_state = HIGH;
  btn_ctx.stable_state = HIGH;
  btn_ctx.count = 0;
  btn_ctx.callback = NULL;
  
  Serial.println(F("[BTN] Init OK"));
  return 0;
}

void button_driver_task(void)
{
  // 10ms마다 호출
  uint8_t current = digitalRead(BUTTON_PIN);
  
  if (current == btn_ctx.raw_state) {
    if (btn_ctx.count < DEBOUNCE_COUNT) {
      btn_ctx.count++;
      
      if (btn_ctx.count == DEBOUNCE_COUNT) {
        // 안정화됨
        if (current != btn_ctx.stable_state) {
          btn_ctx.stable_state = current;
          
          // 콜백 호출
          if (btn_ctx.callback) {
            btn_ctx.callback(0, current == LOW ? 1 : 0);
          }
          
          Serial.print(F("[BTN] "));
          Serial.println(current == LOW ? F("PRESSED") : F("RELEASED"));
        }
      }
    }
  } else {
    btn_ctx.raw_state = current;
    btn_ctx.count = 0;
  }
}

void button_register_callback(button_callback_t cb)
{
  btn_ctx.callback = cb;
}
```

**사용:**
```c
void on_button_event(uint8_t button_id, uint8_t pressed)
{
  if (pressed) {
    Serial.println("Button pressed!");
    led_set_blink_rate(100);  // 빠르게
  } else {
    Serial.println("Button released!");
    led_set_blink_rate(500);  // 느리게
  }
}

void setup() {
  Serial.begin(57600);
  timer2_setup_1ms();
  
  driver_register("LED", led_driver_init, led_driver_task, 10);
  driver_register("Button", button_driver_init, button_driver_task, 10);
  
  button_register_callback(on_button_event);
}
```

---

### 예제 3: ADC 센서 드라이버

```c
/* adc_driver.h */
#ifndef ADC_DRIVER_H
#define ADC_DRIVER_H

#include <stdint.h>

typedef struct {
  uint16_t raw;
  float voltage;
  uint32_t timestamp_ms;
} adc_data_t;

int adc_driver_init(void);
void adc_driver_task(void);
const adc_data_t* adc_get_data(void);

#endif
```

```c
/* adc_driver.c */
#include "adc_driver.h"
#include <Arduino.h>

extern volatile uint32_t g_tick_ms;

#define ADC_PIN A0

static adc_data_t adc_data;

int adc_driver_init(void)
{
  pinMode(ADC_PIN, INPUT);
  
  adc_data.raw = 0;
  adc_data.voltage = 0.0f;
  adc_data.timestamp_ms = 0;
  
  Serial.println(F("[ADC] Init OK"));
  return 0;
}

void adc_driver_task(void)
{
  // 50ms마다 호출
  adc_data.raw = analogRead(ADC_PIN);
  adc_data.voltage = (adc_data.raw * 5.0f) / 1024.0f;
  adc_data.timestamp_ms = g_tick_ms;
  
  static uint8_t log_count = 0;
  if (++log_count >= 20) {  // 1초마다 로그 (50ms * 20)
    log_count = 0;
    Serial.print(F("[ADC] "));
    Serial.print(adc_data.voltage);
    Serial.println(F(" V"));
  }
}

const adc_data_t* adc_get_data(void)
{
  return &adc_data;
}
```

**등록:**
```c
void setup() {
  Serial.begin(57600);
  timer2_setup_1ms();
  
  driver_register("ADC", adc_driver_init, adc_driver_task, 50);
  //                                                        ↑ 50ms 주기
}

void loop() {
  work_run_due(g_tick_ms);
  driver_manager_run();
  
  // 필요할 때 데이터 읽기
  const adc_data_t* data = adc_get_data();
  // data->voltage 사용
}
```

---

## 고급 기능

### 1. 런타임 활성화/비활성화

```c
void some_function() {
  // LED 드라이버 일시 중지
  driver_set_enable("LED", false);
  
  delay(1000);
  
  // 다시 활성화
  driver_set_enable("LED", true);
}
```

### 2. 드라이버 간 통신

```c
/* 공유 데이터 구조 */
typedef struct {
  float temperature;
  uint8_t button_pressed;
} shared_data_t;

extern shared_data_t g_shared_data;

/* temperature_driver.c */
void temp_driver_task(void) {
  g_shared_data.temperature = read_temperature();
}

/* led_driver.c */
void led_driver_task(void) {
  // 온도가 높으면 빨리 깜빡임
  if (g_shared_data.temperature > 30.0f) {
    led_set_blink_rate(100);
  }
}
```

### 3. 초기화 없이 등록 (지연 초기화)

```c
// 초기화 함수 없이 등록
driver_register("SomeDriver", NULL, some_task, 10);

// 나중에 수동으로 초기화
some_driver_init();
```

### 4. 태스크 없이 등록 (이벤트 기반)

```c
// 주기 태스크 없이 등록 (초기화만)
driver_register("EventDriver", event_driver_init, NULL, 10);

// 이벤트 발생 시 워크로 처리
work_schedule_after(event_handler, NULL, 100);
```

---

## 완전한 예제

```c
/* full_example.ino */
#include "driver_manager.h"
#include "led_driver.h"
#include "button_driver.h"
#include "adc_driver.h"

void on_button(uint8_t id, uint8_t pressed)
{
  if (pressed) {
    const adc_data_t* adc = adc_get_data();
    Serial.print("ADC: ");
    Serial.println(adc->voltage);
  }
}

void setup()
{
  Serial.begin(57600);
  while (!Serial) { ; }
  
  Serial.println(F("\n========== System Start =========="));
  
  // 스케줄러 초기화
  timer2_setup_1ms();
  
  // 드라이버 등록
  driver_register("LED", led_driver_init, led_driver_task, 10);
  driver_register("Button", button_driver_init, button_driver_task, 10);
  driver_register("ADC", adc_driver_init, adc_driver_task, 50);
  
  // 콜백 설정
  button_register_callback(on_button);
  
  // 등록된 드라이버 목록 출력
  driver_manager_list();
  
  Serial.println(F("========== Ready ==========\n"));
}

void loop()
{
  work_run_due(g_tick_ms);
  driver_manager_run();  // 모든 드라이버 자동 실행
}
```

---

## 체크리스트

### 새 드라이버 추가 시

1. [ ] `xxx_driver.h` 파일 생성
2. [ ] `int xxx_driver_init(void)` 구현
3. [ ] `void xxx_driver_task(void)` 구현 (필요 시)
4. [ ] `setup()`에서 `driver_register()` 호출
5. [ ] 주기 선택 (10ms or 50ms)
6. [ ] 테스트 및 검증

### 디버깅 팁

```c
// 드라이버 목록 확인
driver_manager_list();

// 특정 드라이버 비활성화
driver_set_enable("문제있는드라이버", false);

// 개별 드라이버 테스트
void loop() {
  // driver_manager_run() 대신
  my_driver_task();  // 직접 호출
}
```

---

## 요약

**3단계로 드라이버 추가:**

1. **작성**: `init()`, `task()` 함수 구현
2. **등록**: `setup()`에서 `driver_register()` 호출
3. **실행**: `loop()`의 `driver_manager_run()`이 자동 실행

**끝!** 🎉