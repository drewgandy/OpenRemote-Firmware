#pragma once
#include <map>
#include <string>
inline std::map<std::string, uint32_t> nvs;
struct Preferences {
  bool begin(const char *, bool) { return true; }
  void end() {}
  uint32_t get(const char *k, uint32_t d) { return nvs.count(k) ? nvs[k] : d; }
  bool getBool(const char *k, bool d) { return get(k, d); }
  uint8_t getUChar(const char *k, uint8_t d) { return get(k, d); }
  uint16_t getUShort(const char *k, uint16_t d) { return get(k, d); }
  uint32_t getUInt(const char *k, uint32_t d) { return get(k, d); }
  void putBool(const char *k, bool v) { nvs[k] = v; }
  void putUChar(const char *k, uint8_t v) { nvs[k] = v; }
  void putUShort(const char *k, uint16_t v) { nvs[k] = v; }
  void putUInt(const char *k, uint32_t v) { nvs[k] = v; }
};
