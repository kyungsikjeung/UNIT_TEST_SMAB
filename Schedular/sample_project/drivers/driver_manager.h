/* driver_manager.h */
#ifndef DRIVER_MANAGER_H
#define DRIVER_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

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

#endif // DRIVER_MANAGER_H