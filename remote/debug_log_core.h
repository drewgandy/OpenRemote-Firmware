/*
  The parts of the debug log that need no hardware: the crash-surviving ring,
  secret masking, file names and what to delete. Kept apart from debug_log.h so
  tools/debug_log_test can build and run them on a computer.
*/
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

namespace dlog {

// ---------------------------------------------------------------------------
// Ring of self-describing records
//
// There is deliberately no head or tail pointer to trust after a crash. Each
// record carries a marker, its length, a sequence number and a CRC, so recovery
// scans the whole ring and keeps whatever checks out. A record cut off by a
// crash, or partly overwritten by a newer one, fails its CRC and is skipped.
// Random power-on memory fails the same way.
// ---------------------------------------------------------------------------

static const uint8_t MARK0 = 0xA5;
static const uint8_t MARK1 = 0x5A;
static const size_t RECORD_HEADER = 9;      // mark(2) len(1) seq(4) crc(2)
static const size_t RECORD_MAX_TEXT = 240;

inline uint16_t crc16(uint16_t crc, uint8_t byte) {
  crc ^= (uint16_t)byte << 8;
  for (uint8_t i = 0; i < 8; i++) {
    crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}

// Writes one record at *position (which may hold anything; it is wrapped).
inline void ringPut(volatile uint8_t *ring, size_t size, uint32_t &position,
                    uint32_t seq, const char *text, size_t length) {
  if (length > RECORD_MAX_TEXT) length = RECORD_MAX_TEXT;
  if (!length || size < RECORD_HEADER + length) return;
  uint8_t header[RECORD_HEADER] = {
    MARK0, MARK1, (uint8_t)length,
    (uint8_t)seq, (uint8_t)(seq >> 8), (uint8_t)(seq >> 16), (uint8_t)(seq >> 24),
    0, 0};
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 2; i < 7; i++) crc = crc16(crc, header[i]);
  for (size_t i = 0; i < length; i++) crc = crc16(crc, (uint8_t)text[i]);
  header[7] = (uint8_t)crc;
  header[8] = (uint8_t)(crc >> 8);
  size_t at = position % size;
  // Text first, header last: until the header lands the record does not exist.
  for (size_t i = 0; i < length; i++) {
    ring[(at + RECORD_HEADER + i) % size] = (uint8_t)text[i];
  }
  for (size_t i = 0; i < RECORD_HEADER; i++) ring[(at + i) % size] = header[i];
  position = (uint32_t)((at + RECORD_HEADER + length) % size);
}

struct RingRecord {
  uint32_t seq;
  uint16_t offset;
  uint8_t length;
};

inline int compareRecords(const void *a, const void *b) {
  uint32_t x = static_cast<const RingRecord *>(a)->seq;
  uint32_t y = static_cast<const RingRecord *>(b)->seq;
  return x < y ? -1 : (x > y ? 1 : 0);
}

inline bool ringRecordAt(const volatile uint8_t *ring, size_t size, size_t at,
                         RingRecord &record) {
  if (ring[at] != MARK0 || ring[(at + 1) % size] != MARK1) return false;
  uint8_t length = ring[(at + 2) % size];
  if (!length || length > RECORD_MAX_TEXT || RECORD_HEADER + length > size) return false;
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 2; i < 7; i++) crc = crc16(crc, ring[(at + i) % size]);
  for (size_t i = 0; i < length; i++) {
    crc = crc16(crc, ring[(at + RECORD_HEADER + i) % size]);
  }
  uint16_t stored = (uint16_t)(ring[(at + 7) % size] | (ring[(at + 8) % size] << 8));
  if (crc != stored) return false;
  record.seq = 0;
  for (uint8_t i = 0; i < 4; i++) {
    record.seq |= (uint32_t)ring[(at + 3 + i) % size] << (8 * i);
  }
  record.offset = (uint16_t)at;
  record.length = length;
  return true;
}

/*
  Copies the text of every intact record newer than afterSeq into out, oldest
  first. scratch holds the record list: size / (RECORD_HEADER + 1) entries is
  enough. maxSeq and nextPosition let the caller carry on writing where the
  newest record ended; both are left at 0 when the ring holds nothing.
*/
inline size_t ringRecover(const volatile uint8_t *ring, size_t size, uint32_t afterSeq,
                          RingRecord *scratch, size_t scratchCount,
                          char *out, size_t outSize,
                          uint32_t &maxSeq, uint32_t &nextPosition) {
  size_t count = 0;
  maxSeq = 0;
  nextPosition = 0;
  for (size_t at = 0; at < size && count < scratchCount; at++) {
    RingRecord record;
    if (!ringRecordAt(ring, size, at, record)) continue;
    scratch[count++] = record;
    if (record.seq >= maxSeq) {
      maxSeq = record.seq;
      nextPosition = (uint32_t)((at + RECORD_HEADER + record.length) % size);
    }
  }
  qsort(scratch, count, sizeof(RingRecord), compareRecords);
  size_t used = 0;
  for (size_t i = 0; i < count; i++) {
    if (scratch[i].seq <= afterSeq || used + scratch[i].length > outSize) continue;
    for (size_t j = 0; j < scratch[i].length; j++) {
      out[used++] = (char)ring[(scratch[i].offset + RECORD_HEADER + j) % size];
    }
  }
  return used;
}

// ---------------------------------------------------------------------------
// Secrets
// ---------------------------------------------------------------------------

// Stars out whatever follows a credential key. Length is unchanged, so it works
// in place. A safety net: the firmware is not meant to print secrets at all.
inline void maskSecrets(char *line) {
  static const char *keys[] = {"token=", "password=", "passwd=", "psk=", "bearer ",
                               "\"token\":\"", "\"password\":\""};
  for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
    size_t keyLength = strlen(keys[k]);
    for (char *at = line; *at; at++) {
      size_t i = 0;
      while (i < keyLength && at[i] && tolower((unsigned char)at[i]) == keys[k][i]) i++;
      if (i != keyLength) continue;
      for (char *value = at + keyLength;
           *value && !strchr(" &\",)\r\n", *value); value++) {
        *value = '*';
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Files: YYYYMMDD-bNNNNN-pPPP.log, date 00000000 while the clock is unset
// ---------------------------------------------------------------------------

struct LogName {
  uint32_t date;   // YYYYMMDD, 0 = unknown
  uint32_t boot;
  uint32_t part;
};

inline void formatLogName(const LogName &name, char *out, size_t size) {
  snprintf(out, size, "%08lu-b%05lu-p%03lu.log", (unsigned long)name.date,
           (unsigned long)name.boot, (unsigned long)name.part);
}

inline bool parseLogName(const char *text, LogName &name) {
  unsigned long date = 0, boot = 0, part = 0;
  int end = 0;
  if (sscanf(text, "%8lu-b%lu-p%lu.log%n", &date, &boot, &part, &end) != 3) return false;
  if (!end || text[end] != '\0') return false;
  name.date = (uint32_t)date;
  name.boot = (uint32_t)boot;
  name.part = (uint32_t)part;
  return true;
}

// Days since 1970-01-01 for a YYYYMMDD number.
inline int32_t dayNumber(uint32_t date) {
  int32_t y = (int32_t)(date / 10000);
  uint32_t m = (date / 100) % 100;
  uint32_t d = date % 100;
  y -= m <= 2;
  int32_t era = (y >= 0 ? y : y - 399) / 400;
  uint32_t yoe = (uint32_t)(y - era * 400);
  uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 719468;
}

struct LogFile {
  LogName name;
  uint32_t size;
  bool current;    // being written; never deleted
  bool remove;     // set by planRetention()
};

/*
  Marks what to delete. First anything dated more than maxAgeDays before today
  (skipped while the date is unknown), then the oldest files, in the order they
  were created, until both the byte and the file-count limits are met.
*/
inline void planRetention(LogFile *files, size_t count, uint32_t today,
                          int32_t maxAgeDays, uint64_t maxBytes, size_t maxFiles) {
  uint64_t total = 0;
  size_t kept = 0;
  for (size_t i = 0; i < count; i++) {
    files[i].remove = !files[i].current && today && files[i].name.date &&
                      dayNumber(today) - dayNumber(files[i].name.date) > maxAgeDays;
    if (!files[i].remove) {
      total += files[i].size;
      kept++;
    }
  }
  while (total > maxBytes || kept > maxFiles) {
    size_t oldest = count;
    for (size_t i = 0; i < count; i++) {
      if (files[i].remove || files[i].current) continue;
      if (oldest == count || files[i].name.boot < files[oldest].name.boot ||
          (files[i].name.boot == files[oldest].name.boot &&
           files[i].name.part < files[oldest].name.part)) {
        oldest = i;
      }
    }
    if (oldest == count) break;
    files[oldest].remove = true;
    total -= files[oldest].size;
    kept--;
  }
}

}  // namespace dlog
