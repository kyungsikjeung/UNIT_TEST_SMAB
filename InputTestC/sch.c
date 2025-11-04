/**
 * @file sch.c
 * @brief 통합 태스크 스케줄러 구현
 */

#include <stdio.h>
#include "sch.h"
#include "fault_input.h"

/* ===== 내부 타입 정의 ===== */
typedef struct {
  task_fn_t   fn;
  task_mode_t mode;
  uint8_t     active;
  uint32_t    due_ms;
  uint32_t    period_ms;
} task_slot_t;


/* ===== 전역 변수 ===== */
/* 시간 */
volatile uint32_t g_tick_ms = 0;     
static volatile uint8_t g_flag_10ms = 0;
static volatile uint8_t g_flag_50ms = 0;
static uint8_t s_acc_1ms  = 0;  // 1ms → 10ms
static uint8_t s_acc_10ms = 0;  // 10ms → 50ms(×5)

/* 스케줄러 모드 제어 */
static uint8_t g_boot_mode = 1;        // 부팅 모드 (1ms 정밀도)
static uint32_t g_boot_timeout = 10000; // 10초 후 일반 모드로 전환

/* 테스크 슬롯 */
static task_slot_t s_tasks[MAX_TASKS];

/* ===== 내부 함수 선언 ===== */
static void init_task_slot(void);
static void register_tasks(void);
static void register_task(task_mode_t mode, task_fn_t fn, uint16_t delay_ms, uint16_t period_ms);
static void unregister_task(int idx);
static void run_task_scheduler(void);
static inline int time_after_eq(uint32_t a, uint32_t b);
static void run_task_10ms(void);
static void run_task_50ms(void);

/* ===== ISR 시뮬레이션 ===== */

void test_isr(void)
{
  g_tick_ms++;
  
  // 부팅 모드 체크 및 전환
  if (g_boot_mode && g_tick_ms > g_boot_timeout) {
    g_boot_mode = 0;  // 일반 모드로 전환
  }
  
  // 부팅 중에만 1ms마다 스케줄러 실행
  if (g_boot_mode) {
    run_task_scheduler();
  }

  if (++s_acc_1ms >= 10) {        // 10ms 도래
    s_acc_1ms = 0;
    g_flag_10ms = 1;
    
    // 일반 모드에서는 10ms마다 스케줄러 실행
    if (!g_boot_mode) {
      run_task_scheduler();
    }

    if (++s_acc_10ms >= 5) {      // 50ms
      s_acc_10ms = 0;
      g_flag_50ms = 1;
    }
  }
}

/*
* @brief 태스크 초기화
*/
 static void init_task_slot(void) {
  for (int i = 0; i < MAX_TASKS; ++i) {
    s_tasks[i].fn = NULL;
    s_tasks[i].mode = TASK_ONESHOT;
    s_tasks[i].active = 0;
    s_tasks[i].due_ms = 0;
    s_tasks[i].period_ms = 0;
  }
}


/*
 * @brief 태스크 등록
 * @param mode      태스크 모드 (반복, 일회성)
 * @param fn        태스크 함수 포인터
 * @param delay_ms  최초 지연 시간 (ms)
 * @param period_ms 반복 주기 (TASK_REPEAT 모드에서만 사용, 0이면 1회 실행 후 중지)
 */
static void register_task(task_mode_t mode, task_fn_t fn, uint16_t delay_ms, uint16_t period_ms) {
  if (!fn) return;
  for (int i = 0; i < MAX_TASKS; ++i) {
    if (!s_tasks[i].active) {
      s_tasks[i].fn = fn;
      s_tasks[i].mode = mode;
      s_tasks[i].active = 1;
      s_tasks[i].due_ms = g_tick_ms + delay_ms;
      // 모드별 주기 설정 (등록 시 고정)
      if (mode == TASK_REPEAT) {
        s_tasks[i].period_ms = period_ms;  // 사용자 지정 주기
      } else {  // TASK_ONESHOT
        s_tasks[i].period_ms = 0;
      }
      return;  // 등록 성공 시 즉시 리턴
    }
  }
}

/*
 * @brief 태스크 등록 해제
 * @param idx 태스크 슬롯 인덱스
 */
static void unregister_task(int idx) {
  if (idx < 0 || idx >= MAX_TASKS) return;
  s_tasks[idx].active = 0;
  s_tasks[idx].fn = NULL;
}

/*
 * @brief 태스크 스케줄러 실행 (ISR에서 호출)
 */
static void run_task_scheduler(void) {
  uint32_t now = g_tick_ms;
  
  for (int i = 0; i < MAX_TASKS; ++i) {
    if (!s_tasks[i].active || !s_tasks[i].fn) continue;
    
    if (time_after_eq(now, s_tasks[i].due_ms)) {
      
      if (s_tasks[i].mode == TASK_ONESHOT) {
        s_tasks[i].fn();
        s_tasks[i].active = 0;
        s_tasks[i].fn = NULL;
      } else {  // TASK_REPEAT
        s_tasks[i].fn();
        if (s_tasks[i].period_ms == 0) {
          s_tasks[i].active = 0;  // period_ms=0이면 중지
        } else {
          s_tasks[i].due_ms = now + s_tasks[i].period_ms;
        }
      }
    }
  }
}

/* ===== 오버플로 안전 비교(a가 b 이후 또는 동일이면 true) ===== */
static inline int time_after_eq(uint32_t a, uint32_t b) {
  return (int32_t)(a - b) >= 0;
}

/* ===== 공용 API 함수 ===== */

/* ===== 공용 API 함수 ===== */

/* 10ms 프레임 러너 */
static void run_task_10ms(void) {
  if (g_flag_10ms) {
    g_flag_10ms = 0;
    // 10ms 전용 작업이 있다면 여기에 추가
    // (스케줄러는 이미 ISR에서 실행됨)
  }
}

/* 50ms 프레임 러너 */
static void run_task_50ms(void) {
  if (g_flag_50ms) {
    g_flag_50ms = 0;
    // 50ms 전용 작업이 있다면 여기에 추가
  }
}

/* ===== 예제 태스크 함수들 ===== */

// OneShot 태스크 예제
static void demo_boot_oneshot(void) {
  printf("test oneshot task executed\n");
}

// Repeat 태스크 예제  
static void demo_led_repeat(void) {
  static uint8_t level = 0;
  printf("test repeat task executed\n");
  level = !level;
}

/* ===== 초기화 및 등록 ===== */

/* 태스크 시작 전 초기화 목록*/
void init_task(void) {
   init_task_slot();      // 태스크 슬롯 초기화
   register_tasks();  // 사용자 태스크 등록
   // init_fault_detection(); // ! TODO :  필요 없을 것 같다.
}

static void register_tasks(void)
{
  register_task(TASK_ONESHOT, demo_boot_oneshot, 5000, 0);   // 5초 후 1회 실행 
  register_task(TASK_REPEAT, fault_input_10ms_task, 2000, 1000);      // 2초 후 시작, 1초 주기
}


  


void run_tasks(void)
{
  run_task_10ms();  // 통합 스케줄러 실행
  run_task_50ms();  // 50ms 전용 (필요시)
}