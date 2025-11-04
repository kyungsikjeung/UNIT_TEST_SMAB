/**
 * @file sch.h
 * @brief 통합 태스크 스케줄러 헤더 파일
 * @details 부팅 모드(1ms 정밀도) 및 일반 모드(10ms 정밀도) 지원
 *          ONESHOT(1회 실행) 및 REPEAT(주기 반복) 모드 제공
 */
#ifndef SCH_H
#define SCH_H

#include <stdint.h>
#include <stdbool.h>
typedef void (*task_fn_t)(void);
typedef enum {
  TASK_ONESHOT = 0,   ///< 1회 실행 후 자동 해제
  TASK_REPEAT = 1,    ///< 주기적 반복 실행
} task_mode_t;

#define MAX_TASKS 10  ///< 최대 태스크 슬롯 개수
extern volatile uint32_t g_tick_ms;
void init_task(void);
void run_tasks(void);
void test_isr(void);

#endif // SCH_H
