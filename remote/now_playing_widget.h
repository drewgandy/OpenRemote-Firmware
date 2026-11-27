/* ---------------------------------------------------------------------- *
   Now Playing widget

   One widget with two halves, either of which may be left empty:

     - Now playing: the app, title, artwork and position of a player (an
       Apple TV, a Roku, a Chromecast, a TV's own apps).
     - Volume: the volume, mute, input and sound mode of a receiver or TV.

   Each half has a source - where its state comes from - and, separately,
   where its keys go:

     Source        Shows                                            Default keys
     Home Asst.    everything                                       HA services
     MQTT          whatever the topics carry (JSON or per field)    none
     Homebridge    power, play state, input, mute, volume 0-100     characteristic writes
     Chromecast    title, artwork, position (the dock's Media feed) none
     None          only what the remote can remember (IR)           none

   "Keys send to" picks any device in WebConfig - IR, Home Assistant, MQTT,
   Homebridge, Bluetooth - and maps each key to one of its commands, so an
   IR-only receiver still gets working volume, mute and input keys. With
   keys mapped they always win over the source's defaults.

   Each half labels its own source, so it is always clear what a key press
   will control. A volume key for the chosen receiver pressed anywhere on
   the remote shows a short pop-up, for receivers that cannot draw their
   own on-screen display over a 4K picture.

   Configuration is global, like every library widget: there is one remote
   per room, so the players are pinned rather than following the page.

   Home Assistant and Homebridge are polled directly while the widget (or
   the pop-up) is on screen and the display is awake; MQTT arrives on the
   remote's subscriptions (or the dock's, for short values); Chromecast
   comes from the dock exactly as it does for the Media widget.

   Included from OpenRemote_1.0.ino after the widget helpers it uses.
 * ---------------------------------------------------------------------- */
#pragma once

static const uint8_t NP_MAX_LIST = 24;
static const uint8_t NP_LIST_TEXT = 32;
static const int16_t NP_ART_SIZE = 96;

enum NpSourceType : uint8_t {
  NP_SRC_NONE = 0,
  NP_SRC_HA = 1,
  NP_SRC_MQTT = 2,
  NP_SRC_HOMEBRIDGE = 3,
  NP_SRC_CAST = 4
};

/* What an MQTT source can carry. In JSON mode each entry is the key (or
   dotted path) inside the state topic's payload; otherwise each entry is a
   topic of its own. Names match WebConfig's. */
enum NpMqttField : uint8_t {
  NPF_STATE, NPF_TITLE, NPF_ARTIST, NPF_APP, NPF_POSITION, NPF_DURATION,
  NPF_VOLUME, NPF_MUTED, NPF_SOURCE, NPF_SOUND_MODE, NPF_PICTURE, NPF_SOURCES,
  NPF_COUNT
};
static const char *const NP_MQTT_FIELD_KEYS[NPF_COUNT] = {
  "state", "title", "artist", "app", "position", "duration", "volume", "muted",
  "source", "soundMode", "picture", "sources"
};

/* The keys a half can map to another device's commands. */
enum NpKey : uint8_t {
  NPK_VOLUME_UP, NPK_VOLUME_DOWN, NPK_MUTE, NPK_POWER_ON, NPK_POWER_OFF,
  NPK_PLAY_PAUSE, NPK_NEXT, NPK_PREVIOUS, NPK_COUNT
};
static const char *const NP_KEY_NAMES[NPK_COUNT] = {
  "volumeUp", "volumeDown", "mute", "powerOn", "powerOff", "playPause", "next", "previous"
};

enum NpVolumeScale : uint8_t { NP_SCALE_UNIT = 0, NP_SCALE_PERCENT = 1, NP_SCALE_DB = 2 };

struct NpSourceSettings {
  uint8_t type;                        // NpSourceType
  char entity[64];                     // Home Assistant media_player
  char accessory[72];                  // Homebridge service uniqueId
  char mqttJsonTopic[96];              // empty: one topic per field
  char mqttFields[NPF_COUNT][80];
  uint8_t volumeScale;                 // NpVolumeScale, for MQTT
  char name[40];                       // shown in place of the source's own name
  char keysDevice[48];                 // a WebConfig device id, or empty
  char keys[NPK_COUNT][48];            // that device's command ids
  char inputs[NP_MAX_LIST][48];        // its input commands, in order
  uint8_t inputCount;
};

struct NowPlayingSettings {
  NpSourceSettings media;
  NpSourceSettings volume;
  bool allowInput;       // the input grid is shown and usable
  bool allowSoundMode;   // the sound-mode grid is shown and usable
  bool volumeDb;         // dB rather than percent
  float dbOffset;        // dB = volume_level x 100 + dbOffset (Denon: -80)
  bool volumePopup;      // pop-up on any page when a volume key is pressed
  bool artwork;          // download and show artwork
};

/* Defaults must match defaultWidgetSettings().nowPlaying in WebConfig. */
NowPlayingSettings npSettings = {};

static void npDefaultSettings(NowPlayingSettings &settings) {
  memset(&settings, 0, sizeof(settings));
  settings.media.type = NP_SRC_HA;
  settings.volume.type = NP_SRC_HA;
  settings.allowInput = true;
  settings.allowSoundMode = true;
  settings.volumeDb = true;
  settings.dbOffset = -80.0f;
  settings.volumePopup = true;
  settings.artwork = true;
}

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
  int32_t sourceIds[NP_MAX_LIST];   // Homebridge input identifiers
  uint8_t sourceCount;
  uint8_t hbPower;       // Homebridge: 1 "Active", 2 "On", 0 neither
  bool hasPlayState;     // the source reports playing/paused at all
  bool guessed;          // state remembered by the remote, not reported
  bool hbInputsLoaded;   // Homebridge input names fetched
  char soundModes[NP_MAX_LIST][NP_LIST_TEXT];
  uint8_t soundModeCount;
  uint32_t listHash;     // changes when either list changes
};

NpEntityState npMedia = {};
NpEntityState npVolume = {};
/* One scratch copy for every reader - they all run on the loop, one at a
   time - kept static because two lists of names are too big for the loop
   task's stack, and shared because internal RAM is scarce. */
NpEntityState npScratch = {};

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

/* Which face a widget draws, chosen from its height. */
enum NpLayout : uint8_t {
  NP_LAYOUT_SLIM = 0,    // 1 row, 224x44
  NP_LAYOUT_TWO,         // 2 rows, 224x96
  NP_LAYOUT_THREE,       // 3 rows, 224x148
  NP_LAYOUT_FULL         // 4-5 rows (224x200, 224x252) or the expanded view
};

struct NowPlayingUi {
  uint8_t layout;        // NpLayout
  bool expanded;         // in the expanded overlay rather than on the page
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
  // Two- and three-row faces reflow when artwork or a position appears.
  int8_t flowArt;        // -1 not laid out yet
  int8_t flowTimed;
  // Volume strip / row
  lv_obj_t *volIcon;
  lv_obj_t *volDetail;   // input and sound mode, under the name (2-3 rows)
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

/* A half is in use when it has a source or keys to send. */
static bool npHalfActive(const NpSourceSettings &half) {
  if (half.keysDevice[0]) return true;
  switch (half.type) {
    case NP_SRC_HA: return half.entity[0] != '\0';
    case NP_SRC_HOMEBRIDGE: return half.accessory[0] != '\0';
    case NP_SRC_MQTT: {
      if (half.mqttJsonTopic[0]) return true;
      for (uint8_t i = 0; i < NPF_COUNT; i++) if (half.mqttFields[i][0]) return true;
      return false;
    }
    case NP_SRC_CAST: return true;
    default: return false;
  }
}

bool nowPlayingConfigured() {
  return npHalfActive(npSettings.media) || npHalfActive(npSettings.volume);
}

static const char *npHalfName(const NpSourceSettings &half, const NpEntityState &s,
                              const char *fallback) {
  if (half.name[0]) return half.name;
  if (s.friendly[0]) return s.friendly;
  if (half.entity[0]) return half.entity;
  return fallback;
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
bool npReadHa(NpEntityState &s, const char *entityId) {
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
  NpEntityState &next = npScratch;
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

  next.hasPlayState = true;
  bool changed = memcmp(&s, &next, sizeof(next)) != 0;
  s = next;
  if (changed) npDirty = true;
  return true;
}

/* ---------------------------------------------------- other sources *
   Each fills the same NpEntityState the Home Assistant reader does, so the
   faces never need to know where a value came from.
 * ---------------------------------------------------------------------- */

/* "Playing", "PAUSED", "stopped", "ON", "true", "1" ... to the HA vocabulary. */
static void npNormaliseState(const char *raw, char *out, size_t size) {
  char lower[24];
  size_t n = 0;
  for (const char *c = raw ? raw : ""; *c && n < sizeof(lower) - 1; c++) {
    lower[n++] = (char)tolower((unsigned char)*c);
  }
  lower[n] = '\0';
  const char *value = lower;
  if (!strcmp(lower, "play") || !strcmp(lower, "playing")) value = "playing";
  else if (!strcmp(lower, "pause") || !strcmp(lower, "paused")) value = "paused";
  else if (!strcmp(lower, "stop") || !strcmp(lower, "stopped") || !strcmp(lower, "idle")) value = "idle";
  else if (!strcmp(lower, "on") || !strcmp(lower, "true") || !strcmp(lower, "1") ||
           !strcmp(lower, "active")) value = "on";
  else if (!strcmp(lower, "off") || !strcmp(lower, "false") || !strcmp(lower, "0") ||
           !strcmp(lower, "standby") || !strcmp(lower, "inactive")) value = "off";
  else if (!n) value = "on";
  strlcpy(out, value, size);
}

static bool npTruthy(const char *raw) {
  if (!raw) return false;
  return !strcasecmp(raw, "true") || !strcasecmp(raw, "on") || !strcmp(raw, "1") ||
         !strcasecmp(raw, "yes") || !strcasecmp(raw, "muted");
}

/* A volume in the source's own scale to Home Assistant's 0..1. */
static float npScaledVolume(float raw, uint8_t scale) {
  float level = scale == NP_SCALE_PERCENT ? raw / 100.0f
              : scale == NP_SCALE_DB ? (raw - npSettings.dbOffset) / 100.0f
              : raw;
  return constrain(level, 0.0f, 1.0f);
}

/* Applies one field's text value. Lists arrive comma-separated here. */
static void npApplyField(NpEntityState &s, const NpSourceSettings &half, uint8_t field,
                         const char *value) {
  if (!value) value = "";
  switch (field) {
    case NPF_STATE: npNormaliseState(value, s.state, sizeof(s.state)); s.hasPlayState = true; break;
    case NPF_TITLE: npCopyText(value, s.title, sizeof(s.title)); break;
    case NPF_ARTIST: npCopyText(value, s.subtitle, sizeof(s.subtitle)); break;
    case NPF_APP: npCopyText(value, s.app, sizeof(s.app)); break;
    case NPF_POSITION:
      s.position = (uint32_t)max(0.0f, (float)atof(value));
      s.positionAtMs = millis();
      s.hasPosition = true;
      break;
    case NPF_DURATION: s.duration = (uint32_t)max(0.0f, (float)atof(value)); break;
    case NPF_VOLUME:
      if (value[0]) { s.volume = npScaledVolume((float)atof(value), half.volumeScale); s.hasVolume = true; }
      break;
    case NPF_MUTED: s.muted = npTruthy(value); break;
    case NPF_SOURCE: npCopyText(value, s.source, sizeof(s.source)); break;
    case NPF_SOUND_MODE:
      npCopyText(value, s.soundMode, sizeof(s.soundMode));
      strlcpy(s.soundModeSelected, s.soundMode, sizeof(s.soundModeSelected));
      break;
    case NPF_PICTURE: strlcpy(s.picture, value, sizeof(s.picture)); break;
    case NPF_SOURCES: {
      s.sourceCount = 0;
      const char *start = value;
      while (*start && s.sourceCount < NP_MAX_LIST) {
        const char *end = strchr(start, ',');
        size_t length = end ? (size_t)(end - start) : strlen(start);
        char item[NP_LIST_TEXT];
        size_t copy = min(length, sizeof(item) - 1);
        memcpy(item, start, copy);
        item[copy] = '\0';
        String trimmed(item);
        trimmed.trim();
        if (trimmed.length()) npCopyText(trimmed.c_str(), s.sources[s.sourceCount++], NP_LIST_TEXT);
        if (!end) break;
        start = end + 1;
      }
      break;
    }
  }
}

static void npRehashLists(NpEntityState &s) {
  uint32_t hash = 2166136261UL;
  for (uint8_t i = 0; i < s.sourceCount; i++) hash = npFnv(hash, s.sources[i]);
  hash = npFnv(hash, "|");
  for (uint8_t i = 0; i < s.soundModeCount; i++) hash = npFnv(hash, s.soundModes[i]);
  s.listHash = hash;
}

/* A dotted path ("player.volume") inside a JSON payload. */
static JsonVariantConst npJsonPath(JsonVariantConst root, const char *path) {
  JsonVariantConst at = root;
  char key[48];
  const char *c = path;
  while (*c) {
    size_t n = 0;
    while (*c && *c != '.' && n < sizeof(key) - 1) key[n++] = *c++;
    key[n] = '\0';
    if (*c == '.') c++;
    at = at[key];
    if (at.isNull()) return at;
  }
  return at;
}

static void npApplyJsonPayload(NpEntityState &s, const NpSourceSettings &half,
                               const char *payload, size_t length) {
  JsonDocument doc(&psramJsonAllocator);
  if (deserializeJson(doc, payload, length)) {
    Serial.println("Now Playing: MQTT state topic is not JSON");
    return;
  }
  s.valid = true;
  if (!s.state[0]) strlcpy(s.state, "on", sizeof(s.state));
  for (uint8_t f = 0; f < NPF_COUNT; f++) {
    if (!half.mqttFields[f][0]) continue;
    JsonVariantConst value = npJsonPath(doc.as<JsonVariantConst>(), half.mqttFields[f]);
    if (value.isNull()) continue;
    if (f == NPF_SOURCES && value.is<JsonArrayConst>()) {
      s.sourceCount = 0;
      for (JsonVariantConst item : value.as<JsonArrayConst>()) {
        if (s.sourceCount >= NP_MAX_LIST) break;
        npCopyText(item | "", s.sources[s.sourceCount++], NP_LIST_TEXT);
      }
      continue;
    }
    String text;
    if (value.is<const char *>()) text = value.as<const char *>();
    else if (value.is<bool>()) text = value.as<bool>() ? "true" : "false";
    else if (value.is<float>()) text = String(value.as<float>(), 3);
    else serializeJson(value, text);
    npApplyField(s, half, f, text.c_str());
  }
  npRehashLists(s);
  npDirty = true;
}

static void npApplyMqttMessage(NpEntityState &s, const NpSourceSettings &half,
                               const char *topic, const char *payload, size_t length) {
  if (half.type != NP_SRC_MQTT || !topic) return;
  if (half.mqttJsonTopic[0]) {
    if (strcmp(topic, half.mqttJsonTopic) == 0) npApplyJsonPayload(s, half, payload, length);
    return;
  }
  for (uint8_t f = 0; f < NPF_COUNT; f++) {
    if (!half.mqttFields[f][0] || strcmp(topic, half.mqttFields[f]) != 0) continue;
    char value[200];
    size_t copy = min(length, sizeof(value) - 1);
    memcpy(value, payload, copy);
    value[copy] = '\0';
    s.valid = true;
    if (!s.state[0]) strlcpy(s.state, "on", sizeof(s.state));
    npApplyField(s, half, f, value);
    if (f == NPF_SOURCES) npRehashLists(s);
    npDirty = true;
  }
}

/*
  Every MQTT message the remote's own client receives, before the tiles'
  48-character copy - so a JSON state topic arrives whole. Called from
  mqttMessageArrived(), on the loop.
*/
void nowPlayingMqttMessage(const char *topic, const uint8_t *payload, unsigned int length) {
  npApplyMqttMessage(npMedia, npSettings.media, topic, (const char *)payload, length);
  npApplyMqttMessage(npVolume, npSettings.volume, topic, (const char *)payload, length);
}

/* Topics the MQTT subscriptions should include. */
uint8_t nowPlayingMqttTopics(const char *topics[], uint8_t maximum) {
  uint8_t count = 0;
  const NpSourceSettings *halves[2] = { &npSettings.media, &npSettings.volume };
  for (const NpSourceSettings *half : halves) {
    if (half->type != NP_SRC_MQTT) continue;
    if (half->mqttJsonTopic[0]) {
      if (count < maximum) topics[count++] = half->mqttJsonTopic;
      continue;
    }
    for (uint8_t f = 0; f < NPF_COUNT && count < maximum; f++) {
      if (half->mqttFields[f][0]) topics[count++] = half->mqttFields[f];
    }
  }
  return count;
}

/*
  With a dock carrying MQTT, per-field values come back through the tiles'
  live-state table (up to 47 characters each). Picked up from there.
*/
static void npMqttFromLiveStates(NpEntityState &s, const NpSourceSettings &half) {
  if (half.type != NP_SRC_MQTT || half.mqttJsonTopic[0]) return;
  static char seen[2][NPF_COUNT][48];
  uint8_t slot = &half == &npSettings.media ? 0 : 1;
  for (uint8_t f = 0; f < NPF_COUNT; f++) {
    if (!half.mqttFields[f][0]) continue;
    const char *value = liveStateFor(half.mqttFields[f]);
    if (!value || strcmp(value, seen[slot][f]) == 0) continue;
    strlcpy(seen[slot][f], value, sizeof(seen[slot][f]));
    npApplyMqttMessage(s, half, half.mqttFields[f], value, strlen(value));
  }
}

/* Homebridge: one service through the UI's REST API. HomeKit has no titles or
   artwork, so this is power, play state, input, mute and volume. */
static bool npHomebridgeInputNames(NpEntityState &s, int aid) {
  String response, error;
  int status = 0;
  if (!homebridgeAuthorizedRequest("GET", "/api/accessories", "", response, status, error)) {
    Serial.printf("Now Playing: Homebridge inputs - %s\n", error.c_str());
    return false;
  }
  JsonDocument filter;
  JsonObject item = filter[0].to<JsonObject>();
  item["aid"] = true;
  item["type"] = true;
  item["values"]["Identifier"] = true;
  item["values"]["ConfiguredName"] = true;
  item["values"]["IsConfigured"] = true;
  item["values"]["CurrentVisibilityState"] = true;
  JsonDocument doc(&psramJsonAllocator);
  if (deserializeJson(doc, response, DeserializationOption::Filter(filter))) return false;
  s.sourceCount = 0;
  for (JsonObjectConst service : doc.as<JsonArrayConst>()) {
    if ((service["aid"] | -1) != aid || strcmp(service["type"] | "", "InputSource") != 0) continue;
    JsonObjectConst values = service["values"].as<JsonObjectConst>();
    if ((values["IsConfigured"] | 1) == 0 || (values["CurrentVisibilityState"] | 0) != 0) continue;
    if (s.sourceCount >= NP_MAX_LIST) break;
    npCopyText(values["ConfiguredName"] | "Input", s.sources[s.sourceCount], NP_LIST_TEXT);
    s.sourceIds[s.sourceCount] = values["Identifier"] | 0;
    s.sourceCount++;
  }
  npRehashLists(s);
  return true;
}

bool npReadHomebridge(NpEntityState &s, const NpSourceSettings &half) {
  String response, error;
  int status = 0;
  String path = String("/api/accessories/") + half.accessory;
  if (!homebridgeAuthorizedRequest("GET", path, "", response, status, error)) {
    Serial.printf("Now Playing: Homebridge %s - %s\n", half.accessory, error.c_str());
    if (status == 404) { s.valid = false; npDirty = true; }
    return false;
  }
  JsonDocument filter;
  filter["aid"] = true;
  filter["serviceName"] = true;
  filter["values"] = true;
  JsonDocument doc(&psramJsonAllocator);
  if (deserializeJson(doc, response, DeserializationOption::Filter(filter))) return false;
  JsonObjectConst values = doc["values"].as<JsonObjectConst>();
  int aid = doc["aid"] | -1;

  NpEntityState &next = npScratch;
  next = s;   // keeps the input names between reads
  next.valid = true;
  next.title[0] = next.subtitle[0] = next.app[0] = '\0';
  npCopyText(values["ConfiguredName"] | (doc["serviceName"] | half.accessory),
             next.friendly, sizeof(next.friendly));
  bool on = true;
  next.hbPower = 0;
  if (!values["Active"].isNull()) { on = (values["Active"] | 0) != 0; next.hbPower = 1; }
  else if (!values["On"].isNull()) { on = values["On"] | false; next.hbPower = 2; }
  int media = values["CurrentMediaState"] | -1;
  next.hasPlayState = media >= 0;
  const char *state = !on ? "off" : media == 0 ? "playing" : media == 1 ? "paused"
                    : media == 2 ? "idle" : "on";
  strlcpy(next.state, state, sizeof(next.state));
  next.hasVolume = !values["Volume"].isNull();
  if (next.hasVolume) next.volume = constrain((float)(values["Volume"] | 0) / 100.0f, 0.0f, 1.0f);
  next.muted = values["Mute"] | false;
  next.hasPosition = false;

  if (!values["ActiveIdentifier"].isNull()) {
    if (!next.hbInputsLoaded) next.hbInputsLoaded = npHomebridgeInputNames(next, aid);
    int32_t active = values["ActiveIdentifier"] | -1;
    next.source[0] = '\0';
    for (uint8_t i = 0; i < next.sourceCount; i++) {
      if (next.sourceIds[i] == active) strlcpy(next.source, next.sources[i], sizeof(next.source));
    }
  }
  bool changed = memcmp(&s, &next, sizeof(next)) != 0;
  s = next;
  if (changed) npDirty = true;
  return true;
}

/* Chromecast: what the dock reports for the Media widget, as this half. */
static void npFromChromecast(NpEntityState &s, const NpSourceSettings &half) {
  NpEntityState &next = npScratch;
  memset(&next, 0, sizeof(next));
  next.valid = true;
  strlcpy(next.friendly, half.name[0] ? half.name : "Chromecast", sizeof(next.friendly));
  strlcpy(next.state, !nowPlaying.valid ? "idle" : nowPlaying.playing ? "playing" : "paused",
          sizeof(next.state));
  next.hasPlayState = nowPlaying.valid;
  if (nowPlaying.valid) {
    strlcpy(next.title, nowPlaying.title, sizeof(next.title));
    strlcpy(next.subtitle, nowPlaying.subtitle, sizeof(next.subtitle));
    strlcpy(next.app, nowPlaying.source, sizeof(next.app));
    next.hasPosition = nowPlaying.duration > 0;
    next.position = nowPlaying.position;
    next.duration = nowPlaying.duration;
    next.positionAtMs = nowPlaying.positionAtMs;
  }
  if (memcmp(&s, &next, sizeof(next)) != 0) {
    s = next;
    npDirty = true;
  }
}

/* The keys device, by id. */
static Device *npFindDevice(const char *id) {
  if (!id || !id[0]) return nullptr;
  for (uint8_t d = 0; d < DEVICE_COUNT; d++) {
    if (strcmp(devices[d].id, id) == 0) return &devices[d];
  }
  return nullptr;
}

static DeviceCommand *npFindCommand(Device *device, const char *id) {
  if (!device || !id || !id[0]) return nullptr;
  for (uint16_t c = 0; c < device->commandCount; c++) {
    if (strcmp(device->commands[c].id, id) == 0) return &device->commands[c];
  }
  return nullptr;
}

/* None: nothing reports back, so show only what the remote itself knows -
   the device's tracked power and the last input picked here. */
char npGuessedInput[2][NP_LIST_TEXT] = {"", ""};

static void npFromNothing(NpEntityState &s, const NpSourceSettings &half, uint8_t slot) {
  NpEntityState &next = npScratch;
  memset(&next, 0, sizeof(next));
  Device *device = npFindDevice(half.keysDevice);
  next.valid = true;
  next.guessed = true;
  strlcpy(next.friendly, half.name[0] ? half.name : device ? device->name : "Receiver",
          sizeof(next.friendly));
  bool on = true;
  if (device && device->powerTrackingEnabled) on = rememberedDeviceOn(*device);
  strlcpy(next.state, on ? "on" : "off", sizeof(next.state));
  strlcpy(next.source, npGuessedInput[slot], sizeof(next.source));
  if (memcmp(&s, &next, sizeof(next)) != 0) {
    s = next;
    npDirty = true;
  }
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
  if (!haBaseUrl.length() && strncmp(picture, "http", 4) != 0) return false;
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

/* The picture the media half shows: the Media widget's poster for a
   Chromecast, otherwise this widget's own. */
const void *npMediaArtSource() {
  if (npSettings.media.type == NP_SRC_CAST) return mediaArtSource();
  return npArtSource();
}

void serviceNowPlayingArtwork(uint32_t now) {
  uint8_t type = npSettings.media.type;
  if (!npSettings.artwork || (type != NP_SRC_HA && type != NP_SRC_MQTT)) {
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

static bool npPolled(const NpSourceSettings &half) {
  if (half.type == NP_SRC_HA) return half.entity[0] && homeAssistantConfigured();
  if (half.type == NP_SRC_HOMEBRIDGE) {
    return half.accessory[0] && homebridgeAddress.length() && homebridgeUsername.length();
  }
  return false;
}

/* Whether anything on screen needs a fresh read over the network. */
bool nowPlayingPollWanted() {
  if (displaySleeping) return false;
  if (!npPolled(npSettings.media) && !npPolled(npSettings.volume)) return false;
  return nowPlayingOnScreen() || nowPlayingPopupActive() ||
         npRefreshMediaAtMs || npRefreshVolumeAtMs || npRefreshVolumeLateMs;
}

static bool npDue(uint32_t &at, uint32_t now) {
  if (!at || (int32_t)(now - at) < 0) return false;
  at = 0;
  return true;
}

static bool npRead(NpEntityState &s, const NpSourceSettings &half) {
  if (!npPolled(half)) return false;
  if (half.type == NP_SRC_HA) return npReadHa(s, half.entity);
  return npReadHomebridge(s, half);
}

/* One read, if one is due. The caller has the station connected. */
void pollNowPlaying(uint32_t now) {
  const NpSourceSettings &media = npSettings.media;
  const NpSourceSettings &volume = npSettings.volume;
  bool pollMedia = npPolled(media), pollVolume = npPolled(volume);
  bool same = pollMedia && pollVolume && media.type == volume.type &&
              (media.type == NP_SRC_HA ? strcmp(media.entity, volume.entity) == 0
                                       : strcmp(media.accessory, volume.accessory) == 0);
  bool readMedia = false, readVolume = false;
  if (pollVolume && (npDue(npRefreshVolumeAtMs, now) || npDue(npRefreshVolumeLateMs, now))) {
    readVolume = true;
  } else if (pollMedia && npDue(npRefreshMediaAtMs, now)) {
    readMedia = true;
  } else if (!npPollNextMs || (int32_t)(now - npPollNextMs) >= 0) {
    // Turn about: the media player, then the receiver.
    npPollTurn ^= 1;
    if (npPollTurn && pollMedia) readMedia = true;
    else if (pollVolume) readVolume = true;
    else readMedia = pollMedia;
    // Both every ~4 s; a single player every ~2.5 s.
    npPollNextMs = now + ((pollMedia && pollVolume && !same) ? 2000UL : 2500UL);
  }
  if (same && (readMedia || readVolume)) {
    if (npRead(npMedia, media)) npVolume = npMedia;
    return;
  }
  if (readMedia) npRead(npMedia, media);
  if (readVolume) npRead(npVolume, volume);
}

/* Sources that arrive rather than being fetched: refreshed from the loop. */
static void npServiceLocalSources(uint32_t now) {
  static uint32_t lastMs = 0;
  if (now - lastMs < 400UL) return;
  lastMs = now;
  const NpSourceSettings *halves[2] = { &npSettings.media, &npSettings.volume };
  NpEntityState *states[2] = { &npMedia, &npVolume };
  for (uint8_t i = 0; i < 2; i++) {
    const NpSourceSettings &half = *halves[i];
    if (half.type == NP_SRC_CAST) npFromChromecast(*states[i], half);
    else if (half.type == NP_SRC_MQTT) npMqttFromLiveStates(*states[i], half);
    else if (half.type == NP_SRC_NONE && half.keysDevice[0]) npFromNothing(*states[i], half, i);
  }
}

/* Whether the Media widget's Chromecast feed should run for this widget. */
bool nowPlayingWantsCast() {
  return npSettings.media.type == NP_SRC_CAST && !displaySleeping && nowPlayingOnScreen();
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
  return transmitIrCommand(command);
}

/* A Homebridge characteristic write: operation 0 sets, 1 toggles. */
static bool npHomebridgeWrite(const char *accessory, const char *characteristic, float value,
                              uint8_t valueType = 2, uint8_t operation = 0) {
  if (!accessory || !accessory[0]) return false;
  DeviceCommand command;
  memset(&command, 0, sizeof(command));
  command.kind = DeviceCommand::HOMEBRIDGE;
  strlcpy(command.homebridgeAccessoryId, accessory, sizeof(command.homebridgeAccessoryId));
  strlcpy(command.homebridgeCharacteristic, characteristic, sizeof(command.homebridgeCharacteristic));
  command.homebridgeValue = value;
  command.homebridgeValueType = valueType;
  command.homebridgeOperation = operation;
  return transmitIrCommand(command);
}

/* A key through the half's chosen device, when one is mapped. */
static bool npMappedKey(const NpSourceSettings &half, uint8_t key, bool &handled) {
  handled = false;
  if (!half.keysDevice[0] || key >= NPK_COUNT || !half.keys[key][0]) return false;
  DeviceCommand *command = npFindCommand(npFindDevice(half.keysDevice), half.keys[key]);
  if (!command) {
    Serial.printf("Now Playing: key %s is mapped to a missing command\n", NP_KEY_NAMES[key]);
    return false;
  }
  handled = true;
  return transmitRuntimeCommand(command) != RUNTIME_COMMAND_FAILED;
}

/* HomeKit RemoteKey values. */
static const int NP_HK_NEXT = 2, NP_HK_PREVIOUS = 3, NP_HK_PLAY_PAUSE = 11;

/*
  Sends one key for a half: its mapped device command if there is one,
  otherwise what its source can do itself. Returns whether anything went.
*/
static bool npSendKey(const NpSourceSettings &half, NpEntityState &s, uint8_t key) {
  bool handled = false;
  bool sent = npMappedKey(half, key, handled);
  if (handled) return sent;
  if (half.type == NP_SRC_HA) {
    const char *entity = half.entity;
    switch (key) {
      case NPK_VOLUME_UP: return npCall(entity, "media_player.volume_up");
      case NPK_VOLUME_DOWN: return npCall(entity, "media_player.volume_down");
      case NPK_MUTE: return npCall(entity, "media_player.volume_mute", "is_volume_muted",
                                   s.muted ? HA_DATA_FALSE : HA_DATA_TRUE);
      case NPK_POWER_ON: return npCall(entity, "media_player.turn_on");
      case NPK_POWER_OFF: return npCall(entity, "media_player.turn_off");
      case NPK_PLAY_PAUSE: return npCall(entity, "media_player.media_play_pause");
      case NPK_NEXT: return npCall(entity, "media_player.media_next_track");
      case NPK_PREVIOUS: return npCall(entity, "media_player.media_previous_track");
    }
  } else if (half.type == NP_SRC_HOMEBRIDGE) {
    const char *accessory = half.accessory;
    switch (key) {
      case NPK_VOLUME_UP: return npHomebridgeWrite(accessory, "VolumeSelector", 0);
      case NPK_VOLUME_DOWN: return npHomebridgeWrite(accessory, "VolumeSelector", 1);
      case NPK_MUTE: return npHomebridgeWrite(accessory, "Mute", s.muted ? 0 : 1, 1);
      case NPK_POWER_ON:
        return s.hbPower == 2 ? npHomebridgeWrite(accessory, "On", 1, 1)
                              : npHomebridgeWrite(accessory, "Active", 1);
      case NPK_POWER_OFF:
        return s.hbPower == 2 ? npHomebridgeWrite(accessory, "On", 0, 1)
                              : npHomebridgeWrite(accessory, "Active", 0);
      case NPK_PLAY_PAUSE: return npHomebridgeWrite(accessory, "RemoteKey", NP_HK_PLAY_PAUSE);
      case NPK_NEXT: return npHomebridgeWrite(accessory, "RemoteKey", NP_HK_NEXT);
      case NPK_PREVIOUS: return npHomebridgeWrite(accessory, "RemoteKey", NP_HK_PREVIOUS);
    }
  }
  Serial.printf("Now Playing: no command for %s - map one in WebConfig\n", NP_KEY_NAMES[key]);
  return false;
}

static bool npHasKey(const NpSourceSettings &half, uint8_t key) {
  if (half.keysDevice[0] && half.keys[key][0]) return true;
  return half.type == NP_SRC_HA || half.type == NP_SRC_HOMEBRIDGE;
}

/* The input grid: the mapped device's input commands when there are any,
   otherwise the source's own list. */
static uint8_t npInputCount() {
  const NpSourceSettings &half = npSettings.volume;
  if (half.keysDevice[0] && half.inputCount) return half.inputCount;
  return npVolume.sourceCount;
}

static const char *npInputLabel(uint8_t index) {
  const NpSourceSettings &half = npSettings.volume;
  if (half.keysDevice[0] && half.inputCount) {
    DeviceCommand *command = npFindCommand(npFindDevice(half.keysDevice), half.inputs[index]);
    return command ? command->label : "?";
  }
  return index < npVolume.sourceCount ? npVolume.sources[index] : "";
}

static bool npSelectInput(uint8_t index) {
  const NpSourceSettings &half = npSettings.volume;
  if (index >= npInputCount()) return false;
  const char *label = npInputLabel(index);
  bool sent = false;
  if (half.keysDevice[0] && half.inputCount) {
    DeviceCommand *command = npFindCommand(npFindDevice(half.keysDevice), half.inputs[index]);
    sent = command && transmitRuntimeCommand(command) != RUNTIME_COMMAND_FAILED;
  } else if (half.type == NP_SRC_HA) {
    sent = npCall(half.entity, "media_player.select_source", "source", HA_DATA_TEXT, label);
  } else if (half.type == NP_SRC_HOMEBRIDGE) {
    sent = npHomebridgeWrite(half.accessory, "ActiveIdentifier", (float)npVolume.sourceIds[index]);
  }
  if (sent) {
    strlcpy(npVolume.source, label, sizeof(npVolume.source));
    strlcpy(npGuessedInput[1], label, sizeof(npGuessedInput[1]));
    npDirty = true;
  }
  return sent;
}

void npShowVolumePopup(const char *action);
void npShowReceiverPanel(bool show);

void runNowPlayingAction(uint8_t action, uint8_t index) {
  const NpSourceSettings &media = npSettings.media;
  const NpSourceSettings &volume = npSettings.volume;
  switch (action) {
    case NP_ACT_PLAY_PAUSE:
      if (npSendKey(media, npMedia, NPK_PLAY_PAUSE)) {
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
      bool mapped = media.keysDevice[0] && media.keys[forward ? NPK_NEXT : NPK_PREVIOUS][0];
      if (!mapped && media.type == NP_SRC_HA && (npMedia.supported & NP_FEATURE_SEEK) &&
          npMedia.hasPosition) {
        int32_t target = (int32_t)npMediaPosition() + (forward ? 10 : -10);
        if (target < 0) target = 0;
        if (npMedia.duration && target > (int32_t)npMedia.duration) target = npMedia.duration;
        ok = npCall(media.entity, "media_player.media_seek", "seek_position", HA_DATA_NUMBER,
                    nullptr, (float)target);
        if (ok) {
          npMedia.position = (uint32_t)target;
          npMedia.positionAtMs = millis();
          npDirty = true;
        }
      } else {
        ok = npSendKey(media, npMedia, forward ? NPK_NEXT : NPK_PREVIOUS);
      }
      if (ok) npRequestRefresh(true, false);
      break;
    }
    case NP_ACT_VOLUME_DOWN:
      npSendKey(volume, npVolume, NPK_VOLUME_DOWN);
      break;
    case NP_ACT_VOLUME_UP:
      npSendKey(volume, npVolume, NPK_VOLUME_UP);
      break;
    case NP_ACT_MUTE:
      if (npSendKey(volume, npVolume, NPK_MUTE) && !npVolume.guessed) {
        npVolume.muted = !npVolume.muted;
        npDirty = true;
      }
      break;
    case NP_ACT_VOLUME_POWER:
      if (npSendKey(volume, npVolume, NPK_POWER_ON)) npRequestRefresh(false, true);
      break;
    case NP_ACT_MEDIA_POWER:
      if (npSendKey(media, npMedia, NPK_POWER_ON)) npRequestRefresh(true, false);
      break;
    case NP_ACT_SOURCE:
      if (npSettings.allowInput && npSelectInput(index)) npRequestRefresh(false, true);
      break;
    case NP_ACT_SOUND_MODE:
      if (!npSettings.allowSoundMode || index >= npVolume.soundModeCount) break;
      if (volume.type == NP_SRC_HA &&
          npCall(volume.entity, "media_player.select_sound_mode", "sound_mode", HA_DATA_TEXT,
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

/* Whether a command is one of the half's mapped keys, and which. */
static int8_t npMappedKeyFor(const NpSourceSettings &half, const DeviceCommand &command) {
  if (!half.keysDevice[0] || !command.id[0]) return -1;
  Device *device = npFindDevice(half.keysDevice);
  if (!device || &command < &device->commands[0] ||
      &command >= &device->commands[device->commandCount]) return -1;
  for (uint8_t k = 0; k < NPK_COUNT; k++) {
    if (half.keys[k][0] && strcmp(half.keys[k], command.id) == 0) return (int8_t)k;
  }
  return -1;
}

/* Whether a command reads as a volume key, wherever it is going: "Volume Up",
   "VOL-", "Mute", a Roku's remote volume_up key, a Homebridge VolumeSelector.
   Returns the pop-up's action text, or nullptr. */
static const char *npVolumeKeyAction(const DeviceCommand &command) {
  auto classify = [](const char *text) -> const char * {
    if (!text || !text[0]) return nullptr;
    char lower[48];
    size_t n = 0;
    for (const char *c = text; *c && n < sizeof(lower) - 1; c++) lower[n++] = (char)tolower((unsigned char)*c);
    lower[n] = '\0';
    if (strstr(lower, "mute")) return "Mute";
    bool volume = strstr(lower, "vol") != nullptr;
    if (!volume) return nullptr;
    if (strstr(lower, "up") || strchr(lower, '+')) return "Volume up";
    if (strstr(lower, "down") || strchr(lower, '-')) return "Volume down";
    return "Volume";
  };
  if (command.kind == DeviceCommand::HOMEASSISTANT && command.haService) {
    const char *service = command.haService;
    if (!strncmp(service, "media_player.volume_", 20)) {
      if (!strcmp(service + 20, "up")) return "Volume up";
      if (!strcmp(service + 20, "down")) return "Volume down";
      if (!strcmp(service + 20, "mute")) return "Mute";
      return "Volume";
    }
    // remote.send_command with command: volume_up (a Roku, an Apple TV)
    if (!strcmp(service, "remote.send_command")) {
      const char *action = classify(command.haDataText);
      if (action) return action;
    }
  }
  if (command.kind == DeviceCommand::HOMEBRIDGE) {
    const char *c = command.homebridgeCharacteristic;
    if (!strcmp(c, "VolumeSelector")) return command.homebridgeValue >= 0.5f ? "Volume down" : "Volume up";
    if (!strcmp(c, "Mute")) return "Mute";
  }
  return classify(command.label);
}

/*
  Every command the remote sends passes through here once sent, whichever
  page, key or transport it came from - so a hardware volume key mapped to
  the receiver brings up the pop-up just as the widget's own keys do.
*/
void nowPlayingNoteCommand(const DeviceCommand &command, bool sent) {
  if (!sent) return;
  const NpSourceSettings &media = npSettings.media;
  const NpSourceSettings &volume = npSettings.volume;
  int8_t volumeKey = npMappedKeyFor(volume, command);
  bool forMedia = npMappedKeyFor(media, command) >= 0;
  bool forVolume = volumeKey >= 0;
  const char *action = nullptr;
  if (volumeKey == NPK_VOLUME_UP) action = "Volume up";
  else if (volumeKey == NPK_VOLUME_DOWN) action = "Volume down";
  else if (volumeKey == NPK_MUTE) action = "Mute";

  if (command.kind == DeviceCommand::HOMEASSISTANT && command.haEntityId && command.haService &&
      strncmp(command.haService, "media_player.", 13) == 0) {
    const char *service = command.haService + 13;
    if (media.type == NP_SRC_HA && media.entity[0] && !strcmp(command.haEntityId, media.entity)) {
      forMedia = true;
    }
    if (volume.type == NP_SRC_HA && volume.entity[0] && !strcmp(command.haEntityId, volume.entity)) {
      forVolume = true;
      if (!strcmp(service, "volume_up")) action = "Volume up";
      else if (!strcmp(service, "volume_down")) action = "Volume down";
      else if (!strcmp(service, "volume_mute") || !strcmp(service, "volume_set")) action = "Mute";
      if (!strcmp(service, "volume_set")) action = "Volume";
    }
  }
  if (command.kind == DeviceCommand::HOMEBRIDGE) {
    if (media.type == NP_SRC_HOMEBRIDGE && !strcmp(command.homebridgeAccessoryId, media.accessory)) {
      forMedia = true;
    }
    if (volume.type == NP_SRC_HOMEBRIDGE && !strcmp(command.homebridgeAccessoryId, volume.accessory)) {
      forVolume = true;
      const char *c = command.homebridgeCharacteristic;
      if (!strcmp(c, "VolumeSelector")) action = command.homebridgeValue >= 0.5f ? "Volume down" : "Volume up";
      else if (!strcmp(c, "Mute")) action = "Mute";
      else if (!strcmp(c, "Volume")) action = "Volume";
    }
  }
  // One remote per room: a volume key aimed anywhere - the TV's or the
  // Roku's own volume keys, which reach the receiver over HDMI-CEC - still
  // changes this room's volume. Show it, and read the receiver back.
  if (!action) {
    action = npVolumeKeyAction(command);
    if (action) forVolume = true;
  }
  if (forMedia) npRequestRefresh(true, false);
  if (forVolume) npRequestRefresh(false, true);
  if (action) npShowVolumePopup(action);
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
static void npSetHidden(lv_obj_t *obj, bool hidden);
bool nowPlayingExpandedOpen();

/* The transport row (back, play/pause, forward) centred at y, keys h high. */
static void npBuildTransport(NowPlayingUi *ui, lv_obj_t *panel, int width, int y, int h) {
  ui->transport = npBox(panel, 0, y, width, h);
  int side = h - 6;
  npKey(ui->transport, width / 2 - 78, 3, 50, side, LV_SYMBOL_PREV, &lv_font_montserrat_14,
        NP_ACT_BACK, 0, &ui->backLabel);
  npKey(ui->transport, width / 2 - h / 2, 0, h, h, LV_SYMBOL_PLAY, &lv_font_montserrat_16,
        NP_ACT_PLAY_PAUSE, 0, &ui->playLabel, true);
  npKey(ui->transport, width / 2 + 28, 3, 50, side, LV_SYMBOL_NEXT, &lv_font_montserrat_14,
        NP_ACT_FORWARD, 0, &ui->forwardLabel);
  ui->mediaPowerBtn = npKey(panel, width / 2 - 50, y + 2, 100, h - 4,
                            LV_SYMBOL_POWER " Turn on", &lv_font_montserrat_12,
                            NP_ACT_MEDIA_POWER);
  lv_obj_add_flag(ui->mediaPowerBtn, LV_OBJ_FLAG_HIDDEN);
}

/* The volume row of the full face: label and level, then - mute + Receiver. */
static void npBuildVolumeRow(NowPlayingUi *ui, lv_obj_t *panel, int width, int y) {
  const lv_color_t muted = widgetMutedColour();
  lv_obj_t *line = glyphBar(panel, 10, y, width - 20, 1, 0, lv_color_white());
  lv_obj_set_style_bg_opa(line, LV_OPA_20, 0);
  ui->volIcon = makeLabel(panel, LV_SYMBOL_VOLUME_MAX, 12, y + 6, &lv_font_montserrat_12,
                          npVolumeColour());
  ui->volLabel = makeWidgetLabel(panel, "", 30, y + 6, &lv_font_montserrat_12, muted,
                                 width - 30 - 90);
  ui->volValue = makeWidgetLabel(panel, "--", width - 92, y + 2, &lv_font_montserrat_18,
                                 textPrimary(), 80, LV_TEXT_ALIGN_RIGHT);
  bool receiver = npSettings.allowInput || npSettings.allowSoundMode;
  ui->volKeys = npBox(panel, 0, y + 24, width, 26);
  int keyW = receiver ? 44 : (width - 24 - 12) / 3;
  int x = 12;
  npKey(ui->volKeys, x, 0, keyW, 24, LV_SYMBOL_MINUS, &lv_font_montserrat_12, NP_ACT_VOLUME_DOWN);
  x += keyW + 6;
  npKey(ui->volKeys, x, 0, keyW, 24, LV_SYMBOL_MUTE, &lv_font_montserrat_12, NP_ACT_MUTE, 0,
        &ui->muteLabel);
  x += keyW + 6;
  npKey(ui->volKeys, x, 0, keyW, 24, LV_SYMBOL_PLUS, &lv_font_montserrat_12, NP_ACT_VOLUME_UP);
  x += keyW + 6;
  if (receiver) {
    ui->receiverBtn = npKey(ui->volKeys, x, 0, width - 12 - x, 24, "Receiver " LV_SYMBOL_RIGHT,
                            &lv_font_montserrat_10, NP_ACT_SHOW_RECEIVER);
  }
  ui->volPowerBtn = npKey(panel, 12, y + 24, width - 24, 24, LV_SYMBOL_POWER " Turn on",
                          &lv_font_montserrat_12, NP_ACT_VOLUME_POWER);
  lv_obj_add_flag(ui->volPowerBtn, LV_OBJ_FLAG_HIDDEN);
}

/*
  Two and three rows: one layout, three rows being the same thing with more
  room and larger type. The media half's pieces move to suit what the player
  actually reports - without artwork the text takes the whole width; without
  a position the progress row is dropped and the text sits lower.
*/
struct NpFlowMetrics {
  const lv_font_t *title;
  const lv_font_t *line;
  int titleH;      // line height of the title font
  int lineH;
  int art;         // artwork side, with and without a progress row
  int artTimed;
};

static NpFlowMetrics npFlowMetrics(const NowPlayingUi *ui) {
  if (ui->layout == NP_LAYOUT_THREE) {
    return { &lv_font_montserrat_20, &lv_font_montserrat_14, 24, 17, 64, 52 };
  }
  return { &lv_font_montserrat_16, &lv_font_montserrat_12, 20, 15, 40, 30 };
}

static void npReflow(NowPlayingUi *ui, bool art, bool timed) {
  if (ui->layout != NP_LAYOUT_TWO && ui->layout != NP_LAYOUT_THREE) return;
  if (!ui->hasMedia || !ui->mediaPanel) return;
  if (ui->flowArt == (int8_t)art && ui->flowTimed == (int8_t)timed) return;
  ui->flowArt = (int8_t)art;
  ui->flowTimed = (int8_t)timed;
  const NpFlowMetrics f = npFlowMetrics(ui);
  const int w = ui->width;
  const int mediaH = lv_obj_get_height(ui->mediaPanel);
  const int progressH = timed ? 15 : 0;
  const int blockH = f.titleH + f.lineH + progressH;
  int artSize = timed ? f.artTimed : f.art;
  // Media only: the artwork can grow, but not past the 96px it is stored at.
  if (!ui->hasVolume) artSize = min(mediaH - 16, min((int)NP_ART_SIZE, artSize * 2));
  int top = max(4, (mediaH - max(blockH, art ? artSize : 0)) / 2);
  npSetHidden(ui->artBox, !art);
  if (art && ui->artBox) {
    lv_obj_set_pos(ui->artBox, 10, (mediaH - artSize) / 2);
    lv_obj_set_size(ui->artBox, artSize, artSize);
    ui->artSize = (int16_t)artSize;
    ui->artShown = (const void *)1;   // re-apply the zoom for the new size
  }
  int textX = art ? 10 + artSize + 10 : 10;
  int textW = w - textX - 10;
  int textY = art ? (mediaH - blockH) / 2 : top;
  lv_obj_set_pos(ui->title, textX, textY);
  lv_obj_set_width(ui->title, textW);
  lv_obj_set_pos(ui->subtitle, textX, textY + f.titleH);
  lv_obj_set_width(ui->subtitle, textW);
  if (timed) {
    // On one line: elapsed, the bar, remaining.
    int rowY = textY + f.titleH + f.lineH + 1;
    lv_obj_set_pos(ui->left, textX, rowY);
    lv_obj_set_pos(ui->right, textX + textW - 40, rowY);
    ui->trackWidth = (int16_t)(textW - 84);
    lv_obj_set_pos(ui->track, textX + 42, rowY + 5);
    lv_obj_set_width(ui->track, ui->trackWidth);
  }
}

static void npBuildMediaText(NowPlayingUi *ui, lv_obj_t *panel) {
  const lv_color_t muted = widgetMutedColour();
  const NpFlowMetrics f = npFlowMetrics(ui);
  npBuildArt(ui, panel, 10, 6, f.art);
  ui->title = npScrollingLabel(panel, 10, 8, ui->width - 20, f.title, textPrimary(),
                               LV_TEXT_ALIGN_LEFT);
  ui->subtitle = makeWidgetLabel(panel, "", 10, 29, f.line, muted, ui->width - 20);
  ui->track = npTrack(panel, 52, 50, 120, 3, npAccent(), &ui->fill);
  ui->trackWidth = 120;
  ui->left = makeWidgetLabel(panel, "", 10, 45, &lv_font_montserrat_10, muted, 40);
  ui->right = makeWidgetLabel(panel, "", ui->width - 50, 45, &lv_font_montserrat_10, muted, 40,
                              LV_TEXT_ALIGN_RIGHT);
}

/* The volume half of the two- and three-row faces: icon, name over input and
   sound mode, the level large on the right. */
static void npBuildVolumeBlock(NowPlayingUi *ui, lv_obj_t *parent, int y, int h, bool big) {
  const lv_color_t muted = widgetMutedColour();
  const int w = ui->width;
  const int nameH = big ? 22 : 18;
  const int detailH = big ? 17 : 15;
  int nameY = y + (h - nameH - detailH) / 2;
  ui->volIcon = makeLabel(parent, LV_SYMBOL_VOLUME_MAX, 10, nameY + (nameH + detailH) / 2 - (big ? 11 : 9),
                          big ? &lv_font_montserrat_20 : &lv_font_montserrat_16, npVolumeColour());
  int textX = big ? 38 : 34;
  ui->volLabel = makeWidgetLabel(parent, "", textX, nameY,
                                 big ? &lv_font_montserrat_18 : &lv_font_montserrat_14,
                                 textPrimary(), w - textX - 100);
  ui->volDetail = makeWidgetLabel(parent, "", textX, nameY + nameH,
                                  big ? &lv_font_montserrat_14 : &lv_font_montserrat_12,
                                  muted, w - textX - 100);
  ui->volValue = makeWidgetLabel(parent, "--", w - 104, y + (h - 28) / 2, &lv_font_montserrat_24,
                                 textPrimary(), 94, LV_TEXT_ALIGN_RIGHT);
}

/*
  Builds the face into `parent`; refreshNowPlayingUi() writes the labels.
  The layout follows the height: one row, two, three, or the full face -
  transport keys and volume keys included - at four or five rows and in the
  expanded view.
*/
NowPlayingUi *buildNowPlayingFace(lv_obj_t *parent, int width, int height, bool expanded,
                                  bool slim) {
  NowPlayingUi *ui = (NowPlayingUi *)lv_mem_alloc(sizeof(NowPlayingUi));
  if (!ui) return nullptr;
  memset(ui, 0, sizeof(*ui));
  ui->expanded = expanded;
  ui->slim = slim && !expanded;
  ui->layout = expanded || height >= 190 ? NP_LAYOUT_FULL
             : ui->slim || height < 70 ? NP_LAYOUT_SLIM
             : height < 120 ? NP_LAYOUT_TWO : NP_LAYOUT_THREE;
  ui->hasMedia = npHalfActive(npSettings.media);
  ui->hasVolume = npHalfActive(npSettings.volume);
  ui->width = (int16_t)width;
  ui->height = (int16_t)height;
  ui->rxListHash = 0xFFFFFFFFUL;
  ui->flowArt = ui->flowTimed = -1;
  lv_obj_add_event_cb(parent, npDeleted, LV_EVENT_DELETE, ui);
  const lv_color_t muted = widgetMutedColour();

  if (!ui->hasMedia && !ui->hasVolume) {
    int y = ui->layout == NP_LAYOUT_SLIM ? 6 : height / 2 - 20;
    makeWidgetLabel(parent, "Now Playing", 0, y, &lv_font_montserrat_14,
                    textPrimary(), width, LV_TEXT_ALIGN_CENTER);
    makeWidgetLabel(parent, "Choose the players in WebConfig", 0, y + 18,
                    &lv_font_montserrat_10, muted, width, LV_TEXT_ALIGN_CENTER);
    return ui;
  }

  if (ui->layout == NP_LAYOUT_SLIM) {
    // One row: artwork, title and app, the level on the right.
    int textX = 10;
    if (ui->hasMedia) {
      npBuildArt(ui, parent, 6, 6, 32);
      textX = 46;
      int textW = width - textX - (ui->hasVolume ? 84 : 8);
      ui->title = npScrollingLabel(parent, textX, 5, textW, &lv_font_montserrat_14,
                                   textPrimary(), LV_TEXT_ALIGN_LEFT);
      ui->subtitle = makeWidgetLabel(parent, "", textX, 24, &lv_font_montserrat_10, muted, textW);
    } else {
      ui->volIcon = makeLabel(parent, LV_SYMBOL_VOLUME_MAX, 10, 13, &lv_font_montserrat_16,
                              npVolumeColour());
      ui->volLabel = makeWidgetLabel(parent, "", 34, 5, &lv_font_montserrat_14, textPrimary(),
                                     width - 34 - 92);
      ui->volDetail = makeWidgetLabel(parent, "", 34, 24, &lv_font_montserrat_10, muted,
                                      width - 34 - 92);
    }
    if (ui->hasVolume) {
      ui->volValue = makeWidgetLabel(parent, "--", width - 90, 10, &lv_font_montserrat_20,
                                     textPrimary(), 82, LV_TEXT_ALIGN_RIGHT);
    }
    return ui;
  }

  if (ui->layout == NP_LAYOUT_TWO || ui->layout == NP_LAYOUT_THREE) {
    bool three = ui->layout == NP_LAYOUT_THREE;
    if (ui->hasMedia) {
      // The media half takes a little over half; the volume half the rest.
      int mediaH = ui->hasVolume ? (three ? 84 : 52) : height;
      ui->mediaPanel = npBox(parent, 0, 0, width, mediaH);
      npBuildMediaText(ui, ui->mediaPanel);
      if (ui->hasVolume) {
        lv_obj_t *line = glyphBar(parent, 10, mediaH, width - 20, 1, 0, lv_color_white());
        lv_obj_set_style_bg_opa(line, LV_OPA_20, 0);
        npBuildVolumeBlock(ui, parent, mediaH + 1, height - mediaH - 1, three);
      }
      npReflow(ui, false, false);
    } else {
      // Volume only: the whole tile.
      npBuildVolumeBlock(ui, parent, 0, height, true);
    }
    return ui;
  }

  // The full face: four or five rows, or the expanded view.
  if (ui->hasMedia) {
    ui->mediaPanel = npBox(parent, 0, 0, width, height);
    ui->tag = makeWidgetLabel(ui->mediaPanel, "", 12, 7, &lv_font_montserrat_10, muted,
                              width - 44);
    if (expanded) makeLabel(ui->mediaPanel, LV_SYMBOL_CLOSE, width - 22, 5, &lv_font_montserrat_12, muted);
    int volumeH = ui->hasVolume ? 52 : 0;
    if (height >= 240) {
      // Stacked: artwork centred, title and progress under it.
      int art = height >= 264 ? NP_ART_SIZE : 80;
      npBuildArt(ui, ui->mediaPanel, (width - art) / 2, 22, art);
      int y = 22 + art + 4;
      ui->title = npScrollingLabel(ui->mediaPanel, 12, y, width - 24, &lv_font_montserrat_16,
                                   textPrimary(), LV_TEXT_ALIGN_CENTER);
      ui->subtitle = makeWidgetLabel(ui->mediaPanel, "", 12, y + 20, &lv_font_montserrat_12, muted,
                                     width - 24, LV_TEXT_ALIGN_CENTER);
      y += 38;
      ui->trackWidth = (int16_t)(width - 32);
      ui->track = npTrack(ui->mediaPanel, 16, y, width - 32, 4, npAccent(), &ui->fill);
      ui->left = makeWidgetLabel(ui->mediaPanel, "", 16, y + 6, &lv_font_montserrat_10, muted, 60);
      ui->right = makeWidgetLabel(ui->mediaPanel, "", width - 76, y + 6, &lv_font_montserrat_10,
                                  muted, 60, LV_TEXT_ALIGN_RIGHT);
      int transportY = y + 18;
      int transportH = height - volumeH - transportY - 6;
      npBuildTransport(ui, ui->mediaPanel, width, transportY, constrain(transportH, 30, 38));
    } else {
      // Side by side: artwork left of the title, to fit four rows.
      int art = 56;
      npBuildArt(ui, ui->mediaPanel, 12, 22, art);
      ui->title = npScrollingLabel(ui->mediaPanel, 12 + art + 10, 26, width - art - 34,
                                   &lv_font_montserrat_16, textPrimary(), LV_TEXT_ALIGN_LEFT);
      ui->subtitle = makeWidgetLabel(ui->mediaPanel, "", 12 + art + 10, 48, &lv_font_montserrat_12,
                                     muted, width - art - 34);
      int y = 22 + art + 8;
      ui->trackWidth = (int16_t)(width - 32);
      ui->track = npTrack(ui->mediaPanel, 16, y, width - 32, 4, npAccent(), &ui->fill);
      ui->left = makeWidgetLabel(ui->mediaPanel, "", 16, y + 6, &lv_font_montserrat_10, muted, 60);
      ui->right = makeWidgetLabel(ui->mediaPanel, "", width - 76, y + 6, &lv_font_montserrat_10,
                                  muted, 60, LV_TEXT_ALIGN_RIGHT);
      int transportY = y + 18;
      npBuildTransport(ui, ui->mediaPanel, width, transportY,
                       constrain(height - volumeH - transportY - 6, 28, 36));
    }
    if (ui->hasVolume) npBuildVolumeRow(ui, ui->mediaPanel, width, height - volumeH);
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
    if (ui->expanded) makeLabel(ui->rxPanel, LV_SYMBOL_CLOSE, width - 22, 5, &lv_font_montserrat_12, muted);
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
                  (npSettings.allowSoundMode ? 0xA200u : 0) ^ ((uint32_t)npInputCount() << 20);
  if (npSettings.volume.keysDevice[0]) hash = npFnv(hash, npSettings.volume.keysDevice);
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

  uint8_t inputs = npInputCount();
  if (npSettings.allowInput && inputs) {
    heading("Input");
    for (uint8_t i = 0; i < inputs; i++) {
      lv_obj_t *key = npKey(ui->rxList, 0, 0, half, 26, npInputLabel(i),
                            &lv_font_montserrat_12, NP_ACT_SOURCE, i);
      if (i == 0) lv_obj_add_flag(key, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
      ui->rxSourceBtns[ui->rxSourceCount++] = key;
    }
  } else if (npVolume.source[0] || inputs) {
    ui->rxSourceText = heading("Input");
  }
  if (npSettings.allowSoundMode && npVolume.soundModeCount && npSettings.volume.type == NP_SRC_HA) {
    heading("Sound mode");
    for (uint8_t i = 0; i < npVolume.soundModeCount; i++) {
      lv_obj_t *key = npKey(ui->rxList, 0, 0, half, 26, npVolume.soundModes[i],
                            &lv_font_montserrat_12, NP_ACT_SOUND_MODE, i);
      if (i == 0) lv_obj_add_flag(key, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
      ui->rxSoundBtns[ui->rxSoundCount++] = key;
    }
  } else if (npVolume.soundMode[0]) {
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
  const void *source = show ? npMediaArtSource() : nullptr;
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

/* Input and sound mode (or the state), optionally after the receiver's name:
   "Den Denon - TV Audio - Dolby Atmos", "Muted - TV Audio". */
static void npVolumeDetail(char *out, size_t size, bool withName) {
  const NpEntityState &v = npVolume;
  const char *name = npHalfName(npSettings.volume, v, "Receiver");
  if (!v.valid) { snprintf(out, size, "%s%s", withName ? name : "", withName ? " - offline" : "Offline"); return; }
  if (npIsOff(v)) { snprintf(out, size, withName ? "%s is off" : "Off", name); return; }
  String text = withName ? String(name) : String();
  bool muted = v.muted && !v.guessed;
  if (muted && !withName) text = "Muted";
  if (v.source[0]) { if (text.length()) text += " - "; text += v.source; }
  if (muted && withName) { text += " - Muted"; }
  else if (!muted && v.soundMode[0]) { if (text.length()) text += " - "; text += v.soundMode; }
  strlcpy(out, text.c_str(), size);
}

/*
  The two lines of media text. The title is what is playing, else the app,
  else the player; the line under it never repeats the title:
    "The Diplomat" / "Netflix - S2 E4 - The Ghost"
    "Apple TV"     / "Roku - Playing"     (the app is all a Roku reports)
*/
static void npMediaLines(const NpEntityState &m, char *title, size_t titleSize, char *line,
                         size_t lineSize, bool withPlayer) {
  bool off = npIsOff(m);
  const char *name = npHalfName(npSettings.media, m, "Media player");
  const char *status = !m.valid ? "Not reachable"
    : off ? "Off" : npStateIs(m, "idle") ? "Idle"
    : npStateIs(m, "paused") ? "Paused"
    : npIsPlaying(m) ? "Playing" : "";
  if (!m.valid || off || (!m.title[0] && !m.app[0])) {
    strlcpy(title, name, titleSize);
    strlcpy(line, status, lineSize);
    return;
  }
  if (m.title[0]) {
    strlcpy(title, m.title, titleSize);
    const char *who = m.app[0] ? m.app : (withPlayer ? name : "");
    const char *what = m.subtitle[0] ? m.subtitle : (npStateIs(m, "paused") ? "Paused" : "");
    snprintf(line, lineSize, "%s%s%s", who, who[0] && what[0] ? " - " : "", what);
    return;
  }
  // Only the app is known.
  strlcpy(title, m.app, titleSize);
  snprintf(line, lineSize, "%s%s%s", strcmp(m.app, name) ? name : "",
           strcmp(m.app, name) && status[0] ? " - " : "", status);
}

void refreshNowPlayingUi(NowPlayingUi *ui) {
  if (!ui) return;
  // Events and polls recolour these labels long after the page was drawn.
  TextThemeScope textScope(textThemeFor(ui->mediaPanel ? ui->mediaPanel : ui->volValue));
  char text[128];
  const bool full = ui->layout == NP_LAYOUT_FULL;

  if (ui->hasMedia) {
    const NpEntityState &m = npMedia;
    bool off = npIsOff(m);
    bool playing = npIsPlaying(m);
    char title[96], line[96];
    npMediaLines(m, title, sizeof(title), line, sizeof(line), !full);
    setWidgetLabelText(ui->title, title);
    if (full) {
      // The full face names the player above the artwork, so the line under
      // the title leaves it out.
      const char *name = npHalfName(npSettings.media, m, "Media player");
      setWidgetLabelText(ui->tag, name);
    }
    setWidgetLabelText(ui->subtitle, line);

    bool timed = !off && m.hasPosition && m.duration > 0;
    bool art = npSettings.artwork && !off && m.valid && npMediaArtSource() != nullptr;
    npReflow(ui, art, timed);
    npSetHidden(ui->track, !timed);
    npSetHidden(ui->left, !timed);
    npSetHidden(ui->right, !timed);
    npSetHidden(ui->stateGlyph, !timed);
    if (timed) {
      uint32_t position = npMediaPosition();
      npSetFill(ui->fill, ui->trackWidth, (uint32_t)((uint64_t)position * 1000ULL / m.duration));
      formatWidgetClock(text, sizeof(text), position, false);
      setWidgetLabelText(ui->left, text);
      if (full) formatWidgetClock(text, sizeof(text), m.duration, false);
      else formatWidgetClock(text, sizeof(text), m.duration - position, true);
      setWidgetLabelText(ui->right, text);
      setWidgetLabelText(ui->stateGlyph, playing ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE);
    }
    // Two and three rows show artwork only when there is a picture; the
    // others keep a placeholder in its frame.
    if (ui->layout == NP_LAYOUT_TWO || ui->layout == NP_LAYOUT_THREE) npRefreshArt(ui, art);
    else npRefreshArt(ui, npSettings.artwork && !off && m.valid);

    if (full) {
      bool canTurnOn = off && m.valid && npHasKey(npSettings.media, NPK_POWER_ON) &&
                       (npSettings.media.type != NP_SRC_HA || (m.supported & NP_FEATURE_TURN_ON));
      npSetHidden(ui->transport, off);
      npSetHidden(ui->mediaPowerBtn, !canTurnOn);
      if (ui->playLabel) setWidgetLabelText(ui->playLabel, playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
      bool seek = npSettings.media.type == NP_SRC_HA && (m.supported & NP_FEATURE_SEEK) &&
                  m.hasPosition && !npSettings.media.keys[NPK_NEXT][0];
      if (ui->backLabel) setWidgetLabelText(ui->backLabel, seek ? "-10s" : LV_SYMBOL_PREV);
      if (ui->forwardLabel) setWidgetLabelText(ui->forwardLabel, seek ? "+10s" : LV_SYMBOL_NEXT);
    }
  }

  if (ui->hasVolume) {
    const NpEntityState &v = npVolume;
    bool off = npIsOff(v);
    bool muted = v.muted && !off && !v.guessed;
    char level[24];
    npFormatVolume(level, sizeof(level), v);
    const char *value = !v.valid ? "--" : off ? "Off" : v.hasVolume ? level : "";
    if (ui->volValue) {
      setWidgetLabelText(ui->volValue, value);
      // Muted dims the level rather than striking it through.
      lv_obj_set_style_text_color(ui->volValue, muted || off ? widgetMutedColour()
                                                             : textPrimary(), 0);
    }
    if (ui->volIcon) {
      setWidgetLabelText(ui->volIcon, off ? LV_SYMBOL_POWER
                                          : muted ? LV_SYMBOL_MUTE : LV_SYMBOL_VOLUME_MAX);
      lv_obj_set_style_text_color(ui->volIcon, off ? widgetMutedColour()
                                  : muted ? npMutedRed() : npVolumeColour(), 0);
    }
    if (ui->volDetail) {
      // Name on one line, input and sound mode (or "Muted") under it.
      setWidgetLabelText(ui->volLabel, npHalfName(npSettings.volume, v, "Receiver"));
      npVolumeDetail(text, sizeof(text), false);
      setWidgetLabelText(ui->volDetail, text);
      lv_obj_set_style_text_color(ui->volDetail, muted ? npMutedRed() : widgetMutedColour(), 0);
    } else if (ui->volLabel) {
      npVolumeDetail(text, sizeof(text), true);
      setWidgetLabelText(ui->volLabel, text);
      lv_obj_set_style_text_color(ui->volLabel, muted ? npMutedRed() : widgetMutedColour(), 0);
    }

    if (full) {
      bool canTurnOn = off && v.valid && npHasKey(npSettings.volume, NPK_POWER_ON);
      npSetHidden(ui->volKeys, off);
      npSetHidden(ui->volPowerBtn, !canTurnOn || !ui->hasMedia);
      if (ui->muteLabel) setWidgetLabelText(ui->muteLabel, muted ? LV_SYMBOL_VOLUME_MAX : LV_SYMBOL_MUTE);

      if (ui->rxPanel) {
        setWidgetLabelText(ui->rxName, npHalfName(npSettings.volume, v, "Receiver"));
        setWidgetLabelText(ui->rxValue, value[0] ? value : "Volume");
        lv_obj_set_style_text_color(ui->rxValue, muted ? npMutedRed() : textPrimary(), 0);
        npSetFill(ui->rxFill, ui->rxTrackWidth,
                  v.hasVolume && !off ? (uint32_t)(constrain(v.volume, 0.0f, 1.0f) * 1000.0f) : 0);
        npSetHidden(ui->rxKeys, off);
        npSetHidden(ui->rxPowerBtn, !canTurnOn);
        if (ui->rxMuteLabel) setWidgetLabelText(ui->rxMuteLabel,
          muted ? LV_SYMBOL_VOLUME_MAX " Unmute" : LV_SYMBOL_MUTE " Mute");
        npRebuildReceiverLists(ui);
        for (uint8_t i = 0; i < ui->rxSourceCount; i++) {
          npSetKeySelected(ui->rxSourceBtns[i], v.source[0] && strcasecmp(npInputLabel(i), v.source) == 0);
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

static void npShowReceiverPanelOn(WidgetInstance &instance, bool show);

void npShowReceiverPanel(bool show) {
  // The open widget if there is one, else any full-size one on the page.
  bool overlay = nowPlayingExpandedOpen();
  for (uint8_t i = 0; overlay && i < widgetExpandedInstanceCount; i++) {
    npShowReceiverPanelOn(widgetExpandedInstances[i], show);
  }
  for (uint8_t i = 0; !overlay && i < widgetInstanceCount; i++) {
    npShowReceiverPanelOn(widgetInstances[i], show);
  }
}

static void npShowReceiverPanelOn(WidgetInstance &instance, bool show) {
  if (instance.kind != WIDGET_NOWPLAYING || !instance.nowPlayingUi) return;
  if (!instance.card || !lv_obj_is_valid(instance.card)) return;
  NowPlayingUi *ui = instance.nowPlayingUi;
  if (!ui->rxPanel || !ui->mediaPanel) return;
  ui->rxShown = show;
  npSetHidden(ui->rxPanel, !show);
  npSetHidden(ui->mediaPanel, show);
  if (show) npRequestRefresh(false, true);
  refreshNowPlayingUi(ui);
}

/* ------------------------------------------------------------- pop-up */

char npPopupAction[24] = "";

static void npRefreshPopup() {
  if (!npPopup || !lv_obj_is_valid(npPopup)) return;
  const NpEntityState &v = npVolume;
  char level[24];
  npFormatVolume(level, sizeof(level), v);
  bool off = npIsOff(v);
  bool muted = v.muted && !off && !v.guessed;
  char name[80];
  snprintf(name, sizeof(name), "%s%s", npHalfName(npSettings.volume, v, "Receiver"),
           muted ? "  -  Muted" : "");
  setWidgetLabelText(npPopupName, name);
  // A receiver that cannot report a level still confirms the key.
  const char *value = !v.valid ? "--" : off && !v.guessed ? "Off"
                    : v.hasVolume ? level : npPopupAction;
  setWidgetLabelText(npPopupValue, value);
  lv_color_t accent = muted ? npMutedRed() : npVolumeColour();
  lv_obj_set_style_text_color(npPopupName, muted ? npMutedRed() : widgetMutedColour(), 0);
  lv_obj_set_style_text_color(npPopupValue, muted ? npMutedRed() : textPrimary(), 0);
  lv_obj_set_style_bg_color(npPopupFill, accent, 0);
  lv_obj_set_style_border_color(npPopup, muted ? lvRgb(92, 58, 58) : lvRgb(58, 74, 92), 0);
  lv_obj_t *track = lv_obj_get_parent(npPopupFill);
  npSetHidden(track, !v.hasVolume);
  npSetFill(npPopupFill, NP_POPUP_TRACK,
            v.hasVolume && !off ? (uint32_t)(constrain(v.volume, 0.0f, 1.0f) * 1000.0f) : 0);
}

void npHideVolumePopup() {
  if (npPopup && lv_obj_is_valid(npPopup)) lv_obj_del(npPopup);
  npPopup = nullptr;
  npPopupName = npPopupValue = npPopupFill = nullptr;
  npPopupUntilMs = 0;
}

bool nowPlayingFullOnScreen();

void npShowVolumePopup(const char *action) {
  if (!npSettings.volumePopup || !npHalfActive(npSettings.volume)) {
    Serial.printf("Now Playing: no pop-up for %s (%s)\n", action ? action : "volume",
                  !npSettings.volumePopup ? "switched off" : "no volume device set");
    return;
  }
  // The open widget, or a full-size one on the page, already shows the level.
  if (nowPlayingFullOnScreen()) {
    Serial.printf("Now Playing: no pop-up for %s (the full widget is on screen)\n",
                  action ? action : "volume");
    return;
  }
  if (!npPopup) Serial.printf("Now Playing: pop-up for %s\n", action ? action : "volume");
  strlcpy(npPopupAction, action ? action : "Volume", sizeof(npPopupAction));
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

bool nowPlayingFullOnScreen() {
  if (nowPlayingExpandedOpen()) return true;
  for (uint8_t i = 0; i < widgetInstanceCount; i++) {
    const WidgetInstance &instance = widgetInstances[i];
    if (instance.kind == WIDGET_NOWPLAYING && instance.nowPlayingUi && instance.card &&
        lv_obj_is_valid(instance.card) && instance.nowPlayingUi->layout == NP_LAYOUT_FULL &&
        instance.nowPlayingUi->hasVolume) return true;
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
  npServiceLocalSources(now);
  serviceNowPlayingArtwork(now);
  if (npDirty) {
    npDirty = false;
    npRefreshAll();
  }
}

static uint8_t npSourceFromName(const char *name) {
  if (!name) return NP_SRC_NONE;
  if (!strcmp(name, "homeassistant")) return NP_SRC_HA;
  if (!strcmp(name, "mqtt")) return NP_SRC_MQTT;
  if (!strcmp(name, "homebridge")) return NP_SRC_HOMEBRIDGE;
  if (!strcmp(name, "chromecast")) return NP_SRC_CAST;
  return NP_SRC_NONE;
}

static const char *npSourceName(uint8_t type) {
  switch (type) {
    case NP_SRC_HA: return "Home Assistant";
    case NP_SRC_MQTT: return "MQTT";
    case NP_SRC_HOMEBRIDGE: return "Homebridge";
    case NP_SRC_CAST: return "Chromecast";
    default: return "none";
  }
}

/*
  One half: {device, source, entity, accessory, name,
             mqtt:{jsonTopic, fields:{state,...}, volumeScale},
             keys:{device, volumeUp, ..., inputs:[...]}}
  WebConfig works the source out from the device chosen and writes it here,
  so the remote never has to guess what kind of device it is.
*/
static void npLoadHalf(NpSourceSettings &half, JsonObjectConst raw, const char *legacyEntity) {
  memset(&half, 0, sizeof(half));
  if (raw.isNull()) {
    half.type = legacyEntity && legacyEntity[0] ? NP_SRC_HA : NP_SRC_NONE;
    strlcpy(half.entity, legacyEntity ? legacyEntity : "", sizeof(half.entity));
    return;
  }
  half.type = npSourceFromName(raw["source"] | "none");
  strlcpy(half.entity, raw["entity"] | "", sizeof(half.entity));
  strlcpy(half.accessory, raw["accessory"] | "", sizeof(half.accessory));
  copyDisplayTextAscii(raw["name"] | "", half.name, sizeof(half.name));
  JsonObjectConst mqtt = raw["mqtt"].as<JsonObjectConst>();
  strlcpy(half.mqttJsonTopic, mqtt["jsonTopic"] | "", sizeof(half.mqttJsonTopic));
  JsonObjectConst fields = mqtt["fields"].as<JsonObjectConst>();
  for (uint8_t f = 0; f < NPF_COUNT; f++) {
    strlcpy(half.mqttFields[f], fields[NP_MQTT_FIELD_KEYS[f]] | "", sizeof(half.mqttFields[f]));
  }
  const char *scale = mqtt["volumeScale"] | "unit";
  half.volumeScale = !strcmp(scale, "percent") ? NP_SCALE_PERCENT
                   : !strcmp(scale, "db") ? NP_SCALE_DB : NP_SCALE_UNIT;
  JsonObjectConst keys = raw["keys"].as<JsonObjectConst>();
  strlcpy(half.keysDevice, keys["device"] | "", sizeof(half.keysDevice));
  for (uint8_t k = 0; k < NPK_COUNT; k++) {
    strlcpy(half.keys[k], keys[NP_KEY_NAMES[k]] | "", sizeof(half.keys[k]));
  }
  for (JsonVariantConst input : keys["inputs"].as<JsonArrayConst>()) {
    if (half.inputCount >= NP_MAX_LIST) break;
    const char *id = input | "";
    if (id[0]) strlcpy(half.inputs[half.inputCount++], id, sizeof(half.inputs[0]));
  }
}

static bool npHalfSameSource(const NpSourceSettings &a, const NpSourceSettings &b) {
  return a.type == b.type && !strcmp(a.entity, b.entity) && !strcmp(a.accessory, b.accessory) &&
         !strcmp(a.mqttJsonTopic, b.mqttJsonTopic) && !strcmp(a.keysDevice, b.keysDevice);
}

/* Reads widgets.nowPlaying from the config. */
void loadNowPlayingSettings(JsonObjectConst widgets) {
  JsonObjectConst settings = widgets["nowPlaying"].as<JsonObjectConst>();
  // Built in PSRAM: a whole second copy of the settings has no business in
  // internal RAM for the moment it takes to compare them.
  NowPlayingSettings *built = (NowPlayingSettings *)(psramFound()
    ? ps_malloc(sizeof(NowPlayingSettings)) : malloc(sizeof(NowPlayingSettings)));
  if (!built) return;
  NowPlayingSettings &next = *built;
  npDefaultSettings(next);
  if (!settings.isNull()) {
    // Builds before source types wrote two bare entity ids.
    npLoadHalf(next.media, settings["media"].as<JsonObjectConst>(), settings["mediaEntity"] | "");
    npLoadHalf(next.volume, settings["volume"].as<JsonObjectConst>(), settings["volumeEntity"] | "");
    next.allowInput = settings["allowInput"] | true;
    next.allowSoundMode = settings["allowSoundMode"] | true;
    next.volumeDb = strcmp(settings["volumeUnit"] | "db", "percent") != 0;
    next.dbOffset = constrain((float)(settings["dbOffset"] | -80.0f), -120.0f, 20.0f);
    next.volumePopup = settings["volumePopup"] | true;
    next.artwork = settings["artwork"] | true;
  } else {
    next.media.type = NP_SRC_NONE;
    next.volume.type = NP_SRC_NONE;
  }
  if (!npHalfSameSource(next.media, npSettings.media)) {
    memset(&npMedia, 0, sizeof(npMedia));
    npArtRelease();
  }
  if (!npHalfSameSource(next.volume, npSettings.volume)) memset(&npVolume, 0, sizeof(npVolume));
  bool mqttChanged = next.media.type == NP_SRC_MQTT || next.volume.type == NP_SRC_MQTT ||
                     npSettings.media.type == NP_SRC_MQTT || npSettings.volume.type == NP_SRC_MQTT;
  npSettings = next;
  free(built);
  npPollNextMs = 0;
  if (mqttChanged) nowPlayingMqttTopicsChanged();
  Serial.printf("Widgets: now playing media=%s %s, volume=%s %s (keys %s/%s, %s)\n",
                npSourceName(npSettings.media.type),
                npSettings.media.entity[0] ? npSettings.media.entity : npSettings.media.accessory,
                npSourceName(npSettings.volume.type),
                npSettings.volume.entity[0] ? npSettings.volume.entity : npSettings.volume.accessory,
                npSettings.media.keysDevice[0] ? npSettings.media.keysDevice : "-",
                npSettings.volume.keysDevice[0] ? npSettings.volume.keysDevice : "-",
                npSettings.volumeDb ? "dB" : "%");
}
