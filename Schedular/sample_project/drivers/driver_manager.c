/* driver_manager.c */
#include "driver_manager.h"
#include <string.h>
#include <Arduino.h>

// 최대 드라이버 수
#ifndef MAX_DRIVERS
#define MAX_DRIVERS 16
#endif

// 전역 드라이버 테이블
static driver_descriptor_t g_drivers[MAX_DRIVERS];
static int g_driver_count = 0;

// 외부 스케줄러 변수 (ultra_light_sched에서 제공)
extern volatile uint8_t g_flag_10ms;
extern volatile uint8_t g_flag_50ms;

// ===== 내부 함수 =====

static driver_descriptor_t* find_driver(const char* name)
{
  for (int i = 0; i < g_driver_count; i++) {
    if (strcmp(g_drivers[i].name, name) == 0) {
      return &g_drivers[i];
    }
  }
  return NULL;
}

// ===== 공개 API 구현 =====

int driver_register(const char* name,
                   driver_init_fn_t init_fn,
                   driver_task_fn_t task_fn,
                   uint8_t period_ms)
{
  // 파라미터 검증
  if (!name) {
    Serial.println(F("[DRV] ERROR: name is NULL"));
    return -2;
  }
  
  if (period_ms != 10 && period_ms != 50) {
    Serial.print(F("[DRV] ERROR: Invalid period "));
    Serial.println(period_ms);
    return -2;
  }
  
  // 슬롯 확인
  if (g_driver_count >= MAX_DRIVERS) {
    Serial.println(F("[DRV] ERROR: Driver slots full"));
    return -1;
  }
  
  // 중복 확인
  if (find_driver(name)) {
    Serial.print(F("[DRV] WARNING: Driver '"));
    Serial.print(name);
    Serial.println(F("' already registered"));
    return -2;
  }
  
  // 드라이버 등록
  driver_descriptor_t* drv = &g_drivers[g_driver_count];
  drv->name = name;
  drv->init_fn = init_fn;
  drv->task_fn = task_fn;
  drv->period_ms = period_ms;
  drv->enabled = 0;        // 기본 비활성
  drv->initialized = 0;
  
  Serial.print(F("[DRV] Registering '"));
  Serial.print(name);
  Serial.print(F("' @ "));
  Serial.print(period_ms);
  Serial.print(F("ms"));
  
  // 초기화 함수 실행
  if (init_fn) {
    int ret = init_fn();
    if (ret != 0) {
      Serial.print(F(" - Init FAILED ("));
      Serial.print(ret);
      Serial.println(F(")"));
      return -3;
    }
    drv->initialized = 1;
  }
  
  // 등록 완료 후 자동 활성화
  drv->enabled = 1;
  g_driver_count++;
  
  Serial.println(F(" - OK"));
  return 0;
}

int driver_unregister(const char* name)
{
  for (int i = 0; i < g_driver_count; i++) {
    if (strcmp(g_drivers[i].name, name) == 0) {
      // 배열에서 제거 (뒤에 있는 것들을 앞으로 이동)
      for (int j = i; j < g_driver_count - 1; j++) {
        g_drivers[j] = g_drivers[j + 1];
      }
      g_driver_count--;
      
      Serial.print(F("[DRV] Unregistered '"));
      Serial.print(name);
      Serial.println(F("'"));
      return 0;
    }
  }
  
  return -1;
}

int driver_set_enable(const char* name, bool enable)
{
  driver_descriptor_t* drv = find_driver(name);
  if (!drv) return -1;
  
  drv->enabled = enable ? 1 : 0;
  
  Serial.print(F("[DRV] '"));
  Serial.print(name);
  Serial.print(F("' "));
  Serial.println(enable ? F("ENABLED") : F("DISABLED"));
  
  return 0;
}

void driver_manager_run(void)
{
  // 10ms 태스크 실행
  if (g_flag_10ms) {
    g_flag_10ms = 0;
    
    for (int i = 0; i < g_driver_count; i++) {
      driver_descriptor_t* drv = &g_drivers[i];
      if (drv->enabled && drv->task_fn && drv->period_ms == 10) {
        drv->task_fn();
      }
    }
  }
  
  // 50ms 태스크 실행
  if (g_flag_50ms) {
    g_flag_50ms = 0;
    
    for (int i = 0; i < g_driver_count; i++) {
      driver_descriptor_t* drv = &g_drivers[i];
      if (drv->enabled && drv->task_fn && drv->period_ms == 50) {
        drv->task_fn();
      }
    }
  }
}

void driver_manager_list(void)
{
  Serial.println(F("\n===== Driver List ====="));
  Serial.print(F("Total: "));
  Serial.print(g_driver_count);
  Serial.print(F(" / "));
  Serial.println(MAX_DRIVERS);
  
  for (int i = 0; i < g_driver_count; i++) {
    driver_descriptor_t* drv = &g_drivers[i];
    
    Serial.print(F("["));
    Serial.print(i);
    Serial.print(F("] "));
    Serial.print(drv->name);
    Serial.print(F(" - "));
    Serial.print(drv->period_ms);
    Serial.print(F("ms - "));
    Serial.print(drv->enabled ? F("ENABLED") : F("DISABLED"));
    Serial.print(F(" - "));
    Serial.println(drv->initialized ? F("INIT OK") : F("NO INIT"));
  }
  
  Serial.println(F("=======================\n"));
}