/**
 * @file fault_input.h
 * @brief Fault Input Detection Module
 * @details 3회 연속 에러 감지 시 latched, 3회 연속 정상 시 cleared
 *          동일 시점 스냅샷 기반 안전한 입력 처리
 */

#ifndef FAULT_INPUT_H
#define FAULT_INPUT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Public API Functions ===== */

/**
 * @brief Fault 감지 시스템 초기화
 * @note 시스템 시작 시 1회 호출 필요
 */
void init_fault_detection(void); // ! TODO :  시스템 시작 시 1회 호출 필요

/**
 * @brief Fault 입력 처리 메인 함수
 * @details 스케줄러에서 주기적으로 호출 (예: 10ms task)
 *          - 3회 연속 에러 감지 시 [FAULT] 메시지 1회 출력
 *          - 3회 연속 정상 감지 시 [CLEAR] 메시지 1회 출력
 * @note ISR-safe, re-entrant safe
 */
void fault_input_10ms_task(void); 

/**
 * @brief LCD Fault 상태 조회
 * @return true: Fault latched, false: Normal
 */
bool is_lcd_fault_latched(void);

/**
 * @brief LED Fault 상태 조회
 * @return true: Fault latched, false: Normal
 */
bool is_led_fault_latched(void);

/**
 * @brief GMSL Fault 상태 조회
 * @return true: Fault latched, false: Normal
 */
bool is_gmsl_fault_latched(void);

/**
 * @brief 테스트 카운터 리셋 (테스트용)
 * @note 테스트 시작 전 호출하여 더미 데이터를 처음부터 재생
 */
void reset_dummy_counter(void);

/* ===== External Variables (Read-Only) ===== */

/**
 * @brief Fault 카운터 (디버깅/모니터링 용도)
 * @note volatile: ISR에서 접근 가능
 */
extern volatile uint8_t lcdErrorCount;
extern volatile uint8_t ledErrorCount;
extern volatile uint8_t gmslErrorCount;
extern volatile uint8_t lcdErrorClearCount;
extern volatile uint8_t ledErrorClearCount;
extern volatile uint8_t gmslErrorClearCount;

#ifdef __cplusplus
}
#endif

#endif // FAULT_INPUT_H