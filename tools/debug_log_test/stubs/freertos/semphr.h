#pragma once
inline int heldMutex = 0;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return &heldMutex; }
inline int xSemaphoreTake(SemaphoreHandle_t, int) { if (heldMutex) return 0; heldMutex = 1; return 1; }
inline void xSemaphoreGive(SemaphoreHandle_t) { heldMutex = 0; }
