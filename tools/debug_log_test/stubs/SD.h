#pragma once
#include "Arduino.h"
#include <map>
#include <vector>
#define FILE_APPEND "a"
inline std::map<std::string, std::string> card;
inline bool cardFails = false;
struct File {
  std::string path; bool open = false, dir = false; std::vector<std::string> list; size_t at = 0;
  explicit operator bool() const { return open; }
  size_t size() { return card[path].size(); }
  size_t write(const uint8_t *d, size_t n) { card[path].append((const char *)d, n); return n; }
  size_t printf(const char *f, ...) {
    char b[256]; va_list a; va_start(a, f); vsnprintf(b, sizeof(b), f, a); va_end(a);
    return write((const uint8_t *)b, strlen(b));
  }
  void close() { open = false; }
  bool isDirectory() { return dir; }
  const char *name() { return path.c_str() + path.rfind('/') + 1; }
  File openNextFile() { File f; if (at < list.size()) { f.path = list[at++]; f.open = true; } return f; }
};
struct SDClass {
  File open(const std::string &p, const char *mode = "r") {
    File f; if (cardFails) return f; f.path = p;
    if (std::string(mode) == "a") { card[p]; f.open = true; return f; }
    if (card.count(p)) { f.open = true; return f; }
    for (auto &e : card) if (e.first.rfind(p + "/", 0) == 0) f.list.push_back(e.first);
    f.open = f.dir = true; return f;
  }
  bool mkdir(const char *) { return true; }
  bool exists(const std::string &p) { return card.count(p); }
  bool rename(const std::string &a, const std::string &b) {
    if (!card.count(a) || card.count(b)) return false; card[b] = card[a]; card.erase(a); return true; }
  bool remove(const std::string &p) { return card.erase(p); }
};
inline SDClass SD;
