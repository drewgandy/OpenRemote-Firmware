#pragma once
inline TaskHandle_t xTaskGetCurrentTaskHandle() { return (TaskHandle_t)(long)fakeTask; }
