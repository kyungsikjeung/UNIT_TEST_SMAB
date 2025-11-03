/* adc_driver.c */
#include "adc_driver.h"
#include <Arduino.h>

// 외부 스케줄러 변수
extern volatile uint32_t g_tick_ms;

// 기본 ADC 핀 (A0)
#ifndef ADC_PIN
#define ADC_PIN A0
#endif

// ADC 드라이버 내부 상태
static struct {
  adc_data_t current_data;    // 현재 ADC 데이터
  float ref_voltage;          // 참조 전압
  uint8_t adc_pin;            // 사용할 ADC 핀
  uint32_t sample_count;      // 총 샘플 수
  uint32_t last_log_ms;       // 마지막 로그 출력 시간
  uint16_t log_interval_ms;   // 로그 출력 간격
} adc_ctx;

int adc_driver_init(void)
{
  // ADC 핀 설정
  pinMode(ADC_PIN, INPUT);
  
  // 드라이버 상태 초기화
  adc_ctx.current_data.raw = 0;
  adc_ctx.current_data.voltage = 0.0f;
  adc_ctx.current_data.timestamp_ms = 0;
  adc_ctx.current_data.valid = 0;
  
  adc_ctx.ref_voltage = 5.0f;         // 기본 5V 참조
  adc_ctx.adc_pin = ADC_PIN;
  adc_ctx.sample_count = 0;
  adc_ctx.last_log_ms = 0;
  adc_ctx.log_interval_ms = 1000;     // 1초마다 로그
  
  Serial.print(F("[ADC] Driver initialized - Pin A"));
  Serial.print(adc_ctx.adc_pin - A0);
  Serial.print(F(", Ref: "));
  Serial.print(adc_ctx.ref_voltage);
  Serial.println(F("V"));
  
  return 0;
}

void adc_driver_task(void)
{
  // 50ms마다 호출되어 ADC 값을 읽음
  
  uint32_t now = g_tick_ms;
  
  // ADC 값 읽기
  uint16_t raw_value = analogRead(adc_ctx.adc_pin);
  
  // 전압으로 변환 (10비트 ADC: 0-1023)
  float voltage = (raw_value * adc_ctx.ref_voltage) / 1024.0f;
  
  // 데이터 업데이트
  adc_ctx.current_data.raw = raw_value;
  adc_ctx.current_data.voltage = voltage;
  adc_ctx.current_data.timestamp_ms = now;
  adc_ctx.current_data.valid = 1;
  adc_ctx.sample_count++;
  
  // 주기적 로그 출력 (1초마다)
  if (now - adc_ctx.last_log_ms >= adc_ctx.log_interval_ms) {
    adc_ctx.last_log_ms = now;
    
    Serial.print(F("[ADC] Raw: "));
    Serial.print(raw_value);
    Serial.print(F(", Voltage: "));
    Serial.print(voltage, 3);  // 소수점 3자리
    Serial.print(F("V, Samples: "));
    Serial.println(adc_ctx.sample_count);
  }
}

const adc_data_t* adc_get_data(void)
{
  return &adc_ctx.current_data;
}

void adc_set_reference_voltage(float ref_voltage)
{
  if (ref_voltage > 0.0f && ref_voltage <= 5.5f) {
    adc_ctx.ref_voltage = ref_voltage;
    
    Serial.print(F("[ADC] Reference voltage set to "));
    Serial.print(ref_voltage);
    Serial.println(F("V"));
  } else {
    Serial.println(F("[ADC] ERROR: Invalid reference voltage"));
  }
}

void adc_set_pin(uint8_t pin)
{
  // 아날로그 핀 범위 확인 (A0-A5 for Uno)
  if (pin >= A0 && pin <= A5) {
    adc_ctx.adc_pin = pin;
    pinMode(pin, INPUT);
    
    Serial.print(F("[ADC] Pin changed to A"));
    Serial.println(pin - A0);
  } else {
    Serial.println(F("[ADC] ERROR: Invalid analog pin"));
  }
}

void adc_print_stats(void)
{
  Serial.println(F("\n===== ADC Statistics ====="));
  Serial.print(F("Pin: A"));
  Serial.println(adc_ctx.adc_pin - A0);
  Serial.print(F("Reference Voltage: "));
  Serial.print(adc_ctx.ref_voltage);
  Serial.println(F("V"));
  Serial.print(F("Total Samples: "));
  Serial.println(adc_ctx.sample_count);
  
  if (adc_ctx.current_data.valid) {
    Serial.print(F("Last Reading: "));
    Serial.print(adc_ctx.current_data.raw);
    Serial.print(F(" ("));
    Serial.print(adc_ctx.current_data.voltage, 3);
    Serial.println(F("V)"));
    Serial.print(F("Last Update: "));
    Serial.print(adc_ctx.current_data.timestamp_ms);
    Serial.println(F(" ms"));
  } else {
    Serial.println(F("No valid data"));
  }
  
  Serial.println(F("========================\n"));
}