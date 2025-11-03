/* button_driver.c */
#include "button_driver.h"
#include <Arduino.h>

// 버튼 핀 설정 (기본적으로 디지털 핀 2 사용)
#ifndef BUTTON_PIN
#define BUTTON_PIN 2
#endif

// 디바운스 카운트 (연속으로 같은 값이 나와야 하는 횟수)
#define DEBOUNCE_COUNT 3

// 버튼 드라이버 내부 상태
static struct {
  uint8_t raw_state;          // 현재 읽은 원시 상태
  uint8_t stable_state;       // 디바운스된 안정화 상태
  uint8_t prev_stable_state;  // 이전 안정화 상태 (에지 검출용)
  uint8_t debounce_count;     // 디바운스 카운터
  uint32_t press_count;       // 누름 횟수 (누적)
  button_callback_t callback; // 이벤트 콜백 함수
} btn_ctx;

int button_driver_init(void)
{
  // 버튼 핀을 풀업 입력으로 설정
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // 드라이버 상태 초기화
  btn_ctx.raw_state = HIGH;           // 풀업이므로 기본값은 HIGH
  btn_ctx.stable_state = HIGH;
  btn_ctx.prev_stable_state = HIGH;
  btn_ctx.debounce_count = 0;
  btn_ctx.press_count = 0;
  btn_ctx.callback = NULL;
  
  Serial.print(F("[BTN] Driver initialized - Pin "));
  Serial.println(BUTTON_PIN);
  return 0;
}

void button_driver_task(void)
{
  // 10ms마다 호출되어 디바운스 처리 수행
  
  uint8_t current_raw = digitalRead(BUTTON_PIN);
  
  if (current_raw == btn_ctx.raw_state) {
    // 연속으로 같은 값이 나오고 있음
    if (btn_ctx.debounce_count < DEBOUNCE_COUNT) {
      btn_ctx.debounce_count++;
      
      if (btn_ctx.debounce_count == DEBOUNCE_COUNT) {
        // 디바운스 완료 - 안정화된 상태로 인정
        btn_ctx.prev_stable_state = btn_ctx.stable_state;
        btn_ctx.stable_state = current_raw;
        
        // 상태 변화 감지 (에지 검출)
        if (btn_ctx.prev_stable_state != btn_ctx.stable_state) {
          if (btn_ctx.stable_state == LOW) {
            // 버튼 눌림 (HIGH -> LOW, 풀업이므로)
            btn_ctx.press_count++;
            
            Serial.print(F("[BTN] PRESSED (count: "));
            Serial.print(btn_ctx.press_count);
            Serial.println(F(")"));
            
            // 콜백 호출
            if (btn_ctx.callback) {
              btn_ctx.callback(0, 1);  // button_id=0, pressed=1
            }
          } else {
            // 버튼 떼어짐 (LOW -> HIGH)
            Serial.println(F("[BTN] RELEASED"));
            
            // 콜백 호출
            if (btn_ctx.callback) {
              btn_ctx.callback(0, 0);  // button_id=0, pressed=0
            }
          }
        }
      }
    }
  } else {
    // 다른 값이 나옴 - 디바운스 카운터 리셋
    btn_ctx.raw_state = current_raw;
    btn_ctx.debounce_count = 0;
  }
}

void button_register_callback(button_callback_t cb)
{
  btn_ctx.callback = cb;
  Serial.println(F("[BTN] Callback registered"));
}

uint8_t button_get_state(void)
{
  // 풀업이므로 LOW가 눌림 상태
  return (btn_ctx.stable_state == LOW) ? 1 : 0;
}

uint32_t button_get_press_count(void)
{
  return btn_ctx.press_count;
}

void button_reset_press_count(void)
{
  btn_ctx.press_count = 0;
  Serial.println(F("[BTN] Press count reset"));
}