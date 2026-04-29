#pragma once

// ---------- Firmware-Version ----------
#define FW_MAJOR       1
#define FW_MINOR       5
#define FW_PATCH       0
#define FW_VERSION_STR "1.5.0"

// ---------- Ethernet ----------
// Board: Olimex ESP32-POE-ISO – PHY: LAN8710A (LAN8720-kompatibel)
// ESP32 gibt 50-MHz-Referenztakt auf GPIO17 aus (intern, nicht herausgeführt)
#define ETH_ADDR        0
#define ETH_POWER_PIN   12
#define ETH_MDC_PIN     23
#define ETH_MDIO_PIN    18
#define ETH_TYPE        ETH_PHY_LAN8720
#define ETH_CLK_MODE    ETH_CLOCK_GPIO17_OUT

// ---------- ST7920 (seriell, 3-Leiter) über 74HCT245 ----------
static const uint8_t LCD_CS  = 16; // RS  -> über 74HCT245 zu ST7920 Pin 4
static const uint8_t LCD_CLK = 14; // E   -> über 74HCT245 zu ST7920 Pin 6
static const uint8_t LCD_SID = 13; // R/W -> über 74HCT245 zu ST7920 Pin 5
static const uint8_t LCD_RST = 4;  // RST -> über 74HCT245 zu ST7920 Pin 17

// Display-Buffer-Größe (128×64 = 8192 Pixel / 8 Bits = 1024 Byte)
static const size_t DISPLAY_BUF_SIZE = 1024;

// ---------- Helligkeit (VEML7700 + PWM Backlight) ----------
static const int I2C_SDA = 32;
static const int I2C_SCL = 33;

static const int BL_PIN  = 5; // Gate MOSFET

// Grenzen fürs Display (0..255)
static const uint8_t BL_MIN = 10;    // nie ganz aus
static const uint8_t BL_MAX = 255;   // volle Helligkeit

// Mapping Lux -> PWM
static const float LUX_MIN = 0.2f;     // darunter "Nacht"
static const float LUX_MAX = 235.0f;   // darüber "Tag"
static const float GAMMA_C = 0.65f;    // Wahrnehmungskorrektur

// Mess-/Regelparameter
static const unsigned long LUX_EVERY_MS  = 100; // passend zu IT_50MS
static const float LUX_HYST             = 1.2f; // Deadband in Lux
static const float LUX_ALPHA_UP         = 0.55f;
static const float LUX_ALPHA_DOWN       = 0.12f;

// Zielwert wird nur ~1 Hz aktualisiert, Ausgabe fährt weich per Rampe
static const unsigned long PWM_RAMP_MS = 20;   // 10..30 ms
static const uint8_t PWM_RAMP_UP       = 6;    // pro Ramp-Tick
static const uint8_t PWM_RAMP_DOWN     = 8;    // pro Ramp-Tick

// ---------- NTP ----------
static const unsigned long QUERY_EVERY_MS    = 10UL * 1000UL;
static const uint32_t      NTP_UNIX_EPOCH_DIFF = 2208988800UL;
#define STAB_SAMPLES 16

// ---------- Enumerationen ----------
enum UiState   { UI_BOOT, UI_SHOW_IP, UI_RUN };
enum SyncState { SYNC_OFFLINE, SYNC_NEVER, SYNC_SYNCING, SYNC_OK, SYNC_STALE };
enum AnimMode  { ANIM_NONE = 0, ANIM_SCROLL = 1, ANIM_CROSSFADE = 2 };
enum DstMode   { DST_OFF = 0, DST_EU = 1, DST_US = 2, DST_CUSTOM = 3 };

// ---------- Globale Objekte ----------
U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, LCD_CLK, LCD_SID, LCD_CS, LCD_RST);
WebServer    server(80);
Preferences  prefs;
Adafruit_VEML7700 veml;

// ---------- Globale Variablen ----------
volatile bool   eth_got_ip  = false;
volatile time_t last_sync   = 0;
volatile bool   have_synced = false;

volatile uint8_t g_stratum = 0;
volatile float   g_rtt_ms  = NAN;  // float = 32-bit, atomar auf ESP32
char g_last_ntp_status[80] = "noch keine NTP-Abfrage";

bool veml_ok  = false;
bool ota_ready = false;
char g_last_reset_reason[48] = "unbekannt";
char g_eth_force_status[80] = "noch nicht gesetzt";

UiState       ui_state   = UI_BOOT;
unsigned long ui_since   = 0;
AnimMode      g_anim_mode = ANIM_NONE;

char     ntp1[64], ntp2[64], ntp3[64];
uint32_t ntp_interval_ms = 60 * 1000UL;

// ---------- Zeitzonen-Konfiguration ----------
int    g_tz_offset    = 60;      // UTC-Offset in Minuten (default +1 Stunde)
DstMode g_tz_dst_mode = DST_EU;  // Sommerzeit-Modus
char   g_tz_dst_start[16]  = "M3.5.0/2";   // EU: letzter Sonntag März, 2:00
char   g_tz_dst_end[16]    = "M10.5.0/3";  // EU: letzter Sonntag Oktober, 3:00
int    g_tz_dst_minutes    = 60;            // Sommerzeit-Versatz in Minuten

// ---------- Netzwerk-Konfiguration ----------
bool g_use_dhcp     = true;
char g_static_ip[16]  = "192.168.1.100";
char g_static_gw[16]  = "192.168.1.1";
char g_static_sn[16]  = "255.255.255.0";
char g_static_dns[16] = "8.8.8.8";

double rtt_hist[STAB_SAMPLES];
int    rtt_count = 0, rtt_index = 0;
static portMUX_TYPE rtt_spinlock = portMUX_INITIALIZER_UNLOCKED;

static unsigned long last_lux_ms       = 0;
static unsigned long bl_warmup_until_ms = 0;
static unsigned long last_ramp_ms      = 0;

static float   lux_filt          = NAN;
static float   lux_hold          = NAN;
static uint8_t pwm_current       = BL_MAX;
static uint8_t pwm_target_global = BL_MAX;

static TaskHandle_t ntp_task_handle = nullptr;

template <size_t N>
static inline void copyTrunc(char (&dst)[N], const char *src) {
  if (!src) {
    dst[0] = '\0';
    return;
  }
  snprintf(dst, N, "%s", src);
}

static const char *resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXT";
    case ESP_RST_SW:        return "SW";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "UNKNOWN";
  }
}

static void captureResetReason() {
  esp_reset_reason_t reason = esp_reset_reason();
  snprintf(g_last_reset_reason, sizeof(g_last_reset_reason), "%s (%d)",
           resetReasonName(reason), (int)reason);
}

static void setNtpStatus(const char *status) {
  copyTrunc(g_last_ntp_status, status);
}

static bool forceEth100FullDuplex(esp_eth_handle_t handle, const char *phase) {
  if (!handle) {
    snprintf(g_eth_force_status, sizeof(g_eth_force_status),
             "%s: kein Ethernet-Handle", phase ? phase : "ETH");
    return false;
  }

  esp_eth_mediator_t *med = (esp_eth_mediator_t*)handle;
  if (!med || !med->phy_reg_write) {
    snprintf(g_eth_force_status, sizeof(g_eth_force_status),
             "%s: kein MDIO-Zugriff", phase ? phase : "ETH");
    return false;
  }

  esp_err_t err = med->phy_reg_write(med, ETH_ADDR, 0 /*BMCR*/, 0x2100);
  snprintf(g_eth_force_status, sizeof(g_eth_force_status),
           "%s: BMCR=0x2100 %s", phase ? phase : "ETH", err == ESP_OK ? "OK" : "FEHLER");
  return err == ESP_OK;
}

// ---------- NTP-Paket ----------
#pragma pack(push,1)
struct NtpPacket {
  uint8_t  li_vn_mode, stratum, poll; int8_t precision;
  uint32_t rootDelay, rootDispersion, refId;
  uint32_t refTsSec, refTsFrac, origTsSec, origTsFrac,
           recvTsSec, recvTsFrac, txTsSec, txTsFrac;
};
#pragma pack(pop)

static inline uint32_t bswap32(uint32_t v) { return __builtin_bswap32(v); }

// ---------- SNTP-Callback ----------
static void sntp_time_sync_cb(struct timeval *) {
  last_sync   = time(nullptr);
  have_synced = true;
}

// ---------- NTP-Konfiguration ----------
void normalizeNTP() {
  auto trim_inplace = [](char *s) {
    while (*s && isspace((unsigned char)*s)) memmove(s, s+1, strlen(s));
    int n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n-1])) s[--n] = '\0';
  };
  trim_inplace(ntp1); trim_inplace(ntp2); trim_inplace(ntp3);
  if (ntp1[0] == '\0') copyTrunc(ntp1, "0.de.pool.ntp.org");
  if (ntp2[0] == '\0') copyTrunc(ntp2, ntp1);
  if (ntp3[0] == '\0') copyTrunc(ntp3, ntp1);
  if (ntp_interval_ms < 1000UL) ntp_interval_ms = 60 * 1000UL;
}

void applySNTP() {
  normalizeNTP();
  if (esp_sntp_enabled()) esp_sntp_stop();
  esp_sntp_setoperatingmode((esp_sntp_operatingmode_t)SNTP_OPMODE_POLL);
  esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
  esp_sntp_set_time_sync_notification_cb(sntp_time_sync_cb);
  esp_sntp_setservername(0, ntp1);
  esp_sntp_setservername(1, ntp2);
  esp_sntp_setservername(2, ntp3);
  esp_sntp_set_sync_interval(ntp_interval_ms);
  have_synced = false; last_sync = 0;
  esp_sntp_init();
}

SyncState currentSyncState() {
  if (!eth_got_ip)          return SYNC_OFFLINE;
  if (!esp_sntp_enabled())  return SYNC_NEVER;
  if (!have_synced)         return SYNC_SYNCING;
  time_t now = time(nullptr);
  if (now == 0)             return SYNC_SYNCING;
  uint32_t max_age_s = (ntp_interval_ms / 1000UL) * 3UL + 30UL;
  time_t   age       = (now > last_sync) ? (now - last_sync) : 0;
  return (age <= (time_t)max_age_s) ? SYNC_OK : SYNC_STALE;
}

// ---------- NTP-Abfrage (UDP, manuell) ----------
bool resolveHost(const char *host, IPAddress &out) {
  IPAddress ip;
  if (ip.fromString(host))      { out = ip; return true; }
  if (WiFi.hostByName(host, ip)) { out = ip; return true; }
  return false;
}

bool ntpQueryOnce(const char *host, uint8_t &outStratum, double &outRTTms) {
  char status[80];
  snprintf(status, sizeof(status), "frage %s ab", host);
  setNtpStatus(status);

  IPAddress ip;
  if (!resolveHost(host, ip)) {
    snprintf(status, sizeof(status), "DNS fehlgeschlagen via WiFi.hostByName: %s", host);
    setNtpStatus(status);
    return false;
  }

  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    setNtpStatus("Socket konnte nicht geoeffnet werden");
    return false;
  }
  sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(123);
  addr.sin_addr.s_addr = (uint32_t)ip;
  timeval tv{1, 200000}; setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  NtpPacket pkt{}; pkt.li_vn_mode = 0b00100011;
  timeval tv1; gettimeofday(&tv1, nullptr);
  uint32_t t1s = (uint32_t)tv1.tv_sec + NTP_UNIX_EPOCH_DIFF;
  uint32_t t1f = (uint32_t)((uint64_t)tv1.tv_usec * 4294.967296);
  pkt.txTsSec = bswap32(t1s); pkt.txTsFrac = bswap32(t1f);
  uint32_t t_send_us = micros();
  if (sendto(sock, &pkt, sizeof(pkt), 0, (sockaddr*)&addr, sizeof(addr)) != (int)sizeof(pkt)) {
    setNtpStatus("Senden an NTP-Server fehlgeschlagen");
    close(sock); return false;
  }
  NtpPacket resp{}; sockaddr_in from{}; socklen_t fl = sizeof(from);
  int recvd = recvfrom(sock, &resp, sizeof(resp), 0, (sockaddr*)&from, &fl);
  uint32_t t_recv_us = micros(); close(sock);
  if (recvd < (int)sizeof(resp)) {
    setNtpStatus("Keine NTP-Antwort empfangen");
    return false;
  }
  if (from.sin_addr.s_addr != addr.sin_addr.s_addr) {
    setNtpStatus("NTP-Antwort von unerwarteter IP");
    return false;
  }
  if (bswap32(resp.origTsSec) != t1s) {
    setNtpStatus("NTP-Origin-Timestamp passt nicht");
    return false;
  }
  outStratum = resp.stratum;
  outRTTms   = (double)(t_recv_us - t_send_us) / 1000.0;
  snprintf(status, sizeof(status), "OK: %s, RTT %.1f ms, Stratum %u",
           ip.toString().c_str(), outRTTms, (unsigned)outStratum);
  setNtpStatus(status);
  return true;
}

void addRttSample(double rtt) {
  if (isnan(rtt)) return;
  portENTER_CRITICAL(&rtt_spinlock);
  if (rtt_count < STAB_SAMPLES) rtt_hist[rtt_count++] = rtt;
  else { rtt_hist[rtt_index] = rtt; rtt_index = (rtt_index + 1) % STAB_SAMPLES; }
  portEXIT_CRITICAL(&rtt_spinlock);
}

bool rttStats(double &mean, double &sd) {
  double buf[STAB_SAMPLES];
  int    count;
  portENTER_CRITICAL(&rtt_spinlock);
  count = rtt_count;
  memcpy(buf, rtt_hist, count * sizeof(double));
  portEXIT_CRITICAL(&rtt_spinlock);

  if (count == 0) return false;
  double sum = 0; for (int i = 0; i < count; i++) sum += buf[i];
  mean = sum / count;
  if (count == 1) { sd = NAN; return true; }
  double v = 0; for (int i = 0; i < count; i++) { double d = buf[i]-mean; v += d*d; }
  sd = sqrt(v / (count - 1)); return true;
}

int stabilityPercent() {
  double m = 0, sd = NAN;
  if (!rttStats(m, sd) || m <= 0 || isnan(sd)) return -1;
  // Absoluter Jitter: 0 ms → 100%, ab 5 ms → 0%
  // CV (sd/m) wäre irreführend: lokale Server hätten trotz besserem absoluten
  // Jitter schlechtere Werte als entfernte Server mit hoher mittlerer RTT.
  const double BAD_JITTER_MS = 5.0;
  double v = 100.0 * (1.0 - sd / BAD_JITTER_MS);
  if (v < 0) v = 0; if (v > 100) v = 100;
  return (int)lround(v);
}

// ---------- Backlight-Regelung ----------
static uint8_t clampU8(int v) {
  if (v < 0)   return 0;
  if (v > 255) return 255;
  return (uint8_t)v;
}

static void setBacklight(uint8_t pwm) {
  pwm_current = pwm;
  analogWrite(BL_PIN, pwm);
}

static uint8_t luxToPwm(float lux) {
  if (!isfinite(lux)) return BL_MAX;
  if (lux < LUX_MIN) lux = LUX_MIN;
  if (lux > LUX_MAX) lux = LUX_MAX;
  float x   = (lux - LUX_MIN) / (LUX_MAX - LUX_MIN);
  float y   = powf(x, GAMMA_C);
  int   pwm = (int)lroundf((float)BL_MIN + y * (float)(BL_MAX - BL_MIN));
  return clampU8(pwm);
}

static void updateBacklightFromLux() {
  if (!veml_ok) return;
  unsigned long now = millis();

  if ((long)(now - bl_warmup_until_ms) < 0) {
    pwm_target_global = BL_MAX;
    if (pwm_current != BL_MAX) setBacklight(BL_MAX);
    return;
  }

  if (now - last_lux_ms < LUX_EVERY_MS) return;
  last_lux_ms = now;

  float lux = veml.readLux();
  if (!isfinite(lux) || lux < 0.0f) return;

  if (!isfinite(lux_filt)) lux_filt = lux;
  else {
    float a = (lux > lux_filt) ? LUX_ALPHA_UP : LUX_ALPHA_DOWN;
    lux_filt = (1.0f - a) * lux_filt + a * lux;
  }

  const float FAST_DELTA = 8.0f;
  if (!isfinite(lux_hold)) {
    lux_hold = lux_filt;
  } else {
    float d = lux_filt - lux_hold;
    if (fabsf(d) >= FAST_DELTA) {
      lux_hold = lux_filt;
    } else {
      if (lux_filt > lux_hold + LUX_HYST)      lux_hold = lux_filt;
      else if (lux_filt < lux_hold - LUX_HYST) lux_hold = lux_filt;
    }
  }

  pwm_target_global = luxToPwm(lux_hold);
}

static void rampBacklight() {
  unsigned long now = millis();
  if (now - last_ramp_ms < PWM_RAMP_MS) return;
  last_ramp_ms = now;

  int diff = (int)pwm_target_global - (int)pwm_current;
  if (diff == 0) return;

  if (diff > 0) {
    int next = (int)pwm_current + (int)PWM_RAMP_UP;
    if (next > (int)pwm_target_global) next = pwm_target_global;
    setBacklight((uint8_t)next);
  } else {
    int next = (int)pwm_current - (int)PWM_RAMP_DOWN;
    if (next < (int)pwm_target_global) next = pwm_target_global;
    setBacklight((uint8_t)next);
  }
}

// ---------- Zeit-Formatierung ----------
void buildTZString(char *buf, size_t bufsz) {
  int posix_off = -(g_tz_offset / 60);
  int extra_min = abs(g_tz_offset) % 60;

  if (extra_min == 0) {
    if (posix_off < 0) snprintf(buf, bufsz, "STD%d", posix_off);
    else               snprintf(buf, bufsz, "STD%d", posix_off);
  } else {
    if (posix_off < 0) snprintf(buf, bufsz, "STD%d:%02d", posix_off, extra_min);
    else               snprintf(buf, bufsz, "STD%d:%02d", posix_off, extra_min);
  }

  if (g_tz_dst_mode == DST_OFF) return;

  const char *start_rule, *end_rule;
  switch (g_tz_dst_mode) {
    case DST_EU:  start_rule = "M3.5.0/2";  end_rule = "M10.5.0/3"; break;
    case DST_US:  start_rule = "M3.2.0/2";  end_rule = "M11.1.0/2"; break;
    case DST_CUSTOM: start_rule = g_tz_dst_start; end_rule = g_tz_dst_end; break;
    default: return;
  }

  char suffix[64];
  if (g_tz_dst_minutes == 60) {
    snprintf(suffix, sizeof(suffix), "DST,%s,%s", start_rule, end_rule);
  } else {
    int dst_h = g_tz_dst_minutes / 60;
    int dst_m = g_tz_dst_minutes % 60;
    if (dst_m == 0)
      snprintf(suffix, sizeof(suffix), "DST%d,%s,%s", dst_h, start_rule, end_rule);
    else
      snprintf(suffix, sizeof(suffix), "DST%d:%02d,%s,%s", dst_h, dst_m, start_rule, end_rule);
  }
  strncat(buf, suffix, bufsz - strlen(buf) - 1);
}
String fmtLocal(time_t t) {
  if (t == 0) return "–";
  tm ltm; localtime_r(&t, &ltm);
  char buf[32];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
    ltm.tm_year+1900, ltm.tm_mon+1, ltm.tm_mday,
    ltm.tm_hour, ltm.tm_min, ltm.tm_sec);
  return String(buf);
}

static String fmtUptime(unsigned long s) {
  unsigned long d = s/86400, h = (s%86400)/3600, m = (s%3600)/60, sc = s%60;
  char buf[24];
  if (d > 0) snprintf(buf, sizeof(buf), "%lud %02lu:%02lu:%02lu", d, h, m, sc);
  else       snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", h, m, sc);
  return String(buf);
}
