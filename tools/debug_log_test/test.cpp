// Host tests for remote/debug_log_core.h.
//   g++ -std=c++17 -fsanitize=address,undefined -I../../remote test.cpp -o /tmp/dlt && /tmp/dlt
#include "debug_log_core.h"
#include <assert.h>
#include <string>
#include <vector>
#include <algorithm>

using namespace dlog;

static const size_t SIZE = 4096;

static std::string recover(const uint8_t *ring, uint32_t after, uint32_t &newest, uint32_t &next) {
  static RingRecord scratch[SIZE / (RECORD_HEADER + 1)];
  static char out[SIZE];
  size_t n = ringRecover(ring, SIZE, after, scratch, SIZE / (RECORD_HEADER + 1), out, SIZE,
                         newest, next);
  return std::string(out, n);
}

static std::string line(uint32_t i) {
  return "line " + std::to_string(i) + " " + std::string(i * 7 % 150, 'x') + "\n";
}

int main() {
  uint8_t ring[SIZE];
  uint32_t newest, next;

  // Random power-on memory yields nothing.
  srand(1);
  for (int round = 0; round < 200; round++) {
    for (auto &b : ring) b = (uint8_t)rand();
    assert(recover(ring, 0, newest, next).empty());
  }

  // Many wraps: what comes back is an unbroken run of the newest lines.
  memset(ring, 0, SIZE);
  uint32_t pos = 0;
  for (uint32_t i = 1; i <= 5000; i++) {
    std::string s = line(i);
    ringPut(ring, SIZE, pos, i, s.data(), s.size());
    if (i % 97) continue;
    std::string got = recover(ring, 0, newest, next);
    assert(newest == i && next == pos);
    std::string want;
    uint32_t j = i;
    while (j > 0 && want.size() + line(j).size() <= got.size()) want = line(j--) + want;
    assert(want == got);
    assert(got.size() > 3000);
    // Only lines newer than what reached the card.
    assert(recover(ring, i - 3, newest, next) == line(i - 2) + line(i - 1) + line(i));
    assert(recover(ring, i, newest, next).empty());
  }

  // Carrying on after recovery, as a boot does.
  recover(ring, 0, newest, next);
  uint32_t resumed = next;
  std::string after = line(5001);
  ringPut(ring, SIZE, resumed, newest + 1, after.data(), after.size());
  assert(recover(ring, 5000, newest, next) == after);

  // A crash part way through a record loses only that record.
  uint32_t before = resumed;
  uint8_t torn[SIZE];
  memcpy(torn, ring, SIZE);
  for (size_t i = 0; i < 40; i++) torn[(before + RECORD_HEADER + i) % SIZE] = 'z';
  std::string got = recover(torn, 4999, newest, next);
  assert(got.find(after) != std::string::npos && newest == 5001);

  // A flipped bit drops that record.
  memcpy(torn, ring, SIZE);
  torn[(before + SIZE - 3) % SIZE] ^= 1;
  assert(recover(torn, 5000, newest, next).empty());

  // Over-long text is cut to the record limit.
  std::string huge(1000, 'q');
  ringPut(ring, SIZE, resumed, 9000, huge.data(), huge.size());
  assert(recover(ring, 8999, newest, next).size() == RECORD_MAX_TEXT);

  // Secrets.
  auto masked = [](const char *in) {
    char buf[200];
    strcpy(buf, in);
    maskSecrets(buf);
    return std::string(buf);
  };
  assert(masked("GET /x?token=abc123&a=1") == "GET /x?token=******&a=1");
  assert(masked("Authorization: Bearer eyJ.ab") == "Authorization: Bearer ******");
  assert(masked("{\"password\":\"hunter2\",\"u\":1}") == "{\"password\":\"*******\",\"u\":1}");
  assert(masked("Dock: pushed (2 token chunk(s))") == "Dock: pushed (2 token chunk(s))");
  assert(masked("PASSWORD=x token=") == "PASSWORD=* token=");

  // Names.
  char name[40];
  formatLogName({20261126, 12, 3}, name, sizeof(name));
  assert(std::string(name) == "20261126-b00012-p003.log");
  LogName parsed;
  assert(parseLogName(name, parsed) && parsed.date == 20261126 && parsed.boot == 12 &&
         parsed.part == 3);
  assert(!parseLogName("battery.csv", parsed));
  assert(!parseLogName("20261126-b00012-p003.log.bak", parsed));

  // Dates.
  assert(dayNumber(19700101) == 0);
  assert(dayNumber(20260301) - dayNumber(20260228) == 1);
  assert(dayNumber(20280301) - dayNumber(20280228) == 2);
  assert(dayNumber(20270101) - dayNumber(20261231) == 1);

  // Age: seven days old stays, eight goes; undated and current stay.
  std::vector<LogFile> files = {
    {{20261118, 1, 1}, 10, false, false}, {{20261119, 2, 1}, 10, false, false},
    {{0, 3, 1}, 10, false, false},        {{20261101, 4, 1}, 10, true, false}};
  planRetention(files.data(), files.size(), 20261126, 7, 1000, 100);
  assert(files[0].remove && !files[1].remove && !files[2].remove && !files[3].remove);
  planRetention(files.data(), files.size(), 0, 7, 1000, 100);
  assert(!files[0].remove);

  // Size cap deletes in creation order and never the current file.
  files = {{{20261126, 5, 2}, 40, false, false}, {{20261126, 4, 1}, 40, false, false},
           {{20261126, 5, 1}, 40, false, false}, {{20261126, 1, 1}, 40, true, false}};
  planRetention(files.data(), files.size(), 20261126, 7, 100, 100);
  assert(files[1].remove && files[2].remove && !files[0].remove && !files[3].remove);
  planRetention(files.data(), files.size(), 20261126, 7, 1, 100);
  assert(!files[3].remove);
  planRetention(files.data(), files.size(), 20261126, 7, 1000, 3);
  assert(files[1].remove && !files[2].remove);

  puts("debug_log_core: all tests passed");
  return 0;
}
