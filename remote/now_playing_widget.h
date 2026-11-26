/* ---------------------------------------------------------------------- *
   Now Playing widget

   One widget with two independent Home Assistant sources, either of which
   may be left empty:

     - "Now playing from": a media_player whose app, title, artwork and
       position are shown (an Apple TV, a Roku, a TV's own apps).
     - "Volume from": a media_player whose volume, mute, input and sound mode
       are shown and controlled (an AV receiver, a TV on ARC).

   Each half labels its own source, so it is always clear what a key press
   will control. Expanding the widget gives transport keys and volume keys,
   and a Receiver panel with the input and sound-mode pickers - each of which
   can be locked in WebConfig. A volume key pressed anywhere on the remote
   shows a short pop-up, for receivers that cannot draw their own on-screen
   display over a 4K picture.

   Configuration is global, like every library widget: there is one remote
   per room, so the players are pinned rather than following the page.

   State comes from Home Assistant's REST API, polled directly by the remote
   while the widget (or the pop-up) is on screen and the display is awake.
   The playback clock is advanced locally between polls. A paired dock does
   not carry this rich state yet, so this widget polls directly even when a
   dock is paired - see serviceHomeAssistantLive().

   Included from OpenRemote_1.0.ino after the widget helpers it uses.
 * ---------------------------------------------------------------------- */
#pragma once

static const uint8_t NP_MAX_LIST = 24;
static const uint8_t NP_LIST_TEXT = 32;
static const int16_t NP_ART_SIZE = 96;

struct NowPlayingSettings {
  char mediaEntity[64];
  char volumeEntity[64];
  bool allowInput;       // the input grid is shown and usable
  bool allowSoundMode;   // the sound-mode grid is shown and usable
  bool volumeDb;         // dB rather than percent
  float dbOffset;        // dB = volume_level x 100 + dbOffset (Denon: -80)
  bool volumePopup;      // pop-up on any page when a volume key is pressed
  bool artwork;          // download and show artwork
};

/* Defaults must match defaultWidgetSettings().nowPlaying in WebConfig. */
NowPlayingSettings npSettings = { "", "", true, true, true, -80.0f, true, true };

struct NpEntityState {
  bool valid;            // the last read succeeded
  char entity[64];       // which entity this was read from
  char state[16];        // playing, paused, idle, standby, on, off, unavailable
  char friendly[40];
  char app[32];
  char title[96];
  char subtitle[72];
  char source[40];
  char soundMode[40];    // as shown: sound_mode_raw when offered
  char soundModeSelected[40];   // sound_mode, for highlighting the picker
  char picture[200];     // entity_picture, as Home Assistant reports it
  uint32_t supported;    // supported_features
  float volume;
  bool hasVolume;
  bool muted;
  bool hasPosition;
  uint32_t position;     // seconds, true at positionAtMs
  uint32_t duration;
  uint32_t positionAtMs;
  char sources[NP_MAX_LIST][NP_LIST_TEXT];
  uint8_t sourceCount;
  char soundModes[NP_MAX_LIST][NP_LIST_TEXT];
  uint8_t soundModeCount;
  uint32_t listHash;     // changes when either list changes
};

NpEntityState npMedia = {};
NpEntityState npVolume = {};

/* Home Assistant's media_player feature bits that matter here. */
static const uint32_t NP_FEATURE_SEEK = 2;
static const uint32_t NP_FEATURE_TURN_ON = 128;

/* Polling and refresh scheduling. */
uint32_t npPollNextMs = 0;
uint8_t npPollTurn = 0;
uint32_t npRefreshMediaAtMs = 0;
uint32_t npRefreshVolumeAtMs = 0;
uint32_t npRefreshVolumeLateMs = 0;
bool npDirty = false;

/* The volume pop-up. */
lv_obj_t *npPopup = nullptr;
lv_obj_t *npPopupName = nullptr;
lv_obj_t *npPopupValue = nullptr;
lv_obj_t *npPopupFill = nullptr;
uint32_t npPopupUntilMs = 0;
static const int NP_POPUP_TRACK = 176;

/* Artwork, kept apart from the Chromecast Media widget's poster. */
uint8_t *npArtPixels = nullptr;
lv_img_dsc_t npArtDescriptor = {};
char npArtKey[200] = "";          // what npArtPixels shows
char npArtFailedKey[200] = "";    // what last failed to download
uint8_t npArtFailures = 0;
uint32_t npArtRetryAtMs = 0;

/* A button press, run from the loop rather than inside the LVGL callback. */
enum NpAction : uint8_t {
  NP_ACT_NONE = 0,
  NP_ACT_PLAY_PAUSE,
  NP_ACT_BACK,
  NP_ACT_FORWARD,
  NP_ACT_VOLUME_DOWN,
  NP_ACT_VOLUME_UP,
  NP_ACT_MUTE,
  NP_ACT_VOLUME_POWER,
  NP_ACT_MEDIA_POWER,
  NP_ACT_SOURCE,
  NP_ACT_SOUND_MODE,
  NP_ACT_SHOW_RECEIVER,
  NP_ACT_SHOW_MEDIA
};
volatile uint8_t npPendingAction = NP_ACT_NONE;
volatile uint8_t npPendingIndex = 0;

struct NowPlayingUi {
  bool expanded;
  bool slim;
  bool hasMedia;
  bool hasVolume;
  int16_t width;
  int16_t height;
  // Now playing
  lv_obj_t *mediaPanel;
  lv_obj_t *artBox;
  lv_obj_t *art;
  lv_obj_t *artGlyph;
  lv_obj_t *tag;
  lv_obj_t *title;
  lv_obj_t *subtitle;
  lv_obj_t *track;
  lv_obj_t *fill;
  lv_obj_t *left;
  lv_obj_t *right;
  lv_obj_t *stateGlyph;
  int16_t trackWidth;
  int16_t artSize;
  const void *artShown;
  // Transport (expanded)
  lv_obj_t *transport;
  lv_obj_t *playLabel;
  lv_obj_t *backLabel;
  lv_obj_t *forwardLabel;
  lv_obj_t *mediaPowerBtn;
  // Volume strip / row
  lv_obj_t *volIcon;
  lv_obj_t *volLabel;
  lv_obj_t *volValue;
  lv_obj_t *volKeys;      // - mute + Receiver (expanded)
  lv_obj_t *muteLabel;
  lv_obj_t *volPowerBtn;
  lv_obj_t *receiverBtn;
  // Receiver panel (expanded)
  lv_obj_t *rxPanel;
  lv_obj_t *rxName;
  lv_obj_t *rxValue;
  lv_obj_t *rxFill;
  lv_obj_t *rxMuteLabel;
  lv_obj_t *rxKeys;
  lv_obj_t *rxPowerBtn;
  lv_obj_t *rxList;
  lv_obj_t *rxSourceBtns[NP_MAX_LIST];
  lv_obj_t *rxSoundBtns[NP_MAX_LIST];
  lv_obj_t *rxSourceText;   // read-only input line when inputs are locked
  lv_obj_t *rxSoundText;
  uint8_t rxSourceCount;
  uint8_t rxSoundCount;
  uint32_t rxListHash;
  int16_t rxTrackWidth;
  bool rxShown;
};

/* ------------------------------------------------------------ helpers */

static lv_color_t npAccent() { return lvRgb(90, 169, 255); }
static lv_color_t npVolumeColour() { return lvRgb(127, 214, 168); }
static lv_color_t npMutedRed() { return lvRgb(255, 138, 122); }
static lv_color_t npKeyColour() { return lvRgb(36, 48, 63); }

bool nowPlayingConfigured() {
  return npSettings.mediaEntity[0] || npSettings.volumeEntity[0];
}

static bool npStateIs(const NpEntityState &s, const char *value) {
  return s.valid && strcmp(s.state, value) == 0;
}

static bool npIsOff(const NpEntityState &s) {
  return !s.valid || npStateIs(s, "off") || npStateIs(s, "standby") ||
         npStateIs(s, "unavailable") || npStateIs(s, "unknown");
}

static bool npIsPlaying(const NpEntityState &s) {
  return npStateIs(s, "playing") || npStateIs(s, "buffering");
}

static uint32_t npFnv(uint32_t hash, const char *text) {
  for (const char *c = text; c && *c; c++) {
    hash ^= (uint8_t)*c;
    hash *= 16777619UL;
  }
  return hash;
}

/* Seconds since 1970 for a Home Assistant timestamp, which is always UTC:
   "2026-11-26T14:00:00.123456+00:00". 0 when it does not parse. */
static time_t npParseUtc(const char *text) {
  if (!text || strlen(text) < 19) return 0;
  int year, month, day, hour, minute, second;
  if (sscanf(text, "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour, &minute,
             &second) != 6) return 0;
  // Days from the civil date (Howard Hinnant's algorithm).
  year -= month <= 2;
  int era = (year >= 0 ? year : year - 399) / 400;
  unsigned yoe = (unsigned)(year - era * 400);
  unsigned doy = (153U * (unsigned)(month + (month > 2 ? -3 : 9)) + 2U) / 5U + (unsigned)day - 1U;
  unsigned doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;
  long days = (long)era * 146097L + (long)doe - 719468L;
  time_t result = (time_t)days * 86400 + hour * 3600 + minute * 60 + second;
  // A +hh:mm / -hh:mm suffix, should Home Assistant ever send one.
  const char *zone = text + 19;
  while (*zone && *zone != '+' && *zone != '-' && *zone != 'Z') zone++;
  if (*zone == '+' || *zone == '-') {
    int zh = 0, zm = 0;
    if (sscanf(zone + 1, "%2d:%2d", &zh, &zm) >= 1) {
      long offset = zh * 3600L + zm * 60L;
      result += *zone == '+' ? -offset : offset;
    }
  }
  return result;
}

uint32_t npMediaPosition() {
  const NpEntityState &s = npMedia;
  if (!s.hasPosition) return 0;
  uint32_t position = s.position;
  if (npIsPlaying(s)) position += (millis() - s.positionAtMs) / 1000U;
  if (s.duration && position > s.duration) position = s.duration;
  return position;
}

/* "-32 dB", "-32.5 dB", "45%", or "--". */
void npFormatVolume(char *out, size_t size, const NpEntityState &s) {
  if (!s.hasVolume) { snprintf(out, size, "--"); return; }
  if (npSettings.volumeDb) {
    float db = s.volume * 100.0f + npSettings.dbOffset;
    float rounded = roundf(db * 2.0f) / 2.0f;   // receivers step in 0.5 dB
    if (fabsf(rounded - roundf(rounded)) < 0.01f) snprintf(out, size, "%d dB", (int)roundf(rounded));
    else snprintf(out, size, "%.1f dB", rounded);
  } else {
    snprintf(out, size, "%d%%", (int)roundf(s.volume * 100.0f));
  }
}

static void npCopyText(const char *source, char *destination, size_t size) {
  copyDisplayTextAscii(source ? source : "", destination, size);
}

/* --------------------------------------------------------- state reads */

/*
  Reads one media_player over REST, keeping only the attributes this widget
  draws. A receiver's attribute block is small; an Apple TV's is not much
  bigger, and the filter keeps anything else out of memory.
*/
bool npReadEntity(NpEntityState &s, const char *entityId) {
  if (!entityId || !entityId[0]) return false;
  String response, error;
  int status = homeAssistantRequest("GET", String("/api/states/") + entityId, "",
                                    response, error);
  if (status < 200 || status >= 300) {
    Serial.printf("Now Playing: could not read %s - %s\n", entityId, error.c_str());
    if (status == 404) {
      s.valid = false;
      strlcpy(s.entity, entityId, sizeof(s.entity));
      strlcpy(s.state, "unavailable", sizeof(s.state));
      npDirty = true;
    }
    return false;
  }

  JsonDocument filter;
  filter["state"] = true;
  JsonObject a = filter["attributes"].to<JsonObject>();
  const char *keys[] = {
    "friendly_name", "app_name", "source", "source_list", "sound_mode",
    "sound_mode_list", "sound_mode_raw", "media_title", "media_artist",
    "media_album_name", "media_series_title", "media_season", "media_episode",
    "media_channel", "media_duration", "media_position",
    "media_position_updated_at", "entity_picture", "volume_level",
    "is_volume_muted", "supported_features"
  };
  for (const char *key : keys) a[key] = true;
  JsonDocument doc(&psramJsonAllocator);
  if (deserializeJson(doc, response, DeserializationOption::Filter(filter))) return false;

  JsonObjectConst attrs = doc["attributes"].as<JsonObjectConst>();
  // Static: two lists of names make this too big for the loop task's stack.
  static NpEntityState next;
  memset(&next, 0, sizeof(next));
  next.valid = true;
  strlcpy(next.entity, entityId, sizeof(next.entity));
  strlcpy(next.state, doc["state"] | "unknown", sizeof(next.state));
  npCopyText(attrs["friendly_name"] | entityId, next.friendly, sizeof(next.friendly));
  npCopyText(attrs["app_name"] | (attrs["source"] | ""), next.app, sizeof(next.app));
  npCopyText(attrs["source"] | "", next.source, sizeof(next.source));
  // The processing mode as the receiver names it ("DOLBY ATMOS") when the
  // integration offers it, otherwise the selectable mode.
  const char *rawMode = attrs["sound_mode_raw"] | "";
  npCopyText(rawMode[0] ? rawMode : (attrs["sound_mode"] | ""), next.soundMode,
             sizeof(next.soundMode));
  strlcpy(next.picture, attrs["entity_picture"] | "", sizeof(next.picture));
  next.supported = attrs["supported_features"] | 0;
  if (!attrs["volume_level"].isNull()) {
    next.volume = attrs["volume_level"] | 0.0f;
    next.hasVolume = true;
  }
  next.muted = attrs["is_volume_muted"] | false;

  // A series reads as the show with "S2 · E4 · Episode" under it; anything
  // else as its title with the artist, album or channel under it.
  const char *title = attrs["media_title"] | "";
  const char *series = attrs["media_series_title"] | "";
  const char *channel = attrs["media_channel"] | "";
  const char *artist = attrs["media_artist"] | "";
  const char *album = attrs["media_album_name"] | "";
  char subtitle[160] = "";
  if (series[0]) {
    npCopyText(series, next.title, sizeof(next.title));
    String season = attrs["media_season"].is<const char *>()
      ? String(attrs["media_season"].as<const char *>())
      : (attrs["media_season"].isNull() ? String() : String(attrs["media_season"].as<long>()));
    String episode = attrs["media_episode"].is<const char *>()
      ? String(attrs["media_episode"].as<const char *>())
      : (attrs["media_episode"].isNull() ? String() : String(attrs["media_episode"].as<long>()));
    if (season.length() && episode.length()) {
      snprintf(subtitle, sizeof(subtitle), "S%s E%s%s%s", season.c_str(), episode.c_str(),
               title[0] ? " - " : "", title);
    } else {
      strlcpy(subtitle, title, sizeof(subtitle));
    }
  } else if (title[0]) {
    npCopyText(title, next.title, sizeof(next.title));
    if (artist[0] && album[0]) snprintf(subtitle, sizeof(subtitle), "%s - %s", artist, album);
    else if (artist[0]) strlcpy(subtitle, artist, sizeof(subtitle));
    else if (channel[0]) strlcpy(subtitle, channel, sizeof(subtitle));
  } else if (channel[0]) {
    npCopyText(channel, next.title, sizeof(next.title));
  }
  npCopyText(subtitle, next.subtitle, sizeof(next.subtitle));

  if (!attrs["media_position"].isNull()) {
    float position = attrs["media_position"] | 0.0f;
    float duration = attrs["media_duration"] | 0.0f;
    next.hasPosition = true;
    next.duration = duration > 0 ? (uint32_t)duration : 0;
    // Home Assistant reports where playback was when it last looked; carry
    // that forward to now if the clock allows, then count locally.
    time_t now = time(nullptr);
    time_t updated = npParseUtc(attrs["media_position_updated_at"] | "");
    if (strcmp(next.state, "playing") == 0 && now > 1700000000 && updated > 0 &&
        now >= updated && now - updated < 6 * 3600) {
      position += (float)(now - updated);
    }
    next.position = position > 0 ? (uint32_t)position : 0;
    if (next.duration && next.position > next.duration) next.position = next.duration;
    next.positionAtMs = millis();
  } else if (!attrs["media_duration"].isNull()) {
    next.duration = (uint32_t)(attrs["media_duration"] | 0.0f);
  }

  uint32_t hash = 2166136261UL;
  for (JsonVariantConst item : attrs["source_list"].as<JsonArrayConst>()) {
    if (next.sourceCount >= NP_MAX_LIST) break;
    npCopyText(item | "", next.sources[next.sourceCount], NP_LIST_TEXT);
    hash = npFnv(hash, next.sources[next.sourceCount]);
    next.sourceCount++;
  }
  hash = npFnv(hash, "|");
  for (JsonVariantConst item : attrs["sound_mode_list"].as<JsonArrayConst>()) {
    if (next.soundModeCount >= NP_MAX_LIST) break;
    npCopyText(item | "", next.soundModes[next.soundModeCount], NP_LIST_TEXT);
    hash = npFnv(hash, next.soundModes[next.soundModeCount]);
    next.soundModeCount++;
  }
  next.listHash = hash;

  // The current sound mode, for highlighting, is the selectable one.
  char selectableMode[40];
  npCopyText(attrs["sound_mode"] | "", selectableMode, sizeof(selectableMode));
  if (!rawMode[0]) strlcpy(next.soundMode, selectableMode, sizeof(next.soundMode));

  // The grid highlights the selectable name; the label shows the receiver's
  // own wording when there is one.
  strlcpy(next.soundModeSelected, selectableMode, sizeof(next.soundModeSelected));

  bool changed = memcmp(&s, &next, sizeof(next)) != 0;
  s = next;
  if (changed) npDirty = true;
  return true;
}

/* ------------------------------------------------------------- artwork *
   The player's entity_picture - a poster on an Apple TV, the app's icon on
   a Roku - fetched through Home Assistant, fitted into 96x96 and cached on
   the SD card in the same raw RGB565 form as the Media widget's posters.
   The cache key leaves out Home Assistant's rotating access token, so the
   same picture is found again tomorrow.
 * ---------------------------------------------------------------------- */
static void npArtKeyFor(const char *picture, char *out, size_t size) {
  out[0] = '\0';
  if (!picture || !picture[0]) return;
  String key(picture);
  int at = key.indexOf("token=");
  if (at >= 0) {
    int end = key.indexOf('&', at);
    key.remove(at, end < 0 ? key.length() - at : end - at + 1);
  }
  strlcpy(out, key.c_str(), size);
}

void npArtRelease() {
  if (npArtPixels) { free(npArtPixels); npArtPixels = nullptr; }
  npArtDescriptor = lv_img_dsc_t{};
  npArtKey[0] = '\0';
}

const void *npArtSource() {
  return npArtPixels ? (const void *)&npArtDescriptor : nullptr;
}

static bool npArtAdopt(uint8_t *pixels, uint16_t width, uint16_t height, const char *key) {
  npArtRelease();
  npArtPixels = pixels;
  npArtDescriptor.header.always_zero = 0;
  npArtDescriptor.header.w = width;
  npArtDescriptor.header.h = height;
  npArtDescriptor.header.cf = LV_IMG_CF_TRUE_COLOR;
  npArtDescriptor.data_size = (uint32_t)width * height * 2;
  npArtDescriptor.data = npArtPixels;
  strlcpy(npArtKey, key, sizeof(npArtKey));
  npDirty = true;
  return true;
}

static bool npArtLoadFromCard(const char *path, const char *key) {
  if (!sdReady || !SD.exists(path)) return false;
  File file = SD.open(path, FILE_READ);
  if (!file) return false;
  uint16_t width = 0, height = 0;
  if (file.read((uint8_t *)&width, 2) != 2 || file.read((uint8_t *)&height, 2) != 2 ||
      !width || !height || width > 256 || height > 256) {
    file.close();
    return false;
  }
  size_t bytes = (size_t)width * height * 2;
  uint8_t *pixels = (uint8_t *)(psramFound() ? ps_malloc(bytes) : malloc(bytes));
  if (!pixels) { file.close(); return false; }
  size_t got = file.read(pixels, bytes);
  file.close();
  if (got != bytes) { free(pixels); return false; }
  return npArtAdopt(pixels, width, height, key);
}

/* Downloads, decodes and caches one picture. Blocking, like every other
   network read here, and only ever run with the widget on screen. */
static bool npArtDownload(const char *picture, const char *key, const char *path) {
  if (!ensureStationConnected(8000UL)) return false;
  String url = picture;
  String base = normaliseHomeAssistantUrl(haBaseUrl);
  if (!url.startsWith("http://") && !url.startsWith("https://")) url = base + url;
  HTTPClient http;
  http.setConnectTimeout(4000);
  http.setTimeout(8000);
  if (!http.begin(url)) return false;
  // The token goes to Home Assistant only, never to an outside image host.
  if (url.startsWith(base + "/")) http.addHeader("Authorization", "Bearer " + haToken);
  int status = http.GET();
  if (status != 200) {
    Serial.printf("Now Playing: artwork HTTP %d\n", status);
    http.end();
    return false;
  }
  const size_t limit = 400UL * 1024UL;
  int declared = http.getSize();
  if (declared > (int)limit) {
    Serial.printf("Now Playing: artwork too large (%d bytes)\n", declared);
    http.end();
    return false;
  }
  size_t capacity = declared > 0 ? (size_t)declared : limit;
  uint8_t *data = (uint8_t *)(psramFound() ? ps_malloc(capacity) : malloc(capacity));
  if (!data) { http.end(); return false; }
  auto *stream = http.getStreamPtr();
  size_t length = 0;
  uint32_t started = millis();
  while (http.connected() && length < capacity && millis() - started < 8000UL) {
    size_t available = stream->available();
    if (!available) {
      if (declared > 0 && length >= (size_t)declared) break;
      delay(2);
      continue;
    }
    int got = stream->readBytes(data + length, min(available, capacity - length));
    if (got <= 0) break;
    length += (size_t)got;
    if (declared > 0 && length >= (size_t)declared) break;
  }
  http.end();
  if (length < 16) { free(data); return false; }

  lgfx::LGFX_Sprite sprite;
  sprite.setColorDepth(16);
  sprite.setPsram(true);
  if (!sprite.createSprite(NP_ART_SIZE, NP_ART_SIZE)) { free(data); return false; }
  sprite.fillScreen(sprite.color565(28, 38, 51));
  bool png = data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G';
  bool jpeg = data[0] == 0xFF && data[1] == 0xD8;
  // A scale of zero fits the picture inside the box, centred.
  bool drawn = png
    ? sprite.drawPng(data, length, 0, 0, NP_ART_SIZE, NP_ART_SIZE, 0, 0, 0.0f, 0.0f,
                     lgfx::datum_t::middle_center)
    : jpeg
    ? sprite.drawJpg(data, length, 0, 0, NP_ART_SIZE, NP_ART_SIZE, 0, 0, 0.0f, 0.0f,
                     lgfx::datum_t::middle_center)
    : false;
  free(data);
  if (!drawn) {
    Serial.println("Now Playing: artwork is not a JPEG or PNG the remote can decode");
    sprite.deleteSprite();
    return false;
  }

  const size_t bytes = (size_t)NP_ART_SIZE * NP_ART_SIZE * 2;
  uint8_t *pixels = (uint8_t *)(psramFound() ? ps_malloc(bytes) : malloc(bytes));
  if (!pixels) { sprite.deleteSprite(); return false; }
  // The sprite holds byte-swapped RGB565; LVGL here wants it native.
  const uint16_t *source = (const uint16_t *)sprite.getBuffer();
  uint16_t *target = (uint16_t *)pixels;
  for (size_t i = 0; i < (size_t)NP_ART_SIZE * NP_ART_SIZE; i++) {
    uint16_t v = source[i];
    target[i] = (uint16_t)((v >> 8) | (v << 8));
  }
  sprite.deleteSprite();

  if (sdReady) {
    if (!SD.exists(MEDIA_ART_DIR)) SD.mkdir(MEDIA_ART_DIR);
    String temporary = String(path) + ".tmp";
    File file = SD.open(temporary.c_str(), FILE_WRITE);
    if (file) {
      uint16_t size = NP_ART_SIZE;
      bool ok = file.write((const uint8_t *)&size, 2) == 2 &&
                file.write((const uint8_t *)&size, 2) == 2 &&
                file.write(pixels, bytes) == bytes;
      file.close();
      if (ok) {
        if (SD.exists(path)) SD.remove(path);
        SD.rename(temporary.c_str(), path);
      } else {
        SD.remove(temporary.c_str());
      }
    }
  }
  Serial.printf("Now Playing: artwork cached (%u bytes source)\n", (unsigned)length);
  return npArtAdopt(pixels, NP_ART_SIZE, NP_ART_SIZE, key);
}

bool nowPlayingOnScreen();

void serviceNowPlayingArtwork(uint32_t now) {
  if (!npSettings.artwork || !npSettings.mediaEntity[0]) {
    if (npArtPixels) npArtRelease();
    return;
  }
  char key[200];
  npArtKeyFor(npMedia.valid && !npIsOff(npMedia) ? npMedia.picture : "", key, sizeof(key));
  if (!key[0]) {
    if (npArtPixels) npArtRelease();
    return;
  }
  if (strcmp(key, npArtKey) == 0) return;
  if (displaySleeping || !nowPlayingOnScreen()) return;

  char path[64];
  mediaArtFileForUrl(key, path, sizeof(path));
  if (npArtLoadFromCard(path, key)) return;

  if (strcmp(key, npArtFailedKey) == 0) {
    if (npArtFailures >= 3) return;
    if (npArtRetryAtMs && (int32_t)(now - npArtRetryAtMs) < 0) return;
  } else {
    strlcpy(npArtFailedKey, key, sizeof(npArtFailedKey));
    npArtFailures = 0;
  }
  if (WiFi.status() != WL_CONNECTED) return;   // the poll brings the radio up
  // Shown without artwork meanwhile, rather than with the previous title's.
  if (npArtPixels) npArtRelease();
  if (!npArtDownload(npMedia.picture, key, path)) {
    npArtFailures++;
    npArtRetryAtMs = now + 15000UL;
  }
}

/* ------------------------------------------------------------- polling */

bool nowPlayingPopupActive() {
  return npPopup != nullptr;
}

/* Whether anything on screen needs fresh state. */
bool nowPlayingPollWanted() {
  if (!homeAssistantConfigured() || !nowPlayingConfigured() || displaySleeping) return false;
  return nowPlayingOnScreen() || nowPlayingPopupActive() ||
         npRefreshMediaAtMs || npRefreshVolumeAtMs || npRefreshVolumeLateMs;
}

static bool npDue(uint32_t &at, uint32_t now) {
  if (!at || (int32_t)(now - at) < 0) return false;
  at = 0;
  return true;
}

/* One read, if one is due. The caller has the station connected. */
void pollNowPlaying(uint32_t now) {
  bool same = npSettings.mediaEntity[0] && strcmp(npSettings.mediaEntity, npSettings.volumeEntity) == 0;
  bool readMedia = false, readVolume = false;
  if (npSettings.volumeEntity[0] &&
      (npDue(npRefreshVolumeAtMs, now) || npDue(npRefreshVolumeLateMs, now))) {
    readVolume = true;
  } else if (npSettings.mediaEntity[0] && npDue(npRefreshMediaAtMs, now)) {
    readMedia = true;
  } else if (!npPollNextMs || (int32_t)(now - npPollNextMs) >= 0) {
    // Turn about: the media player, then the receiver.
    npPollTurn ^= 1;
    if (npPollTurn && npSettings.mediaEntity[0]) readMedia = true;
    else if (npSettings.volumeEntity[0]) readVolume = true;
    else readMedia = npSettings.mediaEntity[0] != '\0';
    // Both every ~4 s; a single player every ~2.5 s.
    npPollNextMs = now + ((npSettings.mediaEntity[0] && npSettings.volumeEntity[0] && !same)
                          ? 2000UL : 2500UL);
  }
  if (same && (readMedia || readVolume)) {
    if (npReadEntity(npMedia, npSettings.mediaEntity)) npVolume = npMedia;
    return;
  }
  if (readMedia) npReadEntity(npMedia, npSettings.mediaEntity);
  if (readVolume) npReadEntity(npVolume, npSettings.volumeEntity);
}

/* After a command, read back soon - and again a moment later, because a
   receiver often reports the new level only after it has finished ramping. */
void npRequestRefresh(bool media, bool volume) {
  uint32_t now = millis();
  if (media) npRefreshMediaAtMs = now + 350UL;
  if (volume) {
    npRefreshVolumeAtMs = now + 300UL;
    npRefreshVolumeLateMs = now + 1400UL;
  }
}

/* ------------------------------------------------------------ commands */

static bool npCall(const char *entity, const char *service, const char *key = nullptr,
                   uint8_t kind = HA_DATA_NONE, const char *text = nullptr,
                   float value = 0.0f) {
  if (!entity || !entity[0]) return false;
  DeviceCommand command;
  memset(&command, 0, sizeof(command));
  command.kind = DeviceCommand::HOMEASSISTANT;
  command.haEntityId = const_cast<char *>(entity);
  command.haService = const_cast<char *>(service);
  command.haDataKey = const_cast<char *>(key ? key : "");
  command.haDataKind = kind;
  command.haDataText = const_cast<char *>(text ? text : "");
  command.haDataValue = value;
  command.haHasData = kind == HA_DATA_NUMBER;
  command.haShowState = false;
  return transmitHomeAssistantCommand(command);
}

void npShowVolumePopup();
void npShowReceiverPanel(bool show);

void runNowPlayingAction(uint8_t action, uint8_t index) {
  const char *media = npSettings.mediaEntity;
  const char *volume = npSettings.volumeEntity;
  switch (action) {
    case NP_ACT_PLAY_PAUSE:
      if (npCall(media, "media_player.media_play_pause")) {
        // Shown at once; the read-back confirms it.
        if (npIsPlaying(npMedia)) {
          npMedia.position = npMediaPosition();
          npMedia.positionAtMs = millis();
          strlcpy(npMedia.state, "paused", sizeof(npMedia.state));
        } else if (npStateIs(npMedia, "paused")) {
          npMedia.positionAtMs = millis();
          strlcpy(npMedia.state, "playing", sizeof(npMedia.state));
        }
        npDirty = true;
        npRequestRefresh(true, false);
      }
      break;
    case NP_ACT_BACK:
    case NP_ACT_FORWARD: {
      bool forward = action == NP_ACT_FORWARD;
      bool ok;
      if ((npMedia.supported & NP_FEATURE_SEEK) && npMedia.hasPosition) {
        int32_t target = (int32_t)npMediaPosition() + (forward ? 10 : -10);
        if (target < 0) target = 0;
        if (npMedia.duration && target > (int32_t)npMedia.duration) target = npMedia.duration;
        ok = npCall(media, "media_player.media_seek", "seek_position", HA_DATA_NUMBER,
                    nullptr, (float)target);
        if (ok) {
          npMedia.position = (uint32_t)target;
          npMedia.positionAtMs = millis();
          npDirty = true;
        }
      } else {
        ok = npCall(media, forward ? "media_player.media_next_track"
                                   : "media_player.media_previous_track");
      }
      if (ok) npRequestRefresh(true, false);
      break;
    }
    case NP_ACT_VOLUME_DOWN:
      npCall(volume, "media_player.volume_down");
      break;
    case NP_ACT_VOLUME_UP:
      npCall(volume, "media_player.volume_up");
      break;
    case NP_ACT_MUTE: {
      bool mute = !npVolume.muted;
      if (npCall(volume, "media_player.volume_mute", "is_volume_muted",
                 mute ? HA_DATA_TRUE : HA_DATA_FALSE)) {
        npVolume.muted = mute;
        npDirty = true;
      }
      break;
    }
    case NP_ACT_VOLUME_POWER:
      if (npCall(volume, "media_player.turn_on")) npRequestRefresh(false, true);
      break;
    case NP_ACT_MEDIA_POWER:
      if (npCall(media, "media_player.turn_on")) npRequestRefresh(true, false);
      break;
    case NP_ACT_SOURCE:
      if (!npSettings.allowInput || index >= npVolume.sourceCount) break;
      if (npCall(volume, "media_player.select_source", "source", HA_DATA_TEXT,
                 npVolume.sources[index])) {
        strlcpy(npVolume.source, npVolume.sources[index], sizeof(npVolume.source));
        npDirty = true;
        npRequestRefresh(false, true);
      }
      break;
    case NP_ACT_SOUND_MODE:
      if (!npSettings.allowSoundMode || index >= npVolume.soundModeCount) break;
      if (npCall(volume, "media_player.select_sound_mode", "sound_mode", HA_DATA_TEXT,
                 npVolume.soundModes[index])) {
        strlcpy(npVolume.soundModeSelected, npVolume.soundModes[index],
                sizeof(npVolume.soundModeSelected));
        strlcpy(npVolume.soundMode, npVolume.soundModes[index], sizeof(npVolume.soundMode));
        npDirty = true;
        npRequestRefresh(false, true);
      }
      break;
    case NP_ACT_SHOW_RECEIVER:
      npShowReceiverPanel(true);
      break;
    case NP_ACT_SHOW_MEDIA:
      npShowReceiverPanel(false);
      break;
    default:
      break;
  }
}

/*
  Every Home Assistant command passes through here once sent, whichever
  page or key it came from - so a hardware volume key mapped to the
  receiver brings up the pop-up just as the widget's own keys do.
*/
void nowPlayingNoteCommand(const DeviceCommand &command, bool sent) {
  if (!sent || !command.haEntityId || !command.haService) return;
  bool isVolume = npSettings.volumeEntity[0] &&
                  strcmp(command.haEntityId, npSettings.volumeEntity) == 0;
  bool isMedia = npSettings.mediaEntity[0] &&
                 strcmp(command.haEntityId, npSettings.mediaEntity) == 0;
  if (isMedia && strncmp(command.haService, "media_player.", 13) == 0) {
    npRequestRefresh(true, false);
  }
  if (!isVolume) return;
  const char *service = command.haService;
  bool volumeKey = strcmp(service, "media_player.volume_up") == 0 ||
                   strcmp(service, "media_player.volume_down") == 0 ||
                   strcmp(service, "media_player.volume_mute") == 0 ||
                   strcmp(service, "media_player.volume_set") == 0;
  if (strncmp(service, "media_player.", 13) == 0) npRequestRefresh(false, true);
  if (volumeKey) npShowVolumePopup();
}

/* -------------------------------------------------------------- faces */

static void npActionEvent(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  uintptr_t data = (uintptr_t)lv_event_get_user_data(event);
  npPendingIndex = (uint8_t)((data >> 8) & 0xFF);
  npPendingAction = (uint8_t)(data & 0xFF);
  lastWakeMs = millis();
}

static lv_obj_t *npBox(lv_obj_t *parent, int x, int y, int w, int h) {
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_pos(box, x, y);
  lv_obj_set_size(box, w, h);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  return box;
}

/* A key inside the expanded widget. Clickable, so a press on it never
   reaches the overlay underneath and closes the widget. */
static lv_obj_t *npKey(lv_obj_t *parent, int x, int y, int w, int h, const char *text,
                       const lv_font_t *font, uint8_t action, uint8_t index = 0,
                       lv_obj_t **labelOut = nullptr, bool round = false) {
  lv_obj_t *key = lv_obj_create(parent);
  lv_obj_remove_style_all(key);
  lv_obj_set_pos(key, x, y);
  lv_obj_set_size(key, w, h);
  lv_obj_set_style_radius(key, round ? LV_RADIUS_CIRCLE : 7, 0);
  lv_obj_set_style_bg_color(key, round ? npAccent() : npKeyColour(), 0);
  lv_obj_set_style_bg_opa(key, round ? LV_OPA_COVER : LV_OPA_80, 0);
  lv_obj_set_style_bg_color(key, lvRgb(70, 90, 115), LV_STATE_PRESSED);
  lv_obj_add_flag(key, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(key, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(key, npActionEvent, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)(((uintptr_t)index << 8) | action));
  lv_obj_t *label = lv_label_create(key);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, round ? lvRgb(16, 22, 31) : lvRgb(201, 212, 224), 0);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  lv_obj_set_style_max_width(label, w - 6, 0);
  lv_obj_center(label);
  lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
  if (labelOut) *labelOut = label;
  return key;
}

static void npSetKeySelected(lv_obj_t *key, bool selected) {
  if (!key || !lv_obj_is_valid(key)) return;
  lv_obj_set_style_bg_color(key, selected ? npAccent() : npKeyColour(), 0);
  lv_obj_set_style_bg_opa(key, selected ? LV_OPA_COVER : LV_OPA_80, 0);
  lv_obj_t *label = lv_obj_get_child(key, 0);
  if (label) lv_obj_set_style_text_color(label,
    selected ? lvRgb(16, 22, 31) : lvRgb(201, 212, 224), 0);
}

static lv_obj_t *npTrack(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t colour,
                         lv_obj_t **fillOut) {
  lv_obj_t *track = npBox(parent, x, y, w, h);
  lv_obj_set_style_radius(track, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(track, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(track, LV_OPA_20, 0);
  lv_obj_t *fill = npBox(track, 0, 0, 0, h);
  lv_obj_set_style_radius(fill, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(fill, colour, 0);
  lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
  *fillOut = fill;
  return track;
}

static lv_obj_t *npScrollingLabel(lv_obj_t *parent, int x, int y, int w, const lv_font_t *font,
                                  lv_color_t colour, lv_text_align_t align) {
  lv_obj_t *label = makeWidgetLabel(parent, "", x, y, font, colour, w, align);
  lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_anim_speed(label, 22, 0);
  return label;
}

static void npBuildArt(NowPlayingUi *ui, lv_obj_t *parent, int x, int y, int size) {
  ui->artSize = (int16_t)size;
  ui->artBox = npBox(parent, x, y, size, size);
  lv_obj_set_style_radius(ui->artBox, size >= 80 ? 10 : 6, 0);
  lv_obj_set_style_bg_color(ui->artBox, lvRgb(58, 47, 85), 0);
  lv_obj_set_style_bg_opa(ui->artBox, LV_OPA_COVER, 0);
  lv_obj_set_style_clip_corner(ui->artBox, true, 0);
  ui->artGlyph = makeLabel(ui->artBox, LV_SYMBOL_VIDEO, 0, 0,
                           size >= 80 ? &lv_font_montserrat_24 : &lv_font_montserrat_16,
                           lvRgb(185, 174, 240));
  lv_obj_center(ui->artGlyph);
  ui->art = lv_img_create(ui->artBox);
  lv_obj_clear_flag(ui->art, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui->art, LV_OBJ_FLAG_HIDDEN);
}

/* The compact volume line: icon, "Den Denon - TV Audio - Dolby Atmos", level. */
static void npBuildVolumeStrip(NowPlayingUi *ui, lv_obj_t *parent, int y, int width,
                               const lv_font_t *valueFont) {
  ui->volIcon = makeLabel(parent, LV_SYMBOL_VOLUME_MAX, 10, y + 4, &lv_font_montserrat_12,
                          npVolumeColour());
  ui->volLabel = makeWidgetLabel(parent, "", 28, y + 5, &lv_font_montserrat_10,
                                 widgetMutedColour(), width - 28 - 78);
  ui->volValue = makeWidgetLabel(parent, "--", width - 80, y + 1, valueFont,
                                 textPrimary(), 72, LV_TEXT_ALIGN_RIGHT);
}

static void npDeleted(lv_event_t *event) {
  NowPlayingUi *ui = (NowPlayingUi *)lv_event_get_user_data(event);
  if (ui) lv_mem_free(ui);
}

static void npBuildReceiverPanel(NowPlayingUi *ui, lv_obj_t *parent, int width, int height,
                                 bool withBack);

/*
  Builds the face into `parent`. Large tile 224x96, slim 224x44, expanded
  228x268. Labels are written by refreshNowPlayingUi().
*/
NowPlayingUi *buildNowPlayingFace(lv_obj_t *parent, int width, int height, bool expanded,
                                  bool slim) {
  NowPlayingUi *ui = (NowPlayingUi *)lv_mem_alloc(sizeof(NowPlayingUi));
  if (!ui) return nullptr;
  memset(ui, 0, sizeof(*ui));
  ui->expanded = expanded;
  ui->slim = slim && !expanded;
  ui->hasMedia = npSettings.mediaEntity[0] != '\0';
  ui->hasVolume = npSettings.volumeEntity[0] != '\0';
  ui->width = (int16_t)width;
  ui->height = (int16_t)height;
  ui->rxListHash = 0xFFFFFFFFUL;
  lv_obj_add_event_cb(parent, npDeleted, LV_EVENT_DELETE, ui);
  const lv_color_t muted = widgetMutedColour();

  if (!ui->hasMedia && !ui->hasVolume) {
    makeWidgetLabel(parent, "Now Playing", 0, slim ? 6 : 26, &lv_font_montserrat_14,
                    textPrimary(), width, LV_TEXT_ALIGN_CENTER);
    makeWidgetLabel(parent, "Choose the players in WebConfig", 0, slim ? 24 : 48,
                    &lv_font_montserrat_10, muted, width, LV_TEXT_ALIGN_CENTER);
    return ui;
  }

  if (ui->slim) {
    // One row: artwork, title and app, the level on the right.
    int textX = 10;
    if (ui->hasMedia) {
      npBuildArt(ui, parent, 6, 6, 32);
      textX = 46;
      int textW = width - textX - (ui->hasVolume ? 84 : 8);
      ui->title = npScrollingLabel(parent, textX, 6, textW, &lv_font_montserrat_12,
                                   textPrimary(), LV_TEXT_ALIGN_LEFT);
      ui->tag = makeWidgetLabel(parent, "", textX, 24, &lv_font_montserrat_10, muted, textW);
    } else {
      ui->volLabel = makeWidgetLabel(parent, "", 32, 15, &lv_font_montserrat_12, muted,
                                     width - 32 - 84);
      ui->volIcon = makeLabel(parent, LV_SYMBOL_VOLUME_MAX, 10, 14, &lv_font_montserrat_14,
                              npVolumeColour());
    }
    if (ui->hasVolume) {
      ui->volValue = makeWidgetLabel(parent, "--", width - 82, 13, &lv_font_montserrat_14,
                                     textPrimary(), 74, LV_TEXT_ALIGN_RIGHT);
    }
    return ui;
  }

  if (!expanded) {
    if (ui->hasMedia) {
      int mediaH = ui->hasVolume ? 68 : height;
      int art = ui->hasVolume ? 56 : 72;
      int artY = (mediaH - art) / 2;
      ui->mediaPanel = npBox(parent, 0, 0, width, mediaH);
      npBuildArt(ui, ui->mediaPanel, 8, artY, art);
      int textX = 8 + art + 9;
      int textW = width - textX - 8;
      int y = ui->hasVolume ? 5 : 12;
      ui->tag = makeWidgetLabel(ui->mediaPanel, "", textX, y, &lv_font_montserrat_10, muted, textW);
      ui->title = npScrollingLabel(ui->mediaPanel, textX, y + 13, textW, &lv_font_montserrat_14,
                                   textPrimary(), LV_TEXT_ALIGN_LEFT);
      ui->subtitle = makeWidgetLabel(ui->mediaPanel, "", textX, y + 31, &lv_font_montserrat_10,
                                     muted, textW);
      ui->trackWidth = (int16_t)textW;
      ui->track = npTrack(ui->mediaPanel, textX, y + 46, textW, 3, npAccent(), &ui->fill);
      ui->left = makeWidgetLabel(ui->mediaPanel, "", textX, y + 51, &lv_font_montserrat_10,
                                 muted, 60);
      ui->stateGlyph = makeWidgetLabel(ui->mediaPanel, "", textX + textW / 2 - 10, y + 51,
                                       &lv_font_montserrat_10, muted, 20, LV_TEXT_ALIGN_CENTER);
      ui->right = makeWidgetLabel(ui->mediaPanel, "", textX + textW - 60, y + 51,
                                  &lv_font_montserrat_10, muted, 60, LV_TEXT_ALIGN_RIGHT);
      if (ui->hasVolume) {
        lv_obj_t *line = glyphBar(parent, 8, 69, width - 16, 1, 0, lv_color_white());
        lv_obj_set_style_bg_opa(line, LV_OPA_20, 0);
        npBuildVolumeStrip(ui, parent, 71, width, &lv_font_montserrat_14);
      }
    } else {
      // Volume only: the level large, the receiver's name and input under it.
      ui->volLabel = makeWidgetLabel(parent, "", 10, 10, &lv_font_montserrat_10, muted,
                                     width - 20, LV_TEXT_ALIGN_CENTER);
      ui->volValue = makeWidgetLabel(parent, "--", 10, 28, &lv_font_montserrat_24,
                                     textPrimary(), width - 20, LV_TEXT_ALIGN_CENTER);
      ui->volIcon = makeWidgetLabel(parent, "", 10, 62, &lv_font_montserrat_12,
                                    npVolumeColour(), width - 20, LV_TEXT_ALIGN_CENTER);
    }
    return ui;
  }

  // Expanded.
  if (ui->hasMedia) {
    ui->mediaPanel = npBox(parent, 0, 0, width, height);
    ui->tag = makeWidgetLabel(ui->mediaPanel, "", 12, 7, &lv_font_montserrat_10, muted,
                              width - 44);
    makeLabel(ui->mediaPanel, LV_SYMBOL_CLOSE, width - 22, 5, &lv_font_montserrat_12, muted);
    const int art = NP_ART_SIZE;
    npBuildArt(ui, ui->mediaPanel, (width - art) / 2, 23, art);
    ui->title = npScrollingLabel(ui->mediaPanel, 12, 123, width - 24, &lv_font_montserrat_16,
                                 textPrimary(), LV_TEXT_ALIGN_CENTER);
    ui->subtitle = makeWidgetLabel(ui->mediaPanel, "", 12, 143, &lv_font_montserrat_12, muted,
                                   width - 24, LV_TEXT_ALIGN_CENTER);
    ui->trackWidth = (int16_t)(width - 32);
    ui->track = npTrack(ui->mediaPanel, 16, 161, width - 32, 4, npAccent(), &ui->fill);
    ui->left = makeWidgetLabel(ui->mediaPanel, "", 16, 167, &lv_font_montserrat_10, muted, 60);
    ui->right = makeWidgetLabel(ui->mediaPanel, "", width - 76, 167, &lv_font_montserrat_10,
                                muted, 60, LV_TEXT_ALIGN_RIGHT);
    ui->transport = npBox(ui->mediaPanel, 0, 180, width, 38);
    npKey(ui->transport, width / 2 - 78, 4, 50, 30, LV_SYMBOL_PREV, &lv_font_montserrat_14,
          NP_ACT_BACK, 0, &ui->backLabel);
    npKey(ui->transport, width / 2 - 19, 0, 38, 38, LV_SYMBOL_PLAY, &lv_font_montserrat_16,
          NP_ACT_PLAY_PAUSE, 0, &ui->playLabel, true);
    npKey(ui->transport, width / 2 + 28, 4, 50, 30, LV_SYMBOL_NEXT, &lv_font_montserrat_14,
          NP_ACT_FORWARD, 0, &ui->forwardLabel);
    ui->mediaPowerBtn = npKey(ui->mediaPanel, width / 2 - 50, 182, 100, 32,
                              LV_SYMBOL_POWER " Turn on", &lv_font_montserrat_12,
                              NP_ACT_MEDIA_POWER);
    lv_obj_add_flag(ui->mediaPowerBtn, LV_OBJ_FLAG_HIDDEN);

    if (ui->hasVolume) {
      lv_obj_t *line = glyphBar(ui->mediaPanel, 10, 222, width - 20, 1, 0, lv_color_white());
      lv_obj_set_style_bg_opa(line, LV_OPA_20, 0);
      ui->volIcon = makeLabel(ui->mediaPanel, LV_SYMBOL_VOLUME_MAX, 12, 228,
                              &lv_font_montserrat_10, npVolumeColour());
      ui->volLabel = makeWidgetLabel(ui->mediaPanel, "", 28, 228, &lv_font_montserrat_10, muted,
                                     width - 28 - 84);
      ui->volValue = makeWidgetLabel(ui->mediaPanel, "--", width - 84, 225,
                                     &lv_font_montserrat_14, textPrimary(), 72,
                                     LV_TEXT_ALIGN_RIGHT);
      bool receiver = npSettings.allowInput || npSettings.allowSoundMode;
      ui->volKeys = npBox(ui->mediaPanel, 0, 242, width, 24);
      int keyW = receiver ? 44 : (width - 24 - 12) / 3;
      int x = 12;
      npKey(ui->volKeys, x, 0, keyW, 22, LV_SYMBOL_MINUS, &lv_font_montserrat_12,
            NP_ACT_VOLUME_DOWN);
      x += keyW + 6;
      npKey(ui->volKeys, x, 0, keyW, 22, LV_SYMBOL_MUTE, &lv_font_montserrat_12, NP_ACT_MUTE, 0,
            &ui->muteLabel);
      x += keyW + 6;
      npKey(ui->volKeys, x, 0, keyW, 22, LV_SYMBOL_PLUS, &lv_font_montserrat_12,
            NP_ACT_VOLUME_UP);
      x += keyW + 6;
      if (receiver) {
        ui->receiverBtn = npKey(ui->volKeys, x, 0, width - 12 - x, 22, "Receiver " LV_SYMBOL_RIGHT,
                                &lv_font_montserrat_10, NP_ACT_SHOW_RECEIVER);
      }
      ui->volPowerBtn = npKey(ui->mediaPanel, 12, 242, width - 24, 22,
                              LV_SYMBOL_POWER " Turn on", &lv_font_montserrat_12,
                              NP_ACT_VOLUME_POWER);
      lv_obj_add_flag(ui->volPowerBtn, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (ui->hasVolume) {
    npBuildReceiverPanel(ui, parent, width, height, ui->hasMedia);
    ui->rxShown = !ui->hasMedia;
    if (ui->hasMedia) lv_obj_add_flag(ui->rxPanel, LV_OBJ_FLAG_HIDDEN);
  }
  return ui;
}

static void npBuildReceiverPanel(NowPlayingUi *ui, lv_obj_t *parent, int width, int height,
                                 bool withBack) {
  const lv_color_t muted = widgetMutedColour();
  ui->rxPanel = npBox(parent, 0, 0, width, height);
  if (withBack) {
    npKey(ui->rxPanel, 6, 4, 104, 20, LV_SYMBOL_LEFT " Now playing", &lv_font_montserrat_10,
          NP_ACT_SHOW_MEDIA);
    ui->rxName = makeWidgetLabel(ui->rxPanel, "", 114, 8, &lv_font_montserrat_10, muted,
                                 width - 114 - 10, LV_TEXT_ALIGN_RIGHT);
  } else {
    ui->rxName = makeWidgetLabel(ui->rxPanel, "", 12, 8, &lv_font_montserrat_10, muted,
                                 width - 44);
    makeLabel(ui->rxPanel, LV_SYMBOL_CLOSE, width - 22, 5, &lv_font_montserrat_12, muted);
  }
  ui->rxValue = makeWidgetLabel(ui->rxPanel, "--", 0, 28, &lv_font_montserrat_24, textPrimary(),
                                width, LV_TEXT_ALIGN_CENTER);
  ui->rxTrackWidth = (int16_t)(width - 40);
  npTrack(ui->rxPanel, 20, 60, width - 40, 5, npVolumeColour(), &ui->rxFill);
  ui->rxKeys = npBox(ui->rxPanel, 0, 72, width, 32);
  int keyW = (width - 24 - 12) / 3;
  npKey(ui->rxKeys, 12, 0, keyW, 30, LV_SYMBOL_MINUS, &lv_font_montserrat_14, NP_ACT_VOLUME_DOWN);
  npKey(ui->rxKeys, 12 + keyW + 6, 0, keyW, 30, LV_SYMBOL_MUTE " Mute", &lv_font_montserrat_12,
        NP_ACT_MUTE, 0, &ui->rxMuteLabel);
  npKey(ui->rxKeys, 12 + (keyW + 6) * 2, 0, keyW, 30, LV_SYMBOL_PLUS, &lv_font_montserrat_14,
        NP_ACT_VOLUME_UP);
  ui->rxPowerBtn = npKey(ui->rxPanel, 12, 72, width - 24, 30, LV_SYMBOL_POWER " Turn on",
                         &lv_font_montserrat_12, NP_ACT_VOLUME_POWER);
  lv_obj_add_flag(ui->rxPowerBtn, LV_OBJ_FLAG_HIDDEN);

  // The input and sound-mode pickers scroll; a drag here never closes the widget.
  ui->rxList = lv_obj_create(ui->rxPanel);
  lv_obj_remove_style_all(ui->rxList);
  lv_obj_set_pos(ui->rxList, 0, 110);
  lv_obj_set_size(ui->rxList, width, height - 112);
  lv_obj_set_style_pad_left(ui->rxList, 12, 0);
  lv_obj_set_style_pad_right(ui->rxList, 12, 0);
  lv_obj_set_style_pad_bottom(ui->rxList, 8, 0);
  lv_obj_set_flex_flow(ui->rxList, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_row(ui->rxList, 5, 0);
  lv_obj_set_style_pad_column(ui->rxList, 6, 0);
  lv_obj_add_flag(ui->rxList, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_scroll_dir(ui->rxList, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(ui->rxList, LV_SCROLLBAR_MODE_ACTIVE);
}

/* Rebuilds the picker grids when the lists themselves change. */
static void npRebuildReceiverLists(NowPlayingUi *ui) {
  if (!ui->rxList || !lv_obj_is_valid(ui->rxList)) return;
  uint32_t hash = npVolume.listHash ^ (npSettings.allowInput ? 0x51u : 0) ^
                  (npSettings.allowSoundMode ? 0xA200u : 0);
  if (hash == ui->rxListHash) return;
  ui->rxListHash = hash;
  lv_obj_clean(ui->rxList);
  ui->rxSourceCount = 0;
  ui->rxSoundCount = 0;
  ui->rxSourceText = nullptr;
  ui->rxSoundText = nullptr;
  const lv_color_t muted = widgetMutedColour();
  int inner = ui->width - 24;
  int half = (inner - 6) / 2;

  auto heading = [&](const char *text) -> lv_obj_t * {
    lv_obj_t *label = makeWidgetLabel(ui->rxList, text, 0, 0, &lv_font_montserrat_10, muted, inner);
    lv_obj_add_flag(label, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    return label;
  };

  if (npSettings.allowInput && npVolume.sourceCount) {
    heading("Input");
    for (uint8_t i = 0; i < npVolume.sourceCount; i++) {
      lv_obj_t *key = npKey(ui->rxList, 0, 0, half, 26, npVolume.sources[i],
                            &lv_font_montserrat_12, NP_ACT_SOURCE, i);
      if (i == 0) lv_obj_add_flag(key, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
      ui->rxSourceBtns[ui->rxSourceCount++] = key;
    }
  } else {
    ui->rxSourceText = heading("Input");
  }
  if (npSettings.allowSoundMode && npVolume.soundModeCount) {
    heading("Sound mode");
    for (uint8_t i = 0; i < npVolume.soundModeCount; i++) {
      lv_obj_t *key = npKey(ui->rxList, 0, 0, half, 26, npVolume.soundModes[i],
                            &lv_font_montserrat_12, NP_ACT_SOUND_MODE, i);
      if (i == 0) lv_obj_add_flag(key, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
      ui->rxSoundBtns[ui->rxSoundCount++] = key;
    }
  } else {
    ui->rxSoundText = heading("Sound mode");
  }
}

static void npSetHidden(lv_obj_t *obj, bool hidden) {
  if (!obj || !lv_obj_is_valid(obj)) return;
  if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void npSetFill(lv_obj_t *fill, int trackWidth, uint32_t permille) {
  if (!fill || !lv_obj_is_valid(fill)) return;
  if (permille > 1000) permille = 1000;
  lv_obj_set_width(fill, (int)((uint32_t)trackWidth * permille / 1000U));
}

static void npRefreshArt(NowPlayingUi *ui, bool show) {
  if (!ui->artBox || !lv_obj_is_valid(ui->artBox)) return;
  const void *source = show ? npArtSource() : nullptr;
  if (source != ui->artShown) {
    ui->artShown = source;
    if (source) {
      lv_img_set_src(ui->art, source);
      lv_img_set_pivot(ui->art, 0, 0);
      uint16_t zoom = (uint16_t)((uint32_t)ui->artSize * 256U / NP_ART_SIZE);
      lv_img_set_zoom(ui->art, zoom);
      lv_obj_set_pos(ui->art, 0, 0);
      lv_obj_clear_flag(ui->art, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui->artGlyph, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(ui->art, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui->artGlyph, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static void npVolumeDetail(char *out, size_t size, bool withName) {
  const NpEntityState &v = npVolume;
  const char *name = v.friendly[0] ? v.friendly : npSettings.volumeEntity;
  if (!v.valid) { snprintf(out, size, "%s%s", withName ? name : "", withName ? " - offline" : "Offline"); return; }
  if (npIsOff(v)) { snprintf(out, size, "%s is off", name); return; }
  String text = withName ? String(name) : String();
  if (v.source[0]) { if (text.length()) text += " - "; text += v.source; }
  if (v.muted) { if (text.length()) text += " - "; text += "Muted"; }
  else if (v.soundMode[0]) { if (text.length()) text += " - "; text += v.soundMode; }
  strlcpy(out, text.c_str(), size);
}

void refreshNowPlayingUi(NowPlayingUi *ui) {
  if (!ui) return;
  char text[128];

  if (ui->hasMedia) {
    const NpEntityState &m = npMedia;
    bool off = npIsOff(m);
    bool playing = npIsPlaying(m);
    const char *name = m.friendly[0] ? m.friendly : npSettings.mediaEntity;
    // Who is playing what: "Netflix - Den Apple TV".
    if (!off && m.app[0] && strcmp(m.app, name) != 0) snprintf(text, sizeof(text), "%s - %s", m.app, name);
    else strlcpy(text, name, sizeof(text));
    setWidgetLabelText(ui->tag, ui->slim && !off && m.app[0] ? m.app : text);

    const char *title = m.title[0] ? m.title : (m.app[0] && !off ? m.app : name);
    if (!m.valid) title = name;
    setWidgetLabelText(ui->title, title);
    const char *status = !m.valid ? "Not reachable"
      : off ? "Off" : npStateIs(m, "idle") ? "Idle"
      : npStateIs(m, "paused") ? "Paused" : npStateIs(m, "on") ? "On" : "";
    setWidgetLabelText(ui->subtitle, m.subtitle[0] && !off ? m.subtitle : status);

    bool timed = !off && m.hasPosition && m.duration > 0;
    npSetHidden(ui->track, !timed);
    npSetHidden(ui->left, !timed);
    npSetHidden(ui->right, !timed);
    npSetHidden(ui->stateGlyph, !timed);
    if (timed) {
      uint32_t position = npMediaPosition();
      npSetFill(ui->fill, ui->trackWidth, (uint32_t)((uint64_t)position * 1000ULL / m.duration));
      formatWidgetClock(text, sizeof(text), position, false);
      setWidgetLabelText(ui->left, text);
      if (ui->expanded) formatWidgetClock(text, sizeof(text), m.duration, false);
      else formatWidgetClock(text, sizeof(text), m.duration - position, true);
      setWidgetLabelText(ui->right, text);
      setWidgetLabelText(ui->stateGlyph, playing ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE);
    }
    npRefreshArt(ui, npSettings.artwork && !off && m.valid);

    if (ui->expanded) {
      bool canTurnOn = off && m.valid && (m.supported & NP_FEATURE_TURN_ON);
      npSetHidden(ui->transport, off);
      npSetHidden(ui->mediaPowerBtn, !canTurnOn);
      if (ui->playLabel) setWidgetLabelText(ui->playLabel, playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
      bool seek = (m.supported & NP_FEATURE_SEEK) && m.hasPosition;
      if (ui->backLabel) setWidgetLabelText(ui->backLabel, seek ? "-10s" : LV_SYMBOL_PREV);
      if (ui->forwardLabel) setWidgetLabelText(ui->forwardLabel, seek ? "+10s" : LV_SYMBOL_NEXT);
    }
  }

  if (ui->hasVolume) {
    const NpEntityState &v = npVolume;
    bool off = npIsOff(v);
    char level[24];
    npFormatVolume(level, sizeof(level), v);
    const char *value = !v.valid ? "--" : off ? "Off" : level;
    if (ui->volValue) {
      setWidgetLabelText(ui->volValue, value);
      lv_obj_set_style_text_decor(ui->volValue, v.muted && !off ? LV_TEXT_DECOR_STRIKETHROUGH
                                                                : LV_TEXT_DECOR_NONE, 0);
      lv_obj_set_style_text_color(ui->volValue, v.muted && !off ? widgetMutedColour()
                                                                : textPrimary(), 0);
    }
    bool volumeOnly = !ui->hasMedia && !ui->expanded && !ui->slim;
    if (volumeOnly) {
      setWidgetLabelText(ui->volLabel, v.friendly[0] ? v.friendly : npSettings.volumeEntity);
      npVolumeDetail(text, sizeof(text), false);
      setWidgetLabelText(ui->volIcon, text);
      lv_obj_set_style_text_color(ui->volIcon, v.muted ? npMutedRed() : npVolumeColour(), 0);
    } else {
      npVolumeDetail(text, sizeof(text), true);
      if (ui->volLabel) {
        setWidgetLabelText(ui->volLabel, text);
        lv_obj_set_style_text_color(ui->volLabel, v.muted && !off ? npMutedRed()
                                                                  : widgetMutedColour(), 0);
      }
      if (ui->volIcon) {
        setWidgetLabelText(ui->volIcon, off ? LV_SYMBOL_POWER
                                            : v.muted ? LV_SYMBOL_MUTE : LV_SYMBOL_VOLUME_MAX);
        lv_obj_set_style_text_color(ui->volIcon, off ? widgetMutedColour()
                                    : v.muted ? npMutedRed() : npVolumeColour(), 0);
      }
    }

    if (ui->expanded) {
      bool canTurnOn = off && v.valid;
      npSetHidden(ui->volKeys, off);
      npSetHidden(ui->volPowerBtn, !canTurnOn || !ui->hasMedia);
      if (ui->muteLabel) setWidgetLabelText(ui->muteLabel, v.muted ? LV_SYMBOL_VOLUME_MAX : LV_SYMBOL_MUTE);

      if (ui->rxPanel) {
        setWidgetLabelText(ui->rxName, v.friendly[0] ? v.friendly : npSettings.volumeEntity);
        setWidgetLabelText(ui->rxValue, value);
        lv_obj_set_style_text_color(ui->rxValue, v.muted && !off ? npMutedRed() : textPrimary(), 0);
        npSetFill(ui->rxFill, ui->rxTrackWidth,
                  v.hasVolume && !off ? (uint32_t)(constrain(v.volume, 0.0f, 1.0f) * 1000.0f) : 0);
        npSetHidden(ui->rxKeys, off);
        npSetHidden(ui->rxPowerBtn, !canTurnOn);
        if (ui->rxMuteLabel) setWidgetLabelText(ui->rxMuteLabel,
          v.muted ? LV_SYMBOL_VOLUME_MAX " Unmute" : LV_SYMBOL_MUTE " Mute");
        npRebuildReceiverLists(ui);
        for (uint8_t i = 0; i < ui->rxSourceCount; i++) {
          npSetKeySelected(ui->rxSourceBtns[i], strcmp(npVolume.sources[i], v.source) == 0);
        }
        for (uint8_t i = 0; i < ui->rxSoundCount; i++) {
          npSetKeySelected(ui->rxSoundBtns[i],
                           strcmp(npVolume.soundModes[i], v.soundModeSelected) == 0);
        }
        if (ui->rxSourceText) {
          snprintf(text, sizeof(text), "Input:  %s", v.source[0] ? v.source : "--");
          setWidgetLabelText(ui->rxSourceText, text);
        }
        if (ui->rxSoundText) {
          snprintf(text, sizeof(text), "Sound mode:  %s", v.soundMode[0] ? v.soundMode : "--");
          setWidgetLabelText(ui->rxSoundText, text);
        }
      }
    }
  }
}

void npShowReceiverPanel(bool show) {
  for (uint8_t i = 0; i < widgetExpandedInstanceCount; i++) {
    WidgetInstance &instance = widgetExpandedInstances[i];
    if (instance.kind != WIDGET_NOWPLAYING || !instance.nowPlayingUi) continue;
    NowPlayingUi *ui = instance.nowPlayingUi;
    if (!ui->rxPanel || !ui->mediaPanel) continue;
    ui->rxShown = show;
    npSetHidden(ui->rxPanel, !show);
    npSetHidden(ui->mediaPanel, show);
    if (show) npRequestRefresh(false, true);
    refreshNowPlayingUi(ui);
  }
}

/* ------------------------------------------------------------- pop-up */

static void npRefreshPopup() {
  if (!npPopup || !lv_obj_is_valid(npPopup)) return;
  const NpEntityState &v = npVolume;
  char level[24];
  npFormatVolume(level, sizeof(level), v);
  bool off = npIsOff(v);
  char name[80];
  snprintf(name, sizeof(name), "%s%s", v.friendly[0] ? v.friendly : npSettings.volumeEntity,
           v.muted && !off ? "  -  Muted" : "");
  setWidgetLabelText(npPopupName, name);
  setWidgetLabelText(npPopupValue, !v.valid ? "--" : off ? "Off" : level);
  lv_color_t accent = v.muted && !off ? npMutedRed() : npVolumeColour();
  lv_obj_set_style_text_color(npPopupName, v.muted && !off ? npMutedRed() : widgetMutedColour(), 0);
  lv_obj_set_style_text_color(npPopupValue, v.muted && !off ? npMutedRed() : textPrimary(), 0);
  lv_obj_set_style_bg_color(npPopupFill, accent, 0);
  lv_obj_set_style_border_color(npPopup, v.muted && !off ? lvRgb(92, 58, 58) : lvRgb(58, 74, 92), 0);
  npSetFill(npPopupFill, NP_POPUP_TRACK,
            v.hasVolume && !off ? (uint32_t)(constrain(v.volume, 0.0f, 1.0f) * 1000.0f) : 0);
}

void npHideVolumePopup() {
  if (npPopup && lv_obj_is_valid(npPopup)) lv_obj_del(npPopup);
  npPopup = nullptr;
  npPopupName = npPopupValue = npPopupFill = nullptr;
  npPopupUntilMs = 0;
}

bool nowPlayingExpandedOpen();

void npShowVolumePopup() {
  if (!npSettings.volumePopup || !npSettings.volumeEntity[0]) return;
  // The open widget already shows the level, larger.
  if (nowPlayingExpandedOpen()) return;
  npPopupUntilMs = millis() + 2200UL;
  if (!npPopup || !lv_obj_is_valid(npPopup)) {
    npPopup = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(npPopup);
    lv_obj_set_size(npPopup, 200, 78);
    lv_obj_align(npPopup, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(npPopup, 14, 0);
    lv_obj_set_style_bg_color(npPopup, lvRgb(28, 38, 51), 0);
    lv_obj_set_style_bg_opa(npPopup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(npPopup, 1, 0);
    lv_obj_set_style_border_color(npPopup, lvRgb(58, 74, 92), 0);
    lv_obj_clear_flag(npPopup, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(npPopup, LV_OBJ_FLAG_SCROLLABLE);
    npPopupName = makeWidgetLabel(npPopup, "", 8, 8, &lv_font_montserrat_10, widgetMutedColour(),
                                  184, LV_TEXT_ALIGN_CENTER);
    npPopupValue = makeWidgetLabel(npPopup, "--", 0, 22, &lv_font_montserrat_24, textPrimary(),
                                   200, LV_TEXT_ALIGN_CENTER);
    npTrack(npPopup, 12, 60, NP_POPUP_TRACK, 6, npVolumeColour(), &npPopupFill);
  }
  npRefreshPopup();
}

/* ------------------------------------------------------- the widget host */

bool nowPlayingExpandedOpen() {
  if (!widgetExpandedOverlay || widgetExpandedClosing) return false;
  for (uint8_t i = 0; i < widgetExpandedInstanceCount; i++) {
    if (widgetExpandedInstances[i].kind == WIDGET_NOWPLAYING) return true;
  }
  return false;
}

bool nowPlayingOnScreen() {
  if (nowPlayingExpandedOpen()) return true;
  for (uint8_t i = 0; i < widgetInstanceCount; i++) {
    const WidgetInstance &instance = widgetInstances[i];
    if (instance.kind == WIDGET_NOWPLAYING && instance.card && lv_obj_is_valid(instance.card)) {
      return true;
    }
  }
  return false;
}

static void npRefreshAll() {
  for (uint8_t i = 0; i < widgetInstanceCount; i++) {
    WidgetInstance &instance = widgetInstances[i];
    if (instance.kind == WIDGET_NOWPLAYING && instance.card && lv_obj_is_valid(instance.card)) {
      refreshNowPlayingUi(instance.nowPlayingUi);
    }
  }
  if (widgetExpandedOverlay && !widgetExpandedClosing) {
    for (uint8_t i = 0; i < widgetExpandedInstanceCount; i++) {
      WidgetInstance &instance = widgetExpandedInstances[i];
      if (instance.kind == WIDGET_NOWPLAYING) refreshNowPlayingUi(instance.nowPlayingUi);
    }
  }
  npRefreshPopup();
}

/* From the loop: button presses, artwork, the pop-up timer and repaints. */
void serviceNowPlaying(uint32_t now) {
  uint8_t action = npPendingAction;
  if (action != NP_ACT_NONE) {
    uint8_t index = npPendingIndex;
    npPendingAction = NP_ACT_NONE;
    runNowPlayingAction(action, index);
  }
  if (npPopup && npPopupUntilMs && (int32_t)(now - npPopupUntilMs) >= 0) npHideVolumePopup();
  if (displaySleeping) {
    if (npPopup) npHideVolumePopup();
    return;
  }
  serviceNowPlayingArtwork(now);
  if (npDirty) {
    npDirty = false;
    npRefreshAll();
  }
}

/* Reads widgets.nowPlaying from the config. */
void loadNowPlayingSettings(JsonObjectConst widgets) {
  JsonObjectConst settings = widgets["nowPlaying"].as<JsonObjectConst>();
  NowPlayingSettings next = { "", "", true, true, true, -80.0f, true, true };
  if (!settings.isNull()) {
    strlcpy(next.mediaEntity, settings["mediaEntity"] | "", sizeof(next.mediaEntity));
    strlcpy(next.volumeEntity, settings["volumeEntity"] | "", sizeof(next.volumeEntity));
    next.allowInput = settings["allowInput"] | true;
    next.allowSoundMode = settings["allowSoundMode"] | true;
    next.volumeDb = strcmp(settings["volumeUnit"] | "db", "percent") != 0;
    next.dbOffset = constrain((float)(settings["dbOffset"] | -80.0f), -120.0f, 20.0f);
    next.volumePopup = settings["volumePopup"] | true;
    next.artwork = settings["artwork"] | true;
  }
  if (strcmp(next.mediaEntity, npSettings.mediaEntity) != 0) {
    memset(&npMedia, 0, sizeof(npMedia));
    npArtRelease();
  }
  if (strcmp(next.volumeEntity, npSettings.volumeEntity) != 0) memset(&npVolume, 0, sizeof(npVolume));
  npSettings = next;
  npPollNextMs = 0;
  Serial.printf("Widgets: now playing media=%s volume=%s (%s, input %s, sound mode %s)\n",
                npSettings.mediaEntity[0] ? npSettings.mediaEntity : "-",
                npSettings.volumeEntity[0] ? npSettings.volumeEntity : "-",
                npSettings.volumeDb ? "dB" : "%",
                npSettings.allowInput ? "allowed" : "locked",
                npSettings.allowSoundMode ? "allowed" : "locked");
}
