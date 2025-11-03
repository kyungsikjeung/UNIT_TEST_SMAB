/* simple_led_example.ino */

/*
 * 간단한 LED 드라이버 예제
 * 
 * 이 예제는 LED 드라이버만을 사용하는 최소한의 구현입니다.
 * 드라이버 동적 등록 시스템의 기본 사용법을 보여줍니다.
 */

#include "drivers/driver_manager.h"
#include "drivers/led_driver.h"

// 스케줄러 변수들 (간단한 구현)
volatile uint32_t g_tick_ms = 0;
volatile uint8_t g_flag_10ms = 0;
volatile uint8_t g_flag_50ms = 0;

static uint8_t timer_10ms_count = 0;

// 1ms 타이머 인터럽트
void timer_interrupt_1ms(void)
{
  g_tick_ms++;
  
  timer_10ms_count++;
  if (timer_10ms_count >= 10) {
    timer_10ms_count = 0;
    g_flag_10ms = 1;
  }
}

// Timer1 설정 (1ms 주기)
void timer_setup_1ms(void)
{
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  OCR1A = 249;  // 1ms @ 16MHz with 64 prescaler
  TCCR1B |= (1 << WGM12);   // CTC mode
  TCCR1B |= (1 << CS11) | (1 << CS10); // 64 prescaler
  TIMSK1 |= (1 << OCIE1A); // Enable interrupt
  interrupts();
}

ISR(TIMER1_COMPA_vect)
{
  timer_interrupt_1ms();
}

void setup()
{
  Serial.begin(57600);
  while (!Serial) { ; }
  
  Serial.println(F("Simple LED Driver Example"));
  
  // 스케줄러 초기화
  timer_setup_1ms();
  
  // LED 드라이버 등록
  int ret = driver_register("LED", led_driver_init, led_driver_task, 10);
  if (ret == 0) {
    Serial.println(F("LED driver registered successfully"));
  } else {
    Serial.println(F("LED driver registration failed"));
  }
  
  // 등록된 드라이버 확인
  driver_manager_list();
  
  Serial.println(F("LED will blink every 500ms"));
  Serial.println(F("Send commands:"));
  Serial.println(F("  'f' - Fast blink (100ms)"));
  Serial.println(F("  's' - Slow blink (1000ms)"));
  Serial.println(F("  'n' - Normal blink (500ms)"));
}

void loop()
{
  // 드라이버 실행
  driver_manager_run();
  
  // 시리얼 명령어 처리
  if (Serial.available()) {
    char cmd = Serial.read();
    
    switch (cmd) {
      case 'f':
        led_set_blink_rate(100);
        Serial.println(F("Fast blink"));
        break;
        
      case 's':
        led_set_blink_rate(1000);
        Serial.println(F("Slow blink"));
        break;
        
      case 'n':
        led_set_blink_rate(500);
        Serial.println(F("Normal blink"));
        break;
        
      default:
        if (cmd >= 32) {  // 출력 가능한 문자만
          Serial.print(F("Unknown command: "));
          Serial.println(cmd);
        }
        break;
    }
  }
}