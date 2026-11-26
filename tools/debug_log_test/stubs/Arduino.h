// Just enough of Arduino and ESP-IDF to run remote/debug_log.h on a computer.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <string>
#define RTC_NOINIT_ATTR
#define RTC_DATA_ATTR
inline uint32_t fakeMillis = 0;
inline uint32_t millis() { return fakeMillis; }
inline void delay(uint32_t) {}
inline void *ps_malloc(size_t n) { return malloc(n); }
inline char *utoa(unsigned v, char *out, int) { sprintf(out, "%u", v); return out; }
struct String : std::string {
  String() {}
  String(const char *s) : std::string(s) {}
  String(const std::string &s) : std::string(s) {}
};
inline String operator+(const String &a, const char *b) { return String(std::string(a) + b); }
struct Print {
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t *d, size_t n) { for (size_t i = 0; i < n; i++) write(d[i]); return n; }
  size_t print(const char *s) { return write((const uint8_t *)s, strlen(s)); }
  size_t println(const char *s = "") { return print(s) + print("\r\n"); }
  size_t printf(const char *f, ...) {
    char b[512]; va_list a; va_start(a, f); vsnprintf(b, sizeof(b), f, a); va_end(a); return print(b);
  }
};
struct Stream : Print {
  virtual int available() { return 0; }
  virtual int read() { return -1; }
  virtual int peek() { return -1; }
  virtual void flush() {}
  virtual int availableForWrite() { return 0; }
};
struct HardwareSerial : Stream {
  size_t write(uint8_t) override { return 1; }
  size_t write(const uint8_t *, size_t n) override { return n; }
  void begin(unsigned long) {}
  size_t setRxBufferSize(size_t n) { return n; }
  template <typename F> void onReceiveError(F) {}
};
struct EspClass {
  unsigned getFreeHeap() { return 1; } unsigned getMinFreeHeap() { return 1; }
  unsigned getMaxAllocHeap() { return 1; } unsigned getFreePsram() { return 1; }
};
inline EspClass ESP;
