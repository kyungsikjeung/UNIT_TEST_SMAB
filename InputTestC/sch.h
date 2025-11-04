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


/**
 * @brief 통합 태스크 실행 (10ms + 50ms)
 */
static void register_tasks(void);  // 선언 추가
void init_task(void); // ! TODO 여기서 타이머 설정
static void init_task_slot(void); // 슬롯에 채워진 테스크 초기화
static void register_tasks(void); // 테스크들 등록 (랩핑)
static void register_task(task_mode_t mode, task_fn_t fn, uint16_t delay_ms, uint16_t period_ms);// 테스크 등록 상세
static void unregister_task(int idx); // 테스크 삭제
static void run_task_scheduler(void); // 슬롯에 등록된 테스크 실행
static inline int time_after_eq(uint32_t a, uint32_t b); // 틱 오버플로우 안전 비교
static void run_task_10ms(void); // 10ms 프레임 러너
static void run_task_50ms(void); // 50ms 프레임 러너
void run_tasks(void); // 통합 태스크 실행 (10ms + 50ms)
void test_isr(void); // 1ms 시뮬레이션 ISR

#endif // SCH_H
