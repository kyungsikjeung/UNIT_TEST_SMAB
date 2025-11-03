/* button_led_interaction.ino */

/*
 * 버튼-LED 상호작용 예제
 * 
 * 버튼과 LED 드라이버를 함께 사용하여
 * 버튼 누름에 따라 LED 동작을 제어하는 예제입니다.
 */

#include "drivers/driver_manager.h"
#include "drivers/led_driver.h"
#include "drivers/button_driver.h"

// 스케줄러 변수들
volatile uint32_t g_tick_ms = 0;
volatile uint8_t g_flag_10ms = 0;
volatile uint8_t g_flag_50ms = 0;

static uint8_t timer_10ms_count = 0;

void timer_interrupt_1ms(void)
{
  g_tick_ms++;
  
  timer_10ms_count++;
  if (timer_10ms_count >= 10) {
    timer_10ms_count = 0;
    g_flag_10ms = 1;
  }
}

void timer_setup_1ms(void)
{
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  OCR1A = 249;
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS11) | (1 << CS10);
  TIMSK1 |= (1 << OCIE1A);
  interrupts();
}

ISR(TIMER1_COMPA_vect)
{
  timer_interrupt_1ms();
}

// LED 깜빡임 패턴 상태
typedef enum {
  PATTERN_OFF,      // LED 꺼짐
  PATTERN_SLOW,     // 느린 깜빡임 (1초)
  PATTERN_NORMAL,   // 보통 깜빡임 (500ms)
  PATTERN_FAST,     // 빠른 깜빡임 (200ms)
  PATTERN_VERY_FAST // 매우 빠른 깜빡임 (50ms)
} led_pattern_t;

static led_pattern_t current_pattern = PATTERN_NORMAL;

// 버튼 이벤트 핸들러
void on_button_event(uint8_t button_id, uint8_t pressed)
{
  if (pressed) {
    // 버튼이 눌릴 때마다 패턴 변경
    current_pattern = (led_pattern_t)((current_pattern + 1) % 5);
    
    switch (current_pattern) {
      case PATTERN_OFF:
        led_set_blink_enable(false);
        led_set_state(false);
        Serial.println(F("Pattern: OFF"));
        break;
        
      case PATTERN_SLOW:
        led_set_blink_enable(true);
        led_set_blink_rate(1000);
        Serial.println(F("Pattern: SLOW (1000ms)"));
        break;
        
      case PATTERN_NORMAL:
        led_set_blink_enable(true);
        led_set_blink_rate(500);
        Serial.println(F("Pattern: NORMAL (500ms)"));
        break;
        
      case PATTERN_FAST:
        led_set_blink_enable(true);
        led_set_blink_rate(200);
        Serial.println(F("Pattern: FAST (200ms)"));
        break;
        
      case PATTERN_VERY_FAST:
        led_set_blink_enable(true);
        led_set_blink_rate(50);
        Serial.println(F("Pattern: VERY FAST (50ms)"));
        break;
    }
    
    uint32_t count = button_get_press_count();
    Serial.print(F("Button press count: "));
    Serial.println(count);
    
    // 10번 누르면 카운터 리셋
    if (count >= 10) {
      button_reset_press_count();
      Serial.println(F("*** Button counter reset! ***"));
    }
  }
}

void setup()
{
  Serial.begin(57600);
  while (!Serial) { ; }
  
  Serial.println(F("Button-LED Interaction Example"));
  Serial.println(F("Press button to cycle through LED patterns"));
  
  // 스케줄러 초기화
  timer_setup_1ms();
  
  // 드라이버 등록
  driver_register("LED", led_driver_init, led_driver_task, 10);
  driver_register("Button", button_driver_init, button_driver_task, 10);
  
  // 버튼 이벤트 콜백 등록
  button_register_callback(on_button_event);
  
  // 등록된 드라이버 확인
  driver_manager_list();
  
  Serial.println(F("\nPatterns:"));
  Serial.println(F("  1. OFF"));
  Serial.println(F("  2. SLOW (1000ms)"));
  Serial.println(F("  3. NORMAL (500ms)"));
  Serial.println(F("  4. FAST (200ms)"));
  Serial.println(F("  5. VERY FAST (50ms)"));
  Serial.println(F("\nPress button to cycle through patterns"));
}

void loop()
{
  // 드라이버 실행
  driver_manager_run();
  
  // 주기적 상태 출력 (5초마다)
  static uint32_t last_status_ms = 0;
  if (g_tick_ms - last_status_ms >= 5000) {
    last_status_ms = g_tick_ms;
    
    Serial.print(F("Status - Uptime: "));
    Serial.print(g_tick_ms / 1000);
    Serial.print(F("s, Button count: "));
    Serial.print(button_get_press_count());
    Serial.print(F(", Current pattern: "));
    
    switch (current_pattern) {
      case PATTERN_OFF:      Serial.println(F("OFF")); break;
      case PATTERN_SLOW:     Serial.println(F("SLOW")); break;
      case PATTERN_NORMAL:   Serial.println(F("NORMAL")); break;
      case PATTERN_FAST:     Serial.println(F("FAST")); break;
      case PATTERN_VERY_FAST: Serial.println(F("VERY FAST")); break;
    }
  }
}