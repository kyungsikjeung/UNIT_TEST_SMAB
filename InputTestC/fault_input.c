#include "fault_input.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

/* ===== Fault 카운터 (volatile for ISR safety) ===== */
volatile uint8_t lcdErrorCount = 0;
volatile uint8_t ledErrorCount = 0;
volatile uint8_t gmslErrorCount = 0;
volatile uint8_t lcdErrorClearCount = 0;
volatile uint8_t ledErrorClearCount = 0;
volatile uint8_t gmslErrorClearCount = 0;

/* ===== Fault 상태 (State Machine) ===== */
typedef enum {
    FAULT_STATE_NORMAL = 0,      // 정상 상태
    FAULT_STATE_ERROR_LATCHED,   // 에러 확정 상태
} fault_state_t;

static fault_state_t lcdState = FAULT_STATE_NORMAL;
static fault_state_t ledState = FAULT_STATE_NORMAL;
static fault_state_t gmslState = FAULT_STATE_NORMAL;

/*
* description : Fault 입력 처리 모듈
*/
typedef struct {
    bool lcd_fault;
    bool led_fault;
    bool gmsl_fault;
} fault_inputs_t;

#define FAULT_LATCH_THRESHOLD 3  // 3번 연속 감지 시 확정

/* ===== Hardware Abstraction Layer ===== */

// Fault 입력별 인덱스
typedef enum {
    FAULT_INPUT_LCD = 0,
    FAULT_INPUT_LED = 1,
    FAULT_INPUT_GMSL = 2,
    FAULT_INPUT_MAX
} fault_input_index_t;

// 2차원 테스트 데이터: [입력종류][시간순서]
// true = fault, false = normal
// ! TODO :  배열 아예 지울것
static const bool dummy_test_data[FAULT_INPUT_MAX][33] = {
    // LCD Fault Input (Active High)
    {
        true, true, true,           // 0-2:   3번 연속 에러 → FAULT 리포트
        true, true,                 // 3-4:   에러 계속 (리포트 없음)
        false, false, false,        // 5-7:   3번 연속 정상 → CLEAR 리포트
        false, false,               // 8-9:   정상 계속
        true, true, false,          // 10-12: 불규칙 (2번만 에러)
        true, true, true,           // 13-15: 3번 연속 에러 → FAULT 리포트
        false, false, false,        // 16-18: 3번 연속 정상 → CLEAR 리포트
        false, false, false,        // 19-21: 정상 계속
        false, false, false,        // 22-24: 정상 계속
        false, false, false,        // 25-27: 정상 계속
        false, false, false,        // 28-30: 정상 계속
        false, false                // 31-32: 정상
    },
    
    // LED Fault Input (Active High)
    {
        false, false, false,        // 0-2:   정상
        true, true, true,           // 3-5:   3번 연속 에러 → FAULT 리포트
        true, true,                 // 6-7:   에러 계속
        false, false, false,        // 8-10:  3번 연속 정상 → CLEAR 리포트
        true, false, true,          // 11-13: 불규칙
        false, false, false,        // 14-16: 정상
        true, true, true,           // 17-19: 3번 연속 에러 → FAULT 리포트
        false, false, false,        // 20-22: 3번 연속 정상 → CLEAR 리포트
        false, false, false,        // 23-25: 정상
        false, false, false,        // 26-28: 정상
        false, false, false,        // 29-31: 정상
        false                       // 32:    정상
    },
    
    // GMSL Fault Input (Active High)
    {
        false, false, false,        // 0-2:   정상
        false, false, false,        // 3-5:   정상
        true, true, true,           // 6-8:   3번 연속 에러 → FAULT 리포트
        true,                       // 9:     에러 계속
        false, false, false,        // 10-12: 3번 연속 정상 → CLEAR 리포트
        false, false,               // 13-14: 정상
        true, false, true,          // 15-17: 불규칙
        false, true, true,          // 18-20: 불규칙
        true, true, true,           // 21-23: 3번 연속 에러 → FAULT 리포트
        false, false, false,        // 24-26: 3번 연속 정상 → CLEAR 리포트
        false, false, false,        // 27-29: 정상
        false, false, false         // 30-32: 정상
    }
};

// ! TODO : 변수 아예 지울것
#define TEST_DATA_LENGTH 33

// simulation dummy function (thread-safe 개선)
// ! TODO :  변수 아예 지울것
static volatile int dummy_counter = 0;  // 전역 카운터 (ISR-safe)

/**
 * @brief 테스트 카운터 리셋 (테스트 초기화용)
 */
void reset_dummy_counter(void) {
    // ! TODO :  함수 아예 지울것
    dummy_counter = 0;
}

/* ===== Input Sampling (동일 시점 스냅샷) ===== */

/**
 * @brief 모든 fault 입력을 동일 시점에 샘플링
 * @return fault_inputs_t 구조체 (스냅샷)
 */
static fault_inputs_t read_fault_inputs_snapshot(void) {
    fault_inputs_t snapshot;
    
    // ! Todo Test 용도! 추후 삭제 필요
    int index = dummy_counter % TEST_DATA_LENGTH;
    // ! TODO : 
    snapshot.lcd_fault = dummy_test_data[FAULT_INPUT_LCD][index];
    snapshot.led_fault = dummy_test_data[FAULT_INPUT_LED][index];
    snapshot.gmsl_fault = dummy_test_data[FAULT_INPUT_GMSL][index];
    
    // ! TODO : 나중에 지울것..
    dummy_counter++;
    
    return snapshot;
}

/* ===== Fault Processing Logic ===== */

/**
 * @brief 개별 fault 처리 (순수 함수, 부작용 없음)
 * @param has_fault     현재 fault 상태 (입력)
 * @param error_count   에러 카운터 (입출력)
 * @param clear_count   클리어 카운터 (입출력)
 * @param state         현재 상태 (입출력)
 * @param name          fault 이름 (디버깅)
 */
static void process_single_fault(
    bool has_fault,
    volatile uint8_t *error_count,
    volatile uint8_t *clear_count,
    fault_state_t *state,
    const char *name
) {
    if (!error_count || !clear_count || !state || !name) {
        return;
    } 
    if (has_fault) {
        // 에러 감지
        if (*error_count < FAULT_LATCH_THRESHOLD) {
            (*error_count)++;
        }
        *clear_count = 0;
        
        // 3번 연속 에러 감지 시 상태 전환 및 리포트
        if (*error_count >= FAULT_LATCH_THRESHOLD && *state == FAULT_STATE_NORMAL) {
            printf("[FAULT] %s Error detected (latched)\n", name);
            *state = FAULT_STATE_ERROR_LATCHED;
        }
    } else {
        // 정상 감지
        if (*clear_count < FAULT_LATCH_THRESHOLD) {
            (*clear_count)++;
        }
        *error_count = 0;
        
        // 3번 연속 클리어 감지 시 상태 전환 및 리포트
        if (*clear_count >= FAULT_LATCH_THRESHOLD && *state == FAULT_STATE_ERROR_LATCHED) {
            printf("[CLEAR] %s Error cleared\n", name);
            *state = FAULT_STATE_NORMAL;
        }
    }
}

/* ===== Public API ===== */

/**
 * @brief Fault 입력 처리 메인 함수 (Safety Critical)
 * @note 주기적으로 호출 (예: 10ms task)
 */
void fault_input_10ms_task(void){
    fault_inputs_t inputs = read_fault_inputs_snapshot(); // Fault 읽기
    process_single_fault(
        inputs.lcd_fault, 
        &lcdErrorCount, 
        &lcdErrorClearCount, 
        &lcdState, 
        "LCD"
    );
    
    process_single_fault(
        inputs.led_fault, 
        &ledErrorCount, 
        &ledErrorClearCount, 
        &ledState, 
        "LED"
    );
    
    process_single_fault(
        inputs.gmsl_fault, 
        &gmslErrorCount, 
        &gmslErrorClearCount, 
        &gmslState, 
        "GMSL"
    );
}

/**
 * @brief Fault 시스템 초기화
 */
void init_fault_detection(void) {
    lcdErrorCount = 0;
    ledErrorCount = 0;
    gmslErrorCount = 0;
    lcdErrorClearCount = 0;
    ledErrorClearCount = 0;
    gmslErrorClearCount = 0;
    
    lcdState = FAULT_STATE_NORMAL;
    ledState = FAULT_STATE_NORMAL;
    gmslState = FAULT_STATE_NORMAL;
}

/**
 * @brief Fault 상태 조회 (읽기 전용)
 */
bool is_lcd_fault_latched(void) {
    return (lcdState == FAULT_STATE_ERROR_LATCHED);
}

bool is_led_fault_latched(void) {
    return (ledState == FAULT_STATE_ERROR_LATCHED);
}

bool is_gmsl_fault_latched(void) {
    return (gmslState == FAULT_STATE_ERROR_LATCHED);
}