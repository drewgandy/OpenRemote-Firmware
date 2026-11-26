/*
  Debug log: everything the firmware prints to serial, saved to the SD card.

  - Lines collect in a PSRAM queue and reach the card in batches.
  - The newest ~4 KB are mirrored into RTC memory, which survives a panic, a
    watchdog reset, a software restart and deep sleep (not power loss or the
    reset button). Whatever had not reached the card is recovered at boot.
  - /logs/debug/YYYYMMDD-bNNNNN-pPPP.log: a new boot number for every real
    restart. A deep sleep wake carries on with the same one. A new part starts
    each day and at 1 MB. Kept for 7 days, 50 MB at most.
  - A crash (panic, watchdog, brownout) raises a flag that stays until it is
    cleared on the remote, whether or not logging is switched on.

  Include before the sketch redirects Serial to debugSerial.
*/
#pragma once

#include <Arduino.h>
#include <SD.h>
#include <Preferences.h>
#include <sys/time.h>
#include <time.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#if __has_include(<esp_core_dump.h>)
#include <esp_core_dump.h>
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH && CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF
#define OR_HAVE_COREDUMP 1
#endif
#endif
#include "debug_log_core.h"

static const char *DEBUG_LOG_DIR = "/logs/debug";
static const size_t DEBUG_LOG_RING_BYTES = 4096;
static const size_t DEBUG_LOG_QUEUE_BYTES = 32UL * 1024UL;
static const uint32_t DEBUG_LOG_PART_BYTES = 1024UL * 1024UL;
static const uint64_t DEBUG_LOG_TOTAL_BYTES = 50ULL * 1024ULL * 1024ULL;
static const int32_t DEBUG_LOG_KEEP_DAYS = 7;
// Under the 400 entries the SD browser lists, so every log stays reachable.
static const size_t DEBUG_LOG_MAX_FILES = 380;
static const uint32_t DEBUG_LOG_FLUSH_MS = 5000UL;
// With the screen off, batches go out far less often.
static const uint32_t DEBUG_LOG_IDLE_FLUSH_MS = 60000UL;
static const uint32_t DEBUG_LOG_HEALTH_MS = 60000UL;

// Defined by the sketch: the card is mounted and nothing else is writing it.
bool debugLogCardWritable();

// Survives panics, watchdog and software resets; random after power-on, which
// the record CRCs and the complemented copy below reject.
RTC_NOINIT_ATTR uint8_t debugRtcRing[DEBUG_LOG_RING_BYTES];
RTC_NOINIT_ATTR uint32_t debugRtcSavedSeq;
RTC_NOINIT_ATTR uint32_t debugRtcSavedSeqCheck;
// Survives deep sleep only: a real restart zeroes these.
RTC_DATA_ATTR dlog::LogName debugRtcFile = {0, 0, 0};
RTC_DATA_ATTR uint32_t debugRtcCleanupDay = 0;

struct DebugCrashFlag {
  bool active;
  uint8_t reason;      // esp_reset_reason_t of the latest crash
  uint16_t count;      // crashes since the flag was last cleared
  uint32_t epoch;      // when the latest one restarted the remote, 0 = unknown
};

struct DebugLogPartial {
  TaskHandle_t task;
  uint16_t used;
  char text[dlog::RECORD_MAX_TEXT + 1];
};

static const uint8_t DEBUG_LOG_PARTIALS = 5;
static DebugLogPartial debugLogPartials[DEBUG_LOG_PARTIALS] = {};
static portMUX_TYPE debugLogMux = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t debugLogFlushMutex = nullptr;
static volatile bool debugLogOn = true;
static char *debugLogFill = nullptr;
static char *debugLogOut = nullptr;
static volatile size_t debugLogFillUsed = 0;
static size_t debugLogOutUsed = 0;
static uint32_t debugLogFillSeq = 0;
static uint32_t debugLogOutSeq = 0;
static uint32_t debugLogNextSeq = 1;
static uint32_t debugLogRingPosition = 0;
static volatile uint32_t debugLogDropped = 0;
// Local time of day = (UTC + offset) % 86400, worked out in the loop so that
// printing from a small task stack never has to call localtime_r().
static volatile bool debugLogClockKnown = false;
static volatile int32_t debugLogDayOffset = 0;
static uint32_t debugLogToday = 0;
static bool debugLogCleanupDue = false;
static DebugCrashFlag debugCrash = {};
static bool debugCrashedThisBoot = false;

inline bool debugResetWasCrash(esp_reset_reason_t reason) {
  switch ((int)reason) {
    case ESP_RST_PANIC:
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
    case ESP_RST_BROWNOUT:
    case ESP_RST_CPU_LOCKUP:
      return true;
    default:
      return false;
  }
}

// ---------------------------------------------------------------------------
// Capture
// ---------------------------------------------------------------------------

static void debugLogTwoDigits(char *at, uint32_t value) {
  at[0] = (char)('0' + (value / 10) % 10);
  at[1] = (char)('0' + value % 10);
}

// "HH:MM:SS +uptime " at the start of a line.
static uint16_t debugLogPrefix(char *out) {
  memcpy(out, "--:--:-- +", 10);
  if (debugLogClockKnown) {
    struct timeval now;
    gettimeofday(&now, nullptr);
    uint32_t seconds = (uint32_t)(((int64_t)now.tv_sec + debugLogDayOffset) % 86400);
    debugLogTwoDigits(out, seconds / 3600);
    debugLogTwoDigits(out + 3, (seconds / 60) % 60);
    debugLogTwoDigits(out + 6, seconds % 60);
  }
  uint32_t ms = millis();
  utoa(ms / 1000UL, out + 10, 10);
  uint16_t used = (uint16_t)strlen(out);
  out[used++] = '.';
  debugLogTwoDigits(out + used, (ms % 1000UL) / 10UL);
  used += 2;
  out[used++] = ' ';
  return used;
}

// text must have room for one more byte: the newline.
static void debugLogCommit(char *text, uint16_t length) {
  text[length] = '\0';
  dlog::maskSecrets(text);
  text[length++] = '\n';
  portENTER_CRITICAL(&debugLogMux);
  uint32_t seq = debugLogNextSeq++;
  if (debugLogFill && debugLogFillUsed + length <= DEBUG_LOG_QUEUE_BYTES) {
    memcpy(debugLogFill + debugLogFillUsed, text, length);
    debugLogFillUsed += length;
    debugLogFillSeq = seq;
  } else {
    debugLogDropped++;
  }
  dlog::ringPut(debugRtcRing, DEBUG_LOG_RING_BYTES, debugLogRingPosition, seq, text, length);
  portEXIT_CRITICAL(&debugLogMux);
}

// Collects characters into whole lines, one buffer per task so that two tasks
// printing at once cannot splice each other's lines.
static void debugLogCapture(const uint8_t *data, size_t size) {
  if (!debugLogOn || !debugLogFill || xPortInIsrContext()) return;
  TaskHandle_t task = xTaskGetCurrentTaskHandle();
  DebugLogPartial *slot = nullptr;
  portENTER_CRITICAL(&debugLogMux);
  for (uint8_t i = 0; i < DEBUG_LOG_PARTIALS; i++) {
    if (debugLogPartials[i].task == task) slot = &debugLogPartials[i];
  }
  for (uint8_t i = 0; !slot && i < DEBUG_LOG_PARTIALS; i++) {
    if (!debugLogPartials[i].task) {
      slot = &debugLogPartials[i];
      slot->task = task;
      slot->used = 0;
    }
  }
  portEXIT_CRITICAL(&debugLogMux);
  if (!slot) {
    debugLogDropped++;
    return;
  }
  for (size_t i = 0; i < size; i++) {
    char c = (char)data[i];
    if (c == '\r') continue;
    if (c != '\n') {
      if (!slot->used) slot->used = debugLogPrefix(slot->text);
      slot->text[slot->used++] = c;
    }
    // One byte short of the record limit leaves room for the newline.
    if ((c == '\n' && slot->used) || slot->used >= dlog::RECORD_MAX_TEXT - 1) {
      debugLogCommit(slot->text, slot->used);
      slot->used = 0;
    }
  }
  if (!slot->used) slot->task = nullptr;
}

// Stands in for Serial: same bytes to the UART, and a copy to the log. The USB
// Studio protocol is handed the UART itself, so file transfers stay out.
class DebugSerialPort : public Stream {
public:
  explicit DebugSerialPort(HardwareSerial &port) : port_(port) {}
  void begin(unsigned long baud) { port_.begin(baud); }
  size_t setRxBufferSize(size_t size) { return port_.setRxBufferSize(size); }
  template <typename Callback> void onReceiveError(Callback callback) {
    port_.onReceiveError(callback);
  }
  int available() override { return port_.available(); }
  int read() override { return port_.read(); }
  int peek() override { return port_.peek(); }
  void flush() override { port_.flush(); }
  int availableForWrite() override { return port_.availableForWrite(); }
  size_t write(uint8_t byte) override {
    size_t sent = port_.write(byte);
    debugLogCapture(&byte, 1);
    return sent;
  }
  size_t write(const uint8_t *data, size_t size) override {
    size_t sent = port_.write(data, size);
    debugLogCapture(data, size);
    return sent;
  }
  using Print::write;

private:
  HardwareSerial &port_;
};

// ---------------------------------------------------------------------------
// Boot, settings, crash flag
// ---------------------------------------------------------------------------

static void debugLogMarkSaved(uint32_t seq) {
  debugRtcSavedSeq = seq;
  debugRtcSavedSeqCheck = ~seq;
}

static void debugCrashSave() {
  Preferences store;
  if (!store.begin("ordebug", false)) return;
  store.putBool("cfOn", debugCrash.active);
  store.putUChar("cfWhy", debugCrash.reason);
  store.putUShort("cfN", debugCrash.count);
  store.putUInt("cfAt", debugCrash.epoch);
  store.end();
}

const DebugCrashFlag &debugCrashFlag() { return debugCrash; }

void debugCrashClear() {
  debugCrash = {};
  debugCrashedThisBoot = false;
  debugCrashSave();
}

uint32_t debugLogBootNumber() { return debugRtcFile.boot; }
bool debugLogEnabled() { return debugLogOn; }

// First thing in setup(), before anything is printed.
void debugLogBegin() {
  esp_reset_reason_t reason = esp_reset_reason();
  debugLogFlushMutex = xSemaphoreCreateMutex();
  char *fill = (char *)ps_malloc(DEBUG_LOG_QUEUE_BYTES);
  char *out = (char *)ps_malloc(DEBUG_LOG_QUEUE_BYTES);

  // Recover before the first new line can overwrite anything.
  uint32_t saved = debugRtcSavedSeq == ~debugRtcSavedSeqCheck ? debugRtcSavedSeq : 0;
  uint32_t newest = 0;
  size_t recovered = 0;
  const size_t scratchCount = DEBUG_LOG_RING_BYTES / (dlog::RECORD_HEADER + 1);
  dlog::RingRecord *scratch =
    (dlog::RingRecord *)malloc(scratchCount * sizeof(dlog::RingRecord));
  char *text = (char *)ps_malloc(DEBUG_LOG_RING_BYTES);
  if (scratch && text) {
    recovered = dlog::ringRecover(debugRtcRing, DEBUG_LOG_RING_BYTES, saved, scratch,
                                  scratchCount, text, DEBUG_LOG_RING_BYTES, newest,
                                  debugLogRingPosition);
  }
  debugLogNextSeq = (newest > saved ? newest : saved) + 1;
  if (fill && out && recovered) {
    bool restarted = reason != ESP_RST_DEEPSLEEP;
    size_t used = 0;
    if (restarted) {
      used += snprintf(fill, DEBUG_LOG_QUEUE_BYTES,
                       "---- not yet saved before this restart ----\n");
    }
    memcpy(fill + used, text, recovered);
    used += recovered;
    if (restarted) {
      used += snprintf(fill + used, DEBUG_LOG_QUEUE_BYTES - used,
                       "---- end of recovered lines ----\n");
    }
    debugLogFillUsed = used;
    debugLogFillSeq = newest;
  }
  free(scratch);
  free(text);
  debugLogOut = out;
  debugLogFill = out ? fill : nullptr;

  Preferences store;
  if (store.begin("ordebug", false)) {
    if (reason != ESP_RST_DEEPSLEEP || !debugRtcFile.boot) {
      uint32_t boot = store.getUInt("boot", 0) + 1;
      store.putUInt("boot", boot);
      debugRtcFile = {0, boot, 1};
      debugLogCleanupDue = true;
    }
    debugCrash.active = store.getBool("cfOn", false);
    debugCrash.reason = store.getUChar("cfWhy", 0);
    debugCrash.count = store.getUShort("cfN", 0);
    debugCrash.epoch = store.getUInt("cfAt", 0);
    store.end();
  }
  if (debugResetWasCrash(reason)) {
    debugCrashedThisBoot = true;
    debugCrash.active = true;
    debugCrash.reason = (uint8_t)reason;
    if (debugCrash.count < UINT16_MAX) debugCrash.count++;
    debugCrash.epoch = 0;
    debugCrashSave();
  }
}

// Switching off discards what has not been saved, including the RTC copy.
void debugLogSetEnabled(bool enabled) {
  if (debugLogOn == enabled) return;
  portENTER_CRITICAL(&debugLogMux);
  debugLogOn = enabled;
  if (!enabled) {
    debugLogFillUsed = 0;
    memset(debugRtcRing, 0, DEBUG_LOG_RING_BYTES);
    for (uint8_t i = 0; i < DEBUG_LOG_PARTIALS; i++) debugLogPartials[i].used = 0;
  } else {
    debugLogCleanupDue = true;
  }
  portEXIT_CRITICAL(&debugLogMux);
  if (!enabled && debugLogFlushMutex &&
      xSemaphoreTake(debugLogFlushMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
    debugLogOutUsed = 0;
    xSemaphoreGive(debugLogFlushMutex);
  }
}

// Prints where the last crash happened, from the core dump the panic handler
// wrote to flash. Addresses decode against the build's firmware.elf.
void debugLogReportCrash(Print &out) {
  esp_reset_reason_t reason = esp_reset_reason();
  if (!debugResetWasCrash(reason) || reason == ESP_RST_BROWNOUT) return;
#ifdef OR_HAVE_COREDUMP
  // Left in flash while logging is off, so it is not thrown away unread.
  if (!debugLogOn) return;
  if (esp_core_dump_image_check() != ESP_OK) {
    out.println("Crash: no core dump was saved");
    return;
  }
  esp_core_dump_summary_t *summary =
    (esp_core_dump_summary_t *)malloc(sizeof(esp_core_dump_summary_t));
  if (summary && esp_core_dump_get_summary(summary) == ESP_OK) {
    out.printf("Crash: task '%.16s' pc=0x%08lx cause=%lu address=0x%08lx\n",
               summary->exc_task, (unsigned long)summary->exc_pc,
               (unsigned long)summary->ex_info.exc_cause,
               (unsigned long)summary->ex_info.exc_vaddr);
    out.print("Crash backtrace:");
    for (uint32_t i = 0; i < summary->exc_bt_info.depth && i < 16; i++) {
      out.printf(" 0x%08lx", (unsigned long)summary->exc_bt_info.bt[i]);
    }
    out.println(summary->exc_bt_info.corrupted ? " (corrupted)" : "");
    out.printf("Crash build id: %s\n", (const char *)summary->app_elf_sha256);
    char why[120] = {};
    if (esp_core_dump_get_panic_reason(why, sizeof(why)) == ESP_OK) {
      out.printf("Crash reason: %s\n", why);
    }
    esp_core_dump_image_erase();
  } else {
    out.println("Crash: the core dump could not be read");
  }
  free(summary);
#else
  out.println("Crash: this build does not save core dumps");
#endif
}

// ---------------------------------------------------------------------------
// The card
// ---------------------------------------------------------------------------

static String debugLogPath(const dlog::LogName &name) {
  char file[40];
  dlog::formatLogName(name, file, sizeof(file));
  return String(DEBUG_LOG_DIR) + "/" + file;
}

static File debugLogOpen() {
  String path = debugLogPath(debugRtcFile);
  File file = SD.open(path, FILE_APPEND);
  if (!file) {
    SD.mkdir("/logs");
    SD.mkdir(DEBUG_LOG_DIR);
    file = SD.open(path, FILE_APPEND);
  }
  return file;
}

// Writes one batch. Safe from the loop and the web server task.
bool debugLogFlush() {
  if (!debugLogFlushMutex ||
      xSemaphoreTake(debugLogFlushMutex, pdMS_TO_TICKS(500)) != pdTRUE) {
    return false;
  }
  bool ok = true;
  // Up to two passes: a batch left over from a failed write, then the new one.
  for (uint8_t pass = 0; pass < 2 && ok; pass++) {
    if (!debugLogOn || !debugLogCardWritable()) {
      ok = false;
      break;
    }
    if (!debugLogOutUsed) {
      portENTER_CRITICAL(&debugLogMux);
      if (debugLogFillUsed) {
        char *swap = debugLogOut;
        debugLogOut = debugLogFill;
        debugLogFill = swap;
        debugLogOutUsed = debugLogFillUsed;
        debugLogOutSeq = debugLogFillSeq;
        debugLogFillUsed = 0;
      }
      portEXIT_CRITICAL(&debugLogMux);
    }
    if (!debugLogOutUsed) break;

    uint32_t today = debugLogToday;
    if (today && debugRtcFile.date != today) {
      if (!debugRtcFile.date) {
        // The clock arrived after the file was started: give it its date.
        dlog::LogName dated = debugRtcFile;
        dated.date = today;
        String from = debugLogPath(debugRtcFile);
        if (!SD.exists(from) || SD.rename(from, debugLogPath(dated))) debugRtcFile = dated;
      } else {
        debugRtcFile.date = today;
        debugRtcFile.part++;
        debugLogCleanupDue = true;
      }
    }
    File file = debugLogOpen();
    if (file && file.size() >= DEBUG_LOG_PART_BYTES) {
      file.close();
      debugRtcFile.part++;
      debugLogCleanupDue = true;
      file = debugLogOpen();
    }
    if (!file) {
      ok = false;
      break;
    }
    if (debugLogDropped) {
      file.printf("---- %lu line(s) lost: the log queue was full ----\n",
                  (unsigned long)debugLogDropped);
      debugLogDropped = 0;
    }
    ok = file.write((const uint8_t *)debugLogOut, debugLogOutUsed) == debugLogOutUsed;
    file.close();
    if (ok) {
      debugLogMarkSaved(debugLogOutSeq);
      debugLogOutUsed = 0;
    }
  }
  xSemaphoreGive(debugLogFlushMutex);
  return ok;
}

static void debugLogCleanup() {
  if (!debugLogCardWritable()) return;
  const size_t capacity = 600;
  dlog::LogFile *files = (dlog::LogFile *)ps_malloc(capacity * sizeof(dlog::LogFile));
  if (!files) return;
  if (xSemaphoreTake(debugLogFlushMutex, pdMS_TO_TICKS(500)) != pdTRUE) {
    free(files);
    return;
  }
  size_t count = 0;
  File dir = SD.open(DEBUG_LOG_DIR);
  while (dir && count < capacity) {
    File entry = dir.openNextFile();
    if (!entry) break;
    const char *slash = strrchr(entry.name(), '/');
    dlog::LogFile found = {};
    if (!entry.isDirectory() &&
        dlog::parseLogName(slash ? slash + 1 : entry.name(), found.name)) {
      found.size = (uint32_t)entry.size();
      found.current = found.name.boot == debugRtcFile.boot &&
                      found.name.part == debugRtcFile.part;
      files[count++] = found;
    }
    entry.close();
    if ((count % 32) == 0) delay(1);
  }
  if (dir) dir.close();
  // A file from a run that never learned the date starts ageing from today,
  // so it cannot stay for ever.
  for (size_t i = 0; debugLogToday && i < count; i++) {
    if (files[i].current || files[i].name.date) continue;
    dlog::LogName dated = files[i].name;
    dated.date = debugLogToday;
    if (SD.rename(debugLogPath(files[i].name), debugLogPath(dated))) files[i].name = dated;
  }
  dlog::planRetention(files, count, debugLogToday, DEBUG_LOG_KEEP_DAYS,
                      DEBUG_LOG_TOTAL_BYTES, DEBUG_LOG_MAX_FILES);
  for (size_t i = 0; i < count; i++) {
    if (!files[i].remove) continue;
    SD.remove(debugLogPath(files[i].name));
    delay(1);
  }
  debugRtcCleanupDay = debugLogToday;
  debugLogCleanupDue = false;
  xSemaphoreGive(debugLogFlushMutex);
  free(files);
}

// Keeps the time-of-day offset and today's date current.
void debugLogSetClock(bool trusted) {
  time_t now = time(nullptr);
  if (!trusted || now < 1700000000) {
    debugLogClockKnown = false;
    debugLogToday = 0;
    return;
  }
  struct tm local;
  localtime_r(&now, &local);
  int32_t localSeconds = local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec;
  debugLogDayOffset = (localSeconds - (int32_t)(now % 86400) + 86400) % 86400;
  debugLogToday = (uint32_t)(local.tm_year + 1900) * 10000UL +
                  (uint32_t)(local.tm_mon + 1) * 100UL + (uint32_t)local.tm_mday;
  debugLogClockKnown = true;
  if (debugCrashedThisBoot && debugCrash.active && !debugCrash.epoch) {
    debugCrash.epoch = (uint32_t)now - millis() / 1000UL;
    debugCrashSave();
  }
}

// Every pass of loop(). Nothing here wakes the remote: while the screen is off
// it only saves what other code has already printed.
void debugLogService(bool clockTrusted, bool screenOn, Print &out) {
  static uint32_t lastClockMs = 0;
  static uint32_t lastFlushMs = 0;
  static uint32_t lastHealthMs = 0;
  uint32_t now = millis();
  if (!lastClockMs || now - lastClockMs >= 1000UL) {
    lastClockMs = now;
    debugLogSetClock(clockTrusted);
  }
  if (!debugLogOn) return;
  if (screenOn && (!lastHealthMs || now - lastHealthMs >= DEBUG_LOG_HEALTH_MS)) {
    lastHealthMs = now;
    out.printf("Health: heap=%u lowest=%u largest=%u psram=%u\n",
               (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
               (unsigned)ESP.getMaxAllocHeap(), (unsigned)ESP.getFreePsram());
  }
  size_t pending = debugLogFillUsed + debugLogOutUsed;
  if (pending && (debugLogFillUsed >= DEBUG_LOG_QUEUE_BYTES / 4 ||
                  now - lastFlushMs >= (screenOn ? DEBUG_LOG_FLUSH_MS
                                                 : DEBUG_LOG_IDLE_FLUSH_MS))) {
    lastFlushMs = now;
    debugLogFlush();
  }
  // Once the boot is well out of the way, and again when the date changes.
  if (now >= 15000UL &&
      (debugLogCleanupDue || (debugLogToday && debugLogToday != debugRtcCleanupDay))) {
    static uint32_t lastCleanupMs = 0;
    if (!lastCleanupMs || now - lastCleanupMs >= 60000UL) {
      lastCleanupMs = now;
      debugLogCleanup();
    }
  }
}
