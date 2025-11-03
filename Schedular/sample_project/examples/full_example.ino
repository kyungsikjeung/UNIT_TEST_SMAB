/* full_example.ino */

/*
 * Ultra Light Scheduler - Driver 동적 등록 시스템 완전한 예제
 * 
 * 이 예제는 driver_design_guide.md에서 설명한 동적 드라이버 등록 시스템을
 * 실제로 구현한 완전한 예제입니다.
 * 
 * 포함된 드라이버:
 * - LED Driver: 내장 LED 깜빡임 제어 (10ms 주기)
 * - Button Driver: 버튼 입력 및 디바운스 처리 (10ms 주기)
 * - ADC Driver: 아날로그 센서 값 읽기 (50ms 주기)
 * 
 * 하드웨어 연결:
 * - LED: 내장 LED (LED_BUILTIN) 사용
 * - Button: 디지털 핀 2에 풀업 저항으로 연결
 * - ADC: 아날로그 핀 A0에 가변저항 등 연결
 */

// 드라이버 헤더 파일들 포함
#include "drivers/driver_manager.h"
#include "drivers/led_driver.h"
#include "drivers/button_driver.h"
#include "drivers/adc_driver.h"

// 스케줄러 변수들 (실제로는 ultra_light_sched에서 제공되어야 함)
// 이 예제에서는 간단한 구현으로 대체
volatile uint32_t g_tick_ms = 0;
volatile uint8_t g_flag_10ms = 0;
volatile uint8_t g_flag_50ms = 0;

// 타이머 카운터
static uint8_t timer_10ms_count = 0;
static uint8_t timer_50ms_count = 0;

// 공유 데이터 (드라이버 간 통신용)
typedef struct {
  float temperature;      // 온도 (ADC에서 시뮬레이션)
  uint8_t button_pressed; // 버튼 상태
  uint32_t system_uptime; // 시스템 가동 시간
} shared_data_t;

shared_data_t g_shared_data = {0};

// ===== 간단한 스케줄러 구현 =====

// 1ms 타이머 인터럽트 시뮬레이션
void timer_interrupt_1ms(void)
{
  g_tick_ms++;
  
  // 10ms 플래그
  timer_10ms_count++;
  if (timer_10ms_count >= 10) {
    timer_10ms_count = 0;
    g_flag_10ms = 1;
  }
  
  // 50ms 플래그
  timer_50ms_count++;
  if (timer_50ms_count >= 50) {
    timer_50ms_count = 0;
    g_flag_50ms = 1;
  }
}

// 간단한 타이머 설정 (실제로는 timer2_setup_1ms() 함수 사용)
void timer_setup_1ms(void)
{
  // Timer1을 1ms 주기로 설정 (16MHz 기준)
  noInterrupts();
  
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  
  // 1ms = 16000 클럭 (16MHz / 1000Hz)
  // 프리스케일러 64 사용: 16MHz / 64 = 250kHz
  // 250 카운트 = 1ms
  OCR1A = 249;  // (250 - 1)
  
  TCCR1B |= (1 << WGM12);   // CTC 모드
  TCCR1B |= (1 << CS11) | (1 << CS10); // 64 프리스케일러
  TIMSK1 |= (1 << OCIE1A); // 비교 일치 인터럽트 활성화
  
  interrupts();
  
  Serial.println(F("[TIMER] 1ms timer initialized"));
}

// Timer1 비교 일치 인터럽트 서비스 루틴
ISR(TIMER1_COMPA_vect)
{
  timer_interrupt_1ms();
}

// ===== 버튼 이벤트 핸들러 =====

void on_button_event(uint8_t button_id, uint8_t pressed)
{
  g_shared_data.button_pressed = pressed;
  
  if (pressed) {
    Serial.println(F("\n>>> BUTTON PRESSED <<<"));
    
    // 버튼을 누르면 다양한 동작 수행
    
    // 1. ADC 데이터 즉시 출력
    const adc_data_t* adc_data = adc_get_data();
    if (adc_data->valid) {
      Serial.print(F("Current ADC: "));
      Serial.print(adc_data->voltage, 3);
      Serial.println(F("V"));
    }
    
    // 2. LED 깜빡임 속도 변경 (빠르게)
    led_set_blink_rate(100);
    
    // 3. 시스템 통계 출력
    Serial.print(F("System uptime: "));
    Serial.print(g_tick_ms / 1000);
    Serial.println(F(" seconds"));
    
    Serial.print(F("Button press count: "));
    Serial.println(button_get_press_count());
    
  } else {
    Serial.println(F(">>> BUTTON RELEASED <<<"));
    
    // 버튼을 떼면 LED를 원래 속도로
    led_set_blink_rate(500);
  }
}

// ===== 주기적 시스템 태스크 =====

void system_status_task(void)
{
  static uint32_t last_status_ms = 0;
  
  // 10초마다 시스템 상태 출력
  if (g_tick_ms - last_status_ms >= 10000) {
    last_status_ms = g_tick_ms;
    
    Serial.println(F("\n===== System Status ====="));
    Serial.print(F("Uptime: "));
    Serial.print(g_tick_ms / 1000);
    Serial.println(F(" sec"));
    
    const adc_data_t* adc = adc_get_data();
    if (adc->valid) {
      Serial.print(F("ADC: "));
      Serial.print(adc->voltage, 3);
      Serial.println(F("V"));
    }
    
    Serial.print(F("Button count: "));
    Serial.println(button_get_press_count());
    
    Serial.print(F("Free RAM: "));
    Serial.println(freeRam());
    Serial.println(F("========================\n"));
  }
}

// 사용 가능한 RAM 계산
int freeRam(void)
{
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

// ===== 시리얼 명령어 처리 =====

void process_serial_commands(void)
{
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd == "help") {
      Serial.println(F("\n===== Available Commands ====="));
      Serial.println(F("help        - Show this help"));
      Serial.println(F("drivers     - List all drivers"));
      Serial.println(F("led on      - Turn LED on (manual mode)"));
      Serial.println(F("led off     - Turn LED off (manual mode)"));
      Serial.println(F("led blink   - Switch to blink mode"));
      Serial.println(F("led fast    - Fast blink (100ms)"));
      Serial.println(F("led slow    - Slow blink (1000ms)"));
      Serial.println(F("adc         - Show ADC statistics"));
      Serial.println(F("button      - Show button info"));
      Serial.println(F("reset       - Reset button counter"));
      Serial.println(F("status      - Show system status"));
      Serial.println(F("===============================\n"));
      
    } else if (cmd == "drivers") {
      driver_manager_list();
      
    } else if (cmd == "led on") {
      led_set_blink_enable(false);
      led_set_state(true);
      
    } else if (cmd == "led off") {
      led_set_blink_enable(false);
      led_set_state(false);
      
    } else if (cmd == "led blink") {
      led_set_blink_enable(true);
      
    } else if (cmd == "led fast") {
      led_set_blink_rate(100);
      
    } else if (cmd == "led slow") {
      led_set_blink_rate(1000);
      
    } else if (cmd == "adc") {
      adc_print_stats();
      
    } else if (cmd == "button") {
      Serial.print(F("Button state: "));
      Serial.println(button_get_state() ? F("PRESSED") : F("RELEASED"));
      Serial.print(F("Press count: "));
      Serial.println(button_get_press_count());
      
    } else if (cmd == "reset") {
      button_reset_press_count();
      
    } else if (cmd == "status") {
      system_status_task();
      
    } else if (cmd.length() > 0) {
      Serial.print(F("Unknown command: "));
      Serial.println(cmd);
      Serial.println(F("Type 'help' for available commands"));
    }
  }
}

// ===== Arduino 메인 함수들 =====

void setup()
{
  // 시리얼 통신 초기화
  Serial.begin(57600);
  while (!Serial) { ; }  // Leonardo/Micro용
  
  Serial.println(F("\n========================================"));
  Serial.println(F("Ultra Light Scheduler - Driver Example"));
  Serial.println(F("========================================"));
  
  // 스케줄러 초기화
  timer_setup_1ms();
  
  Serial.println(F("\n--- Registering Drivers ---"));
  
  // 드라이버들을 동적으로 등록
  int ret;
  
  ret = driver_register("LED", led_driver_init, led_driver_task, 10);
  if (ret != 0) {
    Serial.println(F("ERROR: LED driver registration failed"));
  }
  
  ret = driver_register("Button", button_driver_init, button_driver_task, 10);
  if (ret != 0) {
    Serial.println(F("ERROR: Button driver registration failed"));
  }
  
  ret = driver_register("ADC", adc_driver_init, adc_driver_task, 50);
  if (ret != 0) {
    Serial.println(F("ERROR: ADC driver registration failed"));
  }
  
  // 버튼 이벤트 콜백 등록
  button_register_callback(on_button_event);
  
  // 등록된 드라이버 목록 출력
  driver_manager_list();
  
  Serial.println(F("--- System Ready ---"));
  Serial.println(F("Type 'help' for available commands\n"));
  
  // 초기 설정
  led_set_blink_rate(500);  // 0.5초 주기로 깜빡임
}

void loop()
{
  // 드라이버 매니저 실행 (모든 등록된 드라이버 자동 실행)
  driver_manager_run();
  
  // 시스템 태스크들
  system_status_task();
  process_serial_commands();
  
  // 공유 데이터 업데이트
  g_shared_data.system_uptime = g_tick_ms;
  
  // ADC 값을 온도로 시뮬레이션 (실제로는 온도 센서 연결 필요)
  const adc_data_t* adc = adc_get_data();
  if (adc->valid) {
    // 0-5V를 0-50도로 변환 (임의의 변환)
    g_shared_data.temperature = adc->voltage * 10.0f;
  }
}