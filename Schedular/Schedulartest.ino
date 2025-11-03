/* ultra_light_sched_arduino_fixed.ino
 * - Timer2 CTC 1ms ISR → 10ms/50ms flags
 * - 원샷/리핏 워크 스케줄러
 * - Arduino 자동 프로토타입 이슈 회피 (타입/프로토타입을 최상단에 선언)
 */
#include <Arduino.h>
#include <stdint.h>

/* ===== 타입/프로토타입을 최상단에 둔다 ===== */

/* 래핑 안전 비교 */
static inline bool time_after_eq(uint32_t a, uint32_t b) {
  return (int32_t)(a - b) >= 0;
}

/* 워크 스케줄러 타입들 */
typedef void (*work_fn_t)(void *arg);
typedef enum { WORK_ONESHOT = 0, WORK_REPEAT = 1 } work_mode_t;

typedef struct {
  work_fn_t fn;
  void*     arg;
  uint32_t  next_due_ms;   // 만기 시각(절대)
  uint16_t  period_ms;     // REPEAT일 때만 사용
  uint8_t   mode;          // ONESHOT / REPEAT
  uint8_t   active;        // 1=활성
} work_t;

#ifndef WORK_CAP
#define WORK_CAP 8
#endif

/* 전방 선언(프로토타입) — Arduino의 자동 프로토타입보다 먼저! */
static void timer2_setup_1ms(void);
static void work_run_due(uint32_t now_ms);
static work_t* work_schedule_after(work_fn_t fn, void* arg, uint32_t delay_ms);
static work_t* work_schedule_at(work_fn_t fn, void* arg, uint32_t abs_ms);
static work_t* work_schedule_repeat(work_fn_t fn, void* arg, uint32_t first_after_ms, uint16_t period_ms);
static void work_cancel(work_t* w);

/* 10ms/50ms 태스크 프로토타입 */
static void t10_errb(void);
static void t10_led(void);
static void t50_adc(void);
static void t50_log(void);

/* 파워온/리핏 데모 콜백 프로토타입 */
static void do_LCD_RST(void* );
static void do_PON(void* );
static void power_on_sequence(void);
static void demo_repeat_cb(void* );
static void start_demo_repeat(void);

/* ===== 핀 매핑 (데모용) ===== */
static const uint8_t PIN_LED     = LED_BUILTIN; // D13
static const uint8_t PIN_ERRB    = 2;           // D2 (INPUT_PULLUP)
static const uint8_t PIN_LCD_RST = 4;           // D4
static const uint8_t PIN_PON     = 5;           // D5

/* ===== 공유 타임베이스 & 플래그 ===== */
volatile uint32_t g_tick_ms = 0;
static volatile uint8_t g_flag_10ms = 0;
static volatile uint8_t g_flag_50ms = 0;

/* ISR 내부 누적 카운터 */
static uint8_t s_acc_1ms  = 0;  // 1ms → 10ms
static uint8_t s_acc_10ms = 0;  // 10ms → 50ms (5번)

/* ===== Timer2: 1ms CTC 설정 =====
 * 16MHz / 64 = 250kHz → 1ms당 250 tick → OCR2A=249
 */
static void timer2_setup_1ms(void)
{
  cli();
  TCCR2A = (1 << WGM21);           // CTC mode
  TCCR2B = 0;
  OCR2A  = 249;                    // TOP
  TIMSK2 = (1 << OCIE2A);          // Interrupt on compare match
  TCCR2B = (1 << CS22);            // prescaler = 64 (CS22=1, CS21=0, CS20=0)
  sei();
}

/* ===== 1 ms ISR ===== */
ISR(TIMER2_COMPA_vect)
{
  g_tick_ms++;

  if (++s_acc_1ms >= 10) {        // 10ms 도래
    s_acc_1ms = 0;
    g_flag_10ms = 1;

    if (++s_acc_10ms >= 5) {      // 5 * 10ms = 50ms
      s_acc_10ms = 0;
      g_flag_50ms = 1;
    }
  }
}

/* ===== 워크 스케줄러 구현 ===== */
static work_t s_workq[WORK_CAP];

static work_t* work_alloc_slot(void) {
  for (int i = 0; i < WORK_CAP; ++i) if (!s_workq[i].active) return &s_workq[i];
  return NULL;
}

static work_t* work_schedule_after(work_fn_t fn, void* arg, uint32_t delay_ms) {
  work_t* w = work_alloc_slot();
  if (!w) return NULL;
  w->fn = fn;
  w->arg = arg;
  w->next_due_ms = (uint32_t)(g_tick_ms + delay_ms);
  w->period_ms = 0;
  w->mode = WORK_ONESHOT;
  w->active = 1;
  return w;
}

static work_t* work_schedule_at(work_fn_t fn, void* arg, uint32_t abs_ms) {
  work_t* w = work_alloc_slot();
  if (!w) return NULL;
  w->fn = fn;
  w->arg = arg;
  w->next_due_ms = abs_ms;
  w->period_ms = 0;
  w->mode = WORK_ONESHOT;
  w->active = 1;
  return w;
}

static work_t* work_schedule_repeat(work_fn_t fn, void* arg, uint32_t first_after_ms, uint16_t period_ms) {
  work_t* w = work_alloc_slot();
  if (!w) return NULL;
  w->fn = fn;
  w->arg = arg;
  w->next_due_ms = (uint32_t)(g_tick_ms + first_after_ms);
  w->period_ms = period_ms;
  w->mode = WORK_REPEAT;
  w->active = 1;
  return w;
}

static void work_cancel(work_t* w) { if (w) w->active = 0; }

static void work_run_due(uint32_t now_ms) {
  for (int i = 0; i < WORK_CAP; ++i) {
    work_t* w = &s_workq[i];
    if (!w->active) continue;
    if (time_after_eq(now_ms, w->next_due_ms)) {
      w->fn(w->arg);
      if (w->mode == WORK_ONESHOT) {
        w->active = 0;                 // 원샷: 한 번 실행하고 종료
      } else {                         // REPEAT
        do { w->next_due_ms += w->period_ms; }
        while (!time_after_eq(w->next_due_ms, now_ms));
      }
    }
  }
}

/* ===== 10ms 태스크들 ===== */
static void t10_errb(void) {
  // 간단한 디바운스: 연속 3회 LOW → fault set, 3회 HIGH → clear
  static uint8_t lowCnt = 0, highCnt = 0;
  static bool fault = false;

  int v = digitalRead(PIN_ERRB); // INPUT_PULLUP → 눌림=LOW
  if (v == LOW) { if (lowCnt < 255) lowCnt++; highCnt = 0; }
  else          { if (highCnt < 255) highCnt++; lowCnt = 0; }

  if (!fault && lowCnt >= 3) { fault = true;  Serial.println(F("[ERRB] Fault SET")); }
  if ( fault && highCnt >= 3) { fault = false; Serial.println(F("[ERRB] Fault CLEAR")); }
}

static void t10_led(void) {
  // 100ms마다 LED 토글 (10ms 태스크에서 10회 마다)
  static uint8_t acc = 0;
  if (++acc >= 10) {
    acc = 0;
    digitalWrite(PIN_LED, !digitalRead(PIN_LED));
  }
}

/* 배열/카운트는 전역 상수로 */
typedef void (*task_fn_t)(void);
static const task_fn_t g_tasks_10ms[] = { t10_errb, t10_led };
static const int TASK10_COUNT = sizeof(g_tasks_10ms)/sizeof(g_tasks_10ms[0]);

/* ===== 50ms 태스크들 ===== */
static void t50_adc(void) {
  int val = analogRead(A0); // 간단히 한 번 읽기
  static uint8_t acc = 0;
  if (++acc >= 4) { // 50ms * 4 = 200ms
    acc = 0;
    Serial.print(F("[ADC] A0="));
    Serial.println(val);
  }
}
static void t50_log(void) {
  static uint8_t acc = 0;
  if (++acc >= 2) { // 100ms마다
    acc = 0;
    Serial.print(F("[LOG] tick=")); Serial.println(g_tick_ms);
  }
}
static const task_fn_t g_tasks_50ms[] = { t50_adc, t50_log };
static const int TASK50_COUNT = sizeof(g_tasks_50ms)/sizeof(g_tasks_50ms[0]);

/* ===== 파워온 시퀀스: 원샷 워크 ===== */
static void do_LCD_RST(void* ) { digitalWrite(PIN_LCD_RST, HIGH); Serial.println(F("[PWR] LCD_RST=H @5ms")); }
static void do_PON(void* )     { digitalWrite(PIN_PON,     HIGH); Serial.println(F("[PWR] PON=H @21ms")); }

static void power_on_sequence(void)
{
  digitalWrite(PIN_LCD_RST, LOW);
  digitalWrite(PIN_PON,     LOW);
  work_schedule_after(do_LCD_RST, NULL, 5);   // 5ms 후
  work_schedule_after(do_PON,     NULL, 21);  // 21ms 후
}

/* ===== 리핏 워크 데모 (200ms 주기) ===== */
static void demo_repeat_cb(void* ) {
  Serial.println(F("[REPEAT] 200ms tick"));
}
static void start_demo_repeat(void)
{
  work_schedule_repeat(demo_repeat_cb, NULL, 200, 200);
}

/* ===== setup / loop ===== */
void setup()
{
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_ERRB, INPUT_PULLUP);
  pinMode(PIN_LCD_RST, OUTPUT);
  pinMode(PIN_PON, OUTPUT);

  digitalWrite(PIN_LED, LOW);
  digitalWrite(PIN_LCD_RST, LOW);
  digitalWrite(PIN_PON, LOW);

  Serial.begin(57600);
  while (!Serial) { ; }
  Serial.println(F("\n[BOOT] ultra_light_sched demo start"));

  timer2_setup_1ms();     // 1ms 타이머 시작
  power_on_sequence();    // 원샷 워크 데모
  start_demo_repeat();    // 리핏 워크 데모
}

void loop()
{
  uint32_t now = g_tick_ms;

  // 1) 원샷/리핏 워크 수행 (만기 작업 실행)
  work_run_due(now);

  // 2) 10ms 태스크
  if (g_flag_10ms) {
    g_flag_10ms = 0;
    for (int i = 0; i < TASK10_COUNT; ++i) g_tasks_10ms[i]();
  }

  // 3) 50ms 태스크
  if (g_flag_50ms) {
    g_flag_50ms = 0;
    for (int i = 0; i < TASK50_COUNT; ++i) g_tasks_50ms[i]();
  }
}