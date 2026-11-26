#pragma once
typedef void *TaskHandle_t; typedef void *SemaphoreHandle_t;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(m) (void)(m)
#define portEXIT_CRITICAL(m) (void)(m)
#define pdTRUE 1
#define pdMS_TO_TICKS(x) (x)
inline bool xPortInIsrContext() { return false; }
inline int fakeTask = 1;
