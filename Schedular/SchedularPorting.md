MCU 포팅 가이드 - Ultra Light Scheduler
🎯 포팅 개요
Ultra Light Scheduler는 플랫폼 독립적인 코어와 하드웨어 종속적인 타이머 레이어로 분리되어 있어, 다른 MCU로 쉽게 포팅할 수 있습니다.
아키텍처 레이어
┌─────────────────────────────────────┐
│   Application Layer                 │  ← 플랫폼 독립
│   (워크 콜백, 태스크 함수)            │
├─────────────────────────────────────┤
│   Scheduler Core                    │  ← 플랫폼 독립
│   (WorkScheduler, PeriodicTasks)    │
├─────────────────────────────────────┤
│   Time Base                         │  ← 플랫폼 독립
│   (g_tick_ms, flags)                │
├─────────────────────────────────────┤
│   Hardware Timer HAL                │  ← **포팅 필요**
│   (Timer2_AVR, TIM2_STM32, etc)     │
└─────────────────────────────────────┘

📋 포팅 체크리스트
✅ 변경 불필요 (100% 재사용)

✓ work_t 구조체
✓ WorkScheduler 전체 로직
✓ time_after_eq() 함수
✓ PeriodicTaskManager 로직
✓ 플래그 시스템 (g_flag_10ms, g_flag_50ms)

⚠️ 포팅 필요 (타이머 설정만)

⚙️ timer_setup_1ms() - MCU별 타이머 초기화
⚙️ ISR() / IRQHandler() - 인터럽트 핸들러


🔧 포팅 가이드
1. STM32 (STM32F4xx)
timer_hal_stm32.c
c#include "stm32f4xx_hal.h"

TIM_HandleTypeDef htim2;

void timer_setup_1ms(void)
{
  __HAL_RCC_TIM2_CLK_ENABLE();
  
  // 84MHz APB1 클럭 가정
  // 84,000,000 / (83+1) / (999+1) = 1000Hz (1ms)
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 83;           // 84MHz -> 1MHz
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 999;             // 1MHz -> 1kHz
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  
  HAL_TIM_Base_Init(&htim2);
  HAL_TIM_Base_Start_IT(&htim2);
  
  HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(TIM2_IRQn);
}

void TIM2_IRQHandler(void)
{
  if (__HAL_TIM_GET_FLAG(&htim2, TIM_FLAG_UPDATE)) {
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
    
    // 공통 코드
    g_tick_ms++;
    
    if (++s_acc_1ms >= 10) {
      s_acc_1ms = 0;
      g_flag_10ms = 1;
      
      if (++s_acc_10ms >= 5) {
        s_acc_10ms = 0;
        g_flag_50ms = 1;
      }
    }
  }
}
stm32f4xx_it.c
cextern void TIM2_IRQHandler(void);

void TIM2_IRQHandler(void) {
  // timer_hal_stm32.c의 핸들러 호출
}

2. ESP32 (ESP-IDF / Arduino)
timer_hal_esp32.cpp
cpp#include <Arduino.h>

hw_timer_t* timer1ms = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer1ms()
{
  portENTER_CRITICAL_ISR(&timerMux);
  
  // 공통 코드
  g_tick_ms++;
  
  if (++s_acc_1ms >= 10) {
    s_acc_1ms = 0;
    g_flag_10ms = 1;
    
    if (++s_acc_10ms >= 5) {
      s_acc_10ms = 0;
      g_flag_50ms = 1;
    }
  }
  
  portEXIT_CRITICAL_ISR(&timerMux);
}

void timer_setup_1ms(void)
{
  // Timer 0, prescaler 80 (80MHz -> 1MHz)
  timer1ms = timerBegin(0, 80, true);
  
  // 1000 ticks = 1ms
  timerAttachInterrupt(timer1ms, &onTimer1ms, true);
  timerAlarmWrite(timer1ms, 1000, true);
  timerAlarmEnable(timer1ms);
}
주의사항:

ESP32는 듀얼코어이므로 portENTER_CRITICAL_ISR 필수
IRAM에 ISR 배치 (IRAM_ATTR)


3. RP2040 (Raspberry Pi Pico)
timer_hal_rp2040.c
c#include "pico/stdlib.h"
#include "hardware/timer.h"

static struct repeating_timer timer_1ms;

bool timer_1ms_callback(struct repeating_timer *t)
{
  // 공통 코드
  g_tick_ms++;
  
  if (++s_acc_1ms >= 10) {
    s_acc_1ms = 0;
    g_flag_10ms = 1;
    
    if (++s_acc_10ms >= 5) {
      s_acc_10ms = 0;
      g_flag_50ms = 1;
    }
  }
  
  return true;  // 타이머 계속 실행
}

void timer_setup_1ms(void)
{
  add_repeating_timer_ms(1, timer_1ms_callback, NULL, &timer_1ms);
}

4. nRF52 (Nordic Semiconductor)
timer_hal_nrf52.c
c#include "nrf_drv_timer.h"

const nrf_drv_timer_t TIMER_1MS = NRF_DRV_TIMER_INSTANCE(0);

void timer_1ms_event_handler(nrf_timer_event_t event_type, void* p_context)
{
  if (event_type == NRF_TIMER_EVENT_COMPARE0) {
    // 공통 코드
    g_tick_ms++;
    
    if (++s_acc_1ms >= 10) {
      s_acc_1ms = 0;
      g_flag_10ms = 1;
      
      if (++s_acc_10ms >= 5) {
        s_acc_10ms = 0;
        g_flag_50ms = 1;
      }
    }
  }
}

void timer_setup_1ms(void)
{
  nrf_drv_timer_config_t timer_cfg = NRF_DRV_TIMER_DEFAULT_CONFIG;
  timer_cfg.frequency = NRF_TIMER_FREQ_1MHz;  // 1MHz
  
  nrf_drv_timer_init(&TIMER_1MS, &timer_cfg, timer_1ms_event_handler);
  
  // 1000 ticks = 1ms
  nrf_drv_timer_extended_compare(&TIMER_1MS,
                                  NRF_TIMER_CC_CHANNEL0,
                                  1000,
                                  NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK,
                                  true);
  
  nrf_drv_timer_enable(&TIMER_1MS);
}

5. SAM (Arduino Due, Zero)
timer_hal_sam.c
c#include "sam.h"

void TC3_Handler(void)
{
  // 인터럽트 플래그 클리어
  TC_GetStatus(TC1, 0);
  
  // 공통 코드
  g_tick_ms++;
  
  if (++s_acc_1ms >= 10) {
    s_acc_1ms = 0;
    g_flag_10ms = 1;
    
    if (++s_acc_10ms >= 5) {
      s_acc_10ms = 0;
      g_flag_50ms = 1;
    }
  }
}

void timer_setup_1ms(void)
{
  pmc_set_writeprotect(false);
  pmc_enable_periph_clk(ID_TC3);
  
  // 84MHz / 2 / 42000 = 1000Hz (1ms)
  TC_Configure(TC1, 0,
               TC_CMR_WAVE |
               TC_CMR_WAVSEL_UP_RC |
               TC_CMR_TCCLKS_TIMER_CLOCK1);  // MCK/2
  
  TC_SetRC(TC1, 0, 42000);  // 42MHz / 42000 = 1kHz
  TC1->TC_CHANNEL[0].TC_IER = TC_IER_CPCS;
  TC1->TC_CHANNEL[0].TC_IDR = ~TC_IER_CPCS;
  
  NVIC_EnableIRQ(TC3_IRQn);
  TC_Start(TC1, 0);
}

📊 MCU별 타이머 비교표
MCU타이머클럭 소스PrescalerPeriod비고AVRTimer216MHz/642498비트 타이머STM32F4TIM284MHz/8499932비트 타이머ESP32Timer080MHz/801000FreeRTOS 주의RP2040Alarm1MHz-1000하드웨어 반복 타이머nRF52Timer016MHz/161000소프트디바이스와 충돌 주의SAMTC1 Ch084MHz/24200032비트 타이머

🔄 포팅 템플릿
모든 MCU에 적용 가능한 템플릿:
c/* ===== timer_hal_XXXX.c ===== */

// 외부에서 접근 가능한 변수들
extern volatile uint32_t g_tick_ms;
extern volatile uint8_t g_flag_10ms;
extern volatile uint8_t g_flag_50ms;

// ISR 내부 카운터 (static)
static uint8_t s_acc_1ms  = 0;
static uint8_t s_acc_10ms = 0;

// ===== 1. 타이머 초기화 함수 =====
void timer_setup_1ms(void)
{
  // TODO: 1ms 주기 타이머 설정
  // - 클럭 활성화
  // - Prescaler 계산
  // - Period/Compare 값 설정
  // - 인터럽트 활성화
}

// ===== 2. 1ms ISR 핸들러 =====
void TIMER_ISR_HANDLER(void)  // 이름은 MCU마다 다름
{
  // TODO: 인터럽트 플래그 클리어 (필요 시)
  
  // ===== 공통 코드 (모든 MCU 동일) =====
  g_tick_ms++;
  
  if (++s_acc_1ms >= 10) {
    s_acc_1ms = 0;
    g_flag_10ms = 1;
    
    if (++s_acc_10ms >= 5) {
      s_acc_10ms = 0;
      g_flag_50ms = 1;
    }
  }
  // ===== 공통 코드 끝 =====
}

✅ 포팅 검증 체크리스트
1단계: 타이머 주기 확인
cvoid setup() {
  Serial.begin(115200);
  timer_setup_1ms();
}

void loop() {
  static uint32_t last = 0;
  uint32_t now = g_tick_ms;
  
  if (now - last >= 1000) {  // 1초마다
    Serial.print("1 second = ");
    Serial.print(now - last);
    Serial.println(" ticks");
    last = now;
  }
}
예상 출력: 1 second = 1000 ticks (오차 ±1 tick)
2단계: 플래그 동작 확인
cvoid loop() {
  if (g_flag_10ms) {
    g_flag_10ms = 0;
    Serial.println("10ms");  // 100Hz
  }
  
  if (g_flag_50ms) {
    g_flag_50ms = 0;
    Serial.println("50ms");  // 20Hz
  }
}
3단계: 워크 스케줄러 테스트
cvoid test_callback(void* arg) {
  Serial.println("Work executed!");
}

void setup() {
  Serial.begin(115200);
  timer_setup_1ms();
  
  work_schedule_after(test_callback, NULL, 1000);
  Serial.println("Work scheduled for 1000ms");
}

void loop() {
  work_run_due(g_tick_ms);
}

🚨 주의사항
1. FreeRTOS 환경 (ESP32 등)
cpp// 크리티컬 섹션 보호 필수
void IRAM_ATTR timer_isr() {
  portENTER_CRITICAL_ISR(&timerMux);
  // ISR 코드
  portEXIT_CRITICAL_ISR(&timerMux);
}
2. 듀얼코어 MCU

모든 공유 변수는 volatile 선언
가능하면 ISR을 한 코어에 고정

3. 저전력 모드
c// 저전력 모드에서 타이머가 계속 동작하는지 확인
// 일부 MCU는 Sleep 시 타이머 정지
4. SoftDevice/BLE Stack (nRF52)
c// SoftDevice가 Timer0를 사용할 수 있음
// Timer1 또는 RTC 사용 권장

📦 완성된 포팅 예제
파일 구조
ultra_light_sched/
├── core/
│   ├── scheduler.c          (플랫폼 독립)
│   ├── scheduler.h
│   ├── timebase.c          (플랫폼 독립)
│   └── timebase.h
├── hal/
│   ├── timer_hal_avr.c     (AVR 전용)
│   ├── timer_hal_stm32.c   (STM32 전용)
│   ├── timer_hal_esp32.cpp (ESP32 전용)
│   └── timer_hal_xxx.c     (새 플랫폼)
└── examples/
    ├── blink_avr.ino
    ├── blink_stm32.cpp
    └── blink_esp32.ino

🎓 결론
플랫폼 독립적인 설계 덕분에 90% 이상의 코드를 재사용할 수 있습니다!
포팅에 필요한 작업:

✅ 타이머 초기화 함수 작성 (20줄)
✅ ISR 핸들러 작성 (10줄)
✅ 검증 테스트 (5분)

총 소요 시간: 30분~1시간
새로운 MCU로 포팅 시 이 가이드를 참고하세요!