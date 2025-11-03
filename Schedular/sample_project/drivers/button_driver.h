/* button_driver.h */
#ifndef BUTTON_DRIVER_H
#define BUTTON_DRIVER_H

#include <stdint.h>

// 버튼 이벤트 콜백 함수 타입
typedef void (*button_callback_t)(uint8_t button_id, uint8_t pressed);

/**
 * @brief 버튼 드라이버 초기화
 * @return 0: 성공, -1: 실패
 */
int button_driver_init(void);

/**
 * @brief 버튼 드라이버 주기 태스크 (10ms 마다 호출됨)
 * 
 * 디바운스 처리를 수행하고 버튼 상태 변화를 감지합니다.
 */
void button_driver_task(void);

/**
 * @brief 버튼 이벤트 콜백 함수 등록
 * @param cb 콜백 함수 포인터
 */
void button_register_callback(button_callback_t cb);

/**
 * @brief 현재 버튼 상태 읽기
 * @return 1: 눌림, 0: 떼어짐
 */
uint8_t button_get_state(void);

/**
 * @brief 버튼 눌림 횟수 읽기 (누적)
 * @return 눌림 횟수
 */
uint32_t button_get_press_count(void);

/**
 * @brief 버튼 눌림 횟수 리셋
 */
void button_reset_press_count(void);

#endif // BUTTON_DRIVER_H