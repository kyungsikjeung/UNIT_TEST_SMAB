/* adc_driver.h */
#ifndef ADC_DRIVER_H
#define ADC_DRIVER_H

#include <stdint.h>

// ADC 데이터 구조체
typedef struct {
  uint16_t raw;             // 원시 ADC 값 (0-1023)
  float voltage;            // 전압 값 (V)
  uint32_t timestamp_ms;    // 측정 시간 (ms)
  uint8_t valid;            // 데이터 유효성
} adc_data_t;

/**
 * @brief ADC 드라이버 초기화
 * @return 0: 성공, -1: 실패
 */
int adc_driver_init(void);

/**
 * @brief ADC 드라이버 주기 태스크 (50ms 마다 호출됨)
 * 
 * ADC 값을 읽고 전압으로 변환하여 저장합니다.
 */
void adc_driver_task(void);

/**
 * @brief 최신 ADC 데이터 읽기
 * @return ADC 데이터 구조체 포인터 (읽기 전용)
 */
const adc_data_t* adc_get_data(void);

/**
 * @brief ADC 참조 전압 설정
 * @param ref_voltage 참조 전압 (V) - 기본값 5.0V
 */
void adc_set_reference_voltage(float ref_voltage);

/**
 * @brief ADC 핀 변경
 * @param pin 아날로그 핀 번호 (A0, A1, A2, ...)
 */
void adc_set_pin(uint8_t pin);

/**
 * @brief ADC 통계 정보 출력
 */
void adc_print_stats(void);

#endif // ADC_DRIVER_H