// Runs remote/debug_log.h through restarts against a pretend card.
//   g++ -std=c++17 -fsanitize=address,undefined -Istubs -I../../remote sim.cpp -o /tmp/dls && /tmp/dls
#include "debug_log.h"
#include <assert.h>

bool writable = true;
bool debugLogCardWritable() { return writable; }
HardwareSerial uart;
DebugSerialPort serial(uart);

// What a restart does to memory. RTC_DATA is lost unless it was deep sleep.
void boot(esp_reset_reason_t reason) {
  free(debugLogFill); free(debugLogOut);
  debugLogFill = debugLogOut = nullptr;
  debugLogFillUsed = debugLogOutUsed = 0;
  debugLogNextSeq = 1; debugLogRingPosition = 0; debugLogDropped = 0;
  memset(debugLogPartials, 0, sizeof(debugLogPartials));
  debugLogOn = true; debugLogClockKnown = false; debugLogToday = 0;
  debugLogCleanupDue = false; debugCrash = {}; debugCrashedThisBoot = false; heldMutex = 0;
  if (reason != ESP_RST_DEEPSLEEP) { debugRtcFile = {0, 0, 0}; debugRtcCleanupDay = 0; }
  fakeReset = reason; fakeMillis = 10;
  debugLogBegin();
}
size_t times(const std::string &s, const std::string &needle) {
  size_t n = 0;
  for (size_t at = s.find(needle); at != std::string::npos; at = s.find(needle, at + 1)) n++;
  return n;
}
std::string everything() { std::string s; for (auto &e : card) s += e.second; return s; }

int main() {
  setenv("TZ", "EST5", 1); tzset();
  srand(7);
  for (auto &b : debugRtcRing) b = rand();
  debugRtcSavedSeq = rand(); debugRtcSavedSeqCheck = rand();

  boot(ESP_RST_POWERON);
  assert(!debugCrashFlag().active && debugLogBootNumber() == 1);
  serial.println("alpha");
  // Two tasks printing in pieces keep their own lines.
  serial.print("one "); fakeTask = 2; serial.print("two "); fakeTask = 1; serial.println("ONE");
  fakeTask = 2; serial.println("TWO"); fakeTask = 1;
  serial.println("url ?token=sekrit");
  assert(debugLogFlush() && card.size() == 1);
  assert(card.begin()->first == "/logs/debug/00000000-b00001-p001.log");
  std::string s = everything();
  assert(s.find("alpha\n") != std::string::npos && s.find("one ONE\n") != std::string::npos);
  assert(s.find("two TWO\n") != std::string::npos && s.find("sekrit") == std::string::npos);

  // The clock arrives: the file takes its date.
  debugLogSetClock(true);
  serial.println("dated");
  assert(debugLogFlush() && card.size() == 1);
  assert(card.begin()->first != "/logs/debug/00000000-b00001-p001.log");

  // Deep sleep: same file, unsaved lines carried over without a banner.
  serial.println("going to sleep");
  boot(ESP_RST_DEEPSLEEP);
  debugLogSetClock(true);
  serial.println("awake");
  assert(debugLogFlush() && card.size() == 1 && debugLogBootNumber() == 1);
  s = everything();
  assert(times(s, "going to sleep") == 1 && times(s, "alpha") == 1 && times(s, "---- not yet") == 0);

  // Crash before saving: new file holds the lost lines, flag raised.
  writable = false;
  serial.println("last words");
  assert(!debugLogFlush());
  writable = true;
  boot(ESP_RST_TASK_WDT);
  assert(debugCrashFlag().active && debugCrashFlag().count == 1 && !debugCrashFlag().epoch);
  debugLogSetClock(true);
  assert(debugCrashFlag().epoch > 0);
  serial.println("back");
  assert(debugLogFlush() && card.size() == 2 && debugLogBootNumber() == 2);
  s = everything();
  assert(times(s, "last words") == 1 && times(s, "---- not yet") == 1 && times(s, "alpha") == 1);

  // The flag survives a normal restart, counts crashes and clears.
  boot(ESP_RST_SW);
  assert(debugCrashFlag().active && debugCrashFlag().count == 1);
  boot(ESP_RST_PANIC);
  assert(debugCrashFlag().count == 2);
  debugCrashClear();
  boot(ESP_RST_DEEPSLEEP);
  assert(!debugCrashFlag().active);

  // A failed write keeps the batch for later.
  cardFails = true;
  serial.println("kept");
  assert(!debugLogFlush());
  cardFails = false;
  assert(debugLogFlush() && times(everything(), "kept") == 1);

  // Parts roll at 1 MB; cleanup holds the total near 50 MB, age at 7 days.
  debugLogSetClock(true);
  for (int i = 0; i < 400000; i++) {
    serial.printf("filler %06d %s\n", i, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    if (i % 50 == 0) assert(debugLogFlush());
  }
  assert(debugLogFlush());
  card["/logs/debug/20200101-b00000-p001.log"] = "ancient";
  debugLogCleanup();
  uint64_t total = 0;
  for (auto &e : card) { total += e.second.size(); assert(e.second.size() < DEBUG_LOG_PART_BYTES + 40000); }
  assert(total <= DEBUG_LOG_TOTAL_BYTES && total > DEBUG_LOG_TOTAL_BYTES - 3000000);
  assert(!card.count("/logs/debug/20200101-b00000-p001.log"));
  assert(times(everything(), "filler 399999") == 1);

  // Off: nothing is written or carried over.
  debugLogFlush();
  size_t files = card.size(); total = everything().size();
  serial.println("unsaved");
  debugLogSetEnabled(false);
  serial.println("silent");
  debugLogFlush();
  boot(ESP_RST_PANIC);
  assert(debugCrashFlag().active);   // the flag does not depend on logging
  debugLogFlush();
  assert(times(everything(), "silent") == 0 && times(everything(), "unsaved") == 0);
  puts("debug_log: simulation passed");
  free(debugLogFill); free(debugLogOut);
  return 0;
}
