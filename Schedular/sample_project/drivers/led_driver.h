/* led_driver.h */
#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#include <stdint.h>

/**
 * @brief LED 드라이버 초기화
 * @return 0: 성공, -1: 실패
 */
int led_driver_init(void);

/**
 * @brief LED 드라이버 주기 태스크 (10ms 마다 호출됨)
 */
void led_driver_task(void);

/**
 * @brief LED 깜빡임 속도 설정
 * @param rate_ms 깜빡임 주기 (밀리초)
 */
void led_set_blink_rate(uint16_t rate_ms);

/**
 * @brief LED 강제 켜기/끄기
 * @param state true: 켜기, false: 끄기
 */
void led_set_state(bool state);

/**
 * @brief LED 깜빡임 모드 활성화/비활성화
 * @param enable true: 깜빡임 모드, false: 수동 모드
 */
void led_set_blink_enable(bool enable);

#endif // LED_DRIVER_H