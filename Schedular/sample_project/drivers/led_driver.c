/* led_driver.c */
#include "led_driver.h"
#include <Arduino.h>

// 외부 스케줄러 변수
extern volatile uint32_t g_tick_ms;

// LED 드라이버 내부 상태
static struct {
  uint16_t blink_rate_ms;     // 깜빡임 주기
  uint32_t last_toggle_ms;    // 마지막 토글 시간
  uint8_t state;              // 현재 LED 상태 (0/1)
  uint8_t blink_enabled;      // 깜빡임 모드 활성화 여부
  uint8_t manual_state;       // 수동 모드에서의 LED 상태
} led_ctx;

int led_driver_init(void)
{
  // LED 핀 초기화 (Arduino의 내장 LED)
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  // 드라이버 상태 초기화
  led_ctx.blink_rate_ms = 500;      // 기본 500ms 주기
  led_ctx.last_toggle_ms = 0;
  led_ctx.state = 0;
  led_ctx.blink_enabled = 1;        // 기본적으로 깜빡임 모드
  led_ctx.manual_state = 0;
  
  Serial.println(F("[LED] Driver initialized - Blink mode @ 500ms"));
  return 0;
}

void led_driver_task(void)
{
  // 10ms마다 호출됨
  
  if (led_ctx.blink_enabled) {
    // 깜빡임 모드
    uint32_t now = g_tick_ms;
    
    if (now - led_ctx.last_toggle_ms >= led_ctx.blink_rate_ms) {
      led_ctx.last_toggle_ms = now;
      led_ctx.state = !led_ctx.state;
      digitalWrite(LED_BUILTIN, led_ctx.state);
    }
  } else {
    // 수동 모드
    if (led_ctx.state != led_ctx.manual_state) {
      led_ctx.state = led_ctx.manual_state;
      digitalWrite(LED_BUILTIN, led_ctx.state);
    }
  }
}

void led_set_blink_rate(uint16_t rate_ms)
{
  led_ctx.blink_rate_ms = rate_ms;
  
  Serial.print(F("[LED] Blink rate set to "));
  Serial.print(rate_ms);
  Serial.println(F(" ms"));
}

void led_set_state(bool state)
{
  led_ctx.manual_state = state ? 1 : 0;
  
  if (!led_ctx.blink_enabled) {
    // 수동 모드에서만 즉시 적용
    led_ctx.state = led_ctx.manual_state;
    digitalWrite(LED_BUILTIN, led_ctx.state);
  }
  
  Serial.print(F("[LED] Manual state set to "));
  Serial.println(state ? F("ON") : F("OFF"));
}

void led_set_blink_enable(bool enable)
{
  led_ctx.blink_enabled = enable ? 1 : 0;
  
  if (enable) {
    // 깜빡임 모드로 전환
    led_ctx.last_toggle_ms = g_tick_ms;  // 타이밍 리셋
    Serial.println(F("[LED] Switched to BLINK mode"));
  } else {
    // 수동 모드로 전환
    led_ctx.state = led_ctx.manual_state;
    digitalWrite(LED_BUILTIN, led_ctx.state);
    Serial.println(F("[LED] Switched to MANUAL mode"));
  }
}