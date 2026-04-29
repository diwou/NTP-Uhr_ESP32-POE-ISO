#pragma once
// Setzt voraus: config.h wurde bereits eingebunden (u8g2, SyncState,
// g_stratum, g_rtt_ms, currentSyncState, stabilityPercent).

// ---------- Lookup-Tabellen ----------
static const char* const wday[7] = {
  "Sonntag","Montag","Dienstag","Mittwoch","Donnerstag","Freitag","Samstag"
};
static const char* const monate[12] = {
  "Jan.", "Feb.", "März", "Apr.", "Mai", "Jun.",
  "Jul.", "Aug.", "Sept.", "Okt.", "Nov.", "Dez."
};

// ---------- Hilfsfunktionen ----------
int isoWeek(const tm &t_in) {
  tm t = t_in;
  t.tm_hour = 12;   // Stabil gegen DST
  t.tm_min  = 0;
  t.tm_sec  = 0;
  mktime(&t);

  int iso_wday = t.tm_wday == 0 ? 7 : t.tm_wday;

  tm thu = t;
  thu.tm_mday += (4 - iso_wday);
  mktime(&thu);

  int iso_year = thu.tm_year + 1900;

  tm jan4 = {};
  jan4.tm_year = iso_year - 1900;
  jan4.tm_mon  = 0;
  jan4.tm_mday = 4;
  jan4.tm_hour = 12;
  mktime(&jan4);

  int jan4_wday = jan4.tm_wday == 0 ? 7 : jan4.tm_wday;

  tm week1_monday = jan4;
  week1_monday.tm_mday -= (jan4_wday - 1);
  mktime(&week1_monday);

  int days = (int)((mktime(&thu) - mktime(&week1_monday)) / 86400);
  return 1 + days / 7;
}

const char* syncText(SyncState st) {
  switch (st) {
    case SYNC_OK:      return "OK";
    case SYNC_SYNCING: return "SYNC";
    case SYNC_STALE:   return "STALE";
    case SYNC_OFFLINE: return "OFF";
    default:           return "NEVER";
  }
}

// ---------- Zeichenfunktionen ----------
void drawCenteredY(int y, const char *txt, const uint8_t *font) {
  u8g2.setFont(font);
  int w = u8g2.getUTF8Width(txt);
  int x = (128 - w) / 2; if (x < 0) x = 0;
  u8g2.drawUTF8(x, y, txt);
}

// Zeichnet 'txt' linksbündig in eine zentrierte Box der Breite von "88:88:88"
void drawTimeAnchored(int y_baseline, const char *txt, const uint8_t *font) {
  u8g2.setFont(font);
  const char *maxpat = "88:88:88";
  int wbox = u8g2.getStrWidth(maxpat);
  if (wbox < 1) wbox = 1;
  int x = (128 - wbox) / 2;
  u8g2.drawStr(x, y_baseline, txt);
}

void drawCenteredAutoFontY(int y, const char *txt) {
  u8g2.setFont(u8g2_font_8x13B_tf);
  if (u8g2.getUTF8Width(txt) <= 128) { drawCenteredY(y, txt, u8g2_font_8x13B_tf); return; }
  u8g2.setFont(u8g2_font_7x13_tf);
  if (u8g2.getUTF8Width(txt) <= 128) { drawCenteredY(y, txt, u8g2_font_7x13_tf);  return; }
  u8g2.setFont(u8g2_font_6x12_tf);
  if (u8g2.getUTF8Width(txt) <= 128) { drawCenteredY(y, txt, u8g2_font_6x12_tf);  return; }
  drawCenteredY(y, txt, u8g2_font_5x8_tf);
}

void drawStatusBar() {
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 56, 128, 8);

  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_5x8_tf);

  char line[64];
  int       rtt = isnan(g_rtt_ms) ? -1 : (int)lround(g_rtt_ms);
  int        sp = stabilityPercent();
  SyncState  st = currentSyncState();

  if (rtt < 0 && sp < 0)
    snprintf(line, sizeof(line), "Str:%u RTT:--ms Stab:--%% %s", (unsigned)g_stratum, syncText(st));
  else if (rtt < 0)
    snprintf(line, sizeof(line), "Str:%u RTT:--ms Stab:%d%% %s", (unsigned)g_stratum, sp, syncText(st));
  else if (sp < 0)
    snprintf(line, sizeof(line), "Str:%u RTT:%dms Stab:--%% %s", (unsigned)g_stratum, rtt, syncText(st));
  else
    snprintf(line, sizeof(line), "Str:%u RTT:%dms Stab:%d%% %s", (unsigned)g_stratum, rtt, sp, syncText(st));

  u8g2.drawStr(1, 63, line);
  u8g2.setDrawColor(1);
}

void drawIPScreen() {
  static unsigned long last_ip_draw = 0;
  unsigned long now = millis();
  if (now - last_ip_draw < 1000UL) return;
  last_ip_draw = now;

  u8g2.clearBuffer();
  drawCenteredY(14, "IP erhalten",                   u8g2_font_6x12_tf);
  drawCenteredY(32, ETH.localIP().toString().c_str(), u8g2_font_6x10_tf);
  drawCenteredY(50, "Weiter in 5 s",                 u8g2_font_6x10_tf);
  drawStatusBar();
  u8g2.sendBuffer();
}

// ---------- Hilfsfunktion: Statischen Inhalt zeichnen (ohne Zeitzeile) ----------
// dateTxt und last_mday werden als Referenz weitergegeben (static locals aus drawMainScreen)
void drawStaticContent(const tm &t, bool ok, char (&dateTxt)[40], int &last_mday) {
  char midTxt[48];
  if (ok) snprintf(midTxt, sizeof(midTxt), "KW %02d - %s", isoWeek(t), wday[t.tm_wday]);
  else    snprintf(midTxt, sizeof(midTxt), "KW -- - Wochentag");
  drawCenteredAutoFontY(41, midTxt);

  if (ok && t.tm_mday != last_mday) {
    last_mday = t.tm_mday;
    const char *monat = (t.tm_mon >= 0 && t.tm_mon < 12) ? monate[t.tm_mon] : "?";
    snprintf(dateTxt, sizeof(dateTxt), "%d. %s %d", t.tm_mday, monat, t.tm_year + 1900);
  } else if (!ok) {
    snprintf(dateTxt, sizeof(dateTxt), "--. Monat ----");
    last_mday = -1;
  }
  drawCenteredY(54, dateTxt, u8g2_font_8x13B_tf);
  drawStatusBar();
}

// Gesamtdauer der Scroll-Animation – wird in main.cpp für den Pre-Trigger verwendet
// Wert = STEPS × FRAME_MS + Rendering-Overhead (clearBuffer + drawStr + sendBuffer pro Frame)
static const int SCROLL_TOTAL_MS = 80; // STEPS(5) × FRAME_MS(5) + Overhead-Puffer

// ---------- Scroll-Animation ----------
void animateScroll(const char *oldTxt, const char *newTxt,
                   const tm &t, bool ok,
                   char (&dateTxt)[40], int &last_mday) {
  static const int STEPS   = 3;
  static const int CLIP_Y0 = 0;
  static const int CLIP_Y1  = 30;
  static const int FONT_H   = 31;  // Scroll-Versatz (Zeichenhöhe + 1px Puffer)

  u8g2.setFont(u8g2_font_logisoso28_tf);
  int wbox = u8g2.getStrWidth("88:88:88");
  int x0   = (128 - wbox) / 2;

  // Digit-Indizes in "HH:MM:SS" (Doppelpunkte an Pos. 2 und 5 überspringen)
  static const int DIG_IDX[6] = {0, 1, 3, 4, 6, 7};

  // Pro Ziffer: geändert? Clip-X? Clip-Breite?
  struct DigInfo { bool changed; int clip_x, clip_w; };
  DigInfo digs[6];
  bool any = false;

  for (int d = 0; d < 6; d++) {
    int i = DIG_IDX[d];
    digs[d].changed = (oldTxt[i] != newTxt[i]);
    if (!digs[d].changed) continue;
    any = true;

    // X-Position der Ziffer i im neuen String (exakte Präfix-Breite)
    char prefix[9] = {0};
    memcpy(prefix, newTxt, i);
    digs[d].clip_x = x0 + u8g2.getStrWidth(prefix);

    // Clip-Breite = Maximum aus alter und neuer Ziffer + 1px Sicherheitspuffer
    // (Glyph-Pixel können bis zur vollen Advance-Breite reichen)
    char cn[2] = {newTxt[i], '\0'};
    char co[2] = {oldTxt[i], '\0'};
    digs[d].clip_w = max(u8g2.getStrWidth(cn), u8g2.getStrWidth(co)) + 1;
  }
  if (!any) return;

  // Basis-Frame einmalig rendern (statischer Inhalt + neue Zeitzeile für unveränderte Zeichen).
  // Dieser Buffer wird jedes Frame als Ausgangspunkt kopiert – spart clearBuffer+drawAll pro Frame.
  static uint8_t base_buf[DISPLAY_BUF_SIZE];
  u8g2.clearBuffer();
  drawStaticContent(t, ok, dateTxt, last_mday);
  u8g2.setFont(u8g2_font_logisoso28_tf);
  drawTimeAnchored(28, newTxt, u8g2_font_logisoso28_tf);
  memcpy(base_buf, u8g2.getBufferPtr(), DISPLAY_BUF_SIZE);

  uint8_t *buf = u8g2.getBufferPtr();

  // Subsekunden-Kompensation: direkt am richtigen Frame einsteigen.
  // ms_late = wie viele ms nach dem idealen Startzeitpunkt wir erkannt haben.
  // startStep = welchen Frame wir überspringen; last_t rückdatiert den Frame-Timer,
  // sodass beide Uhren danach im gleichen absoluten Takt ticken.
  struct timeval tv_sync;
  gettimeofday(&tv_sync, NULL);
  int ms_in_sec_sync = (int)(tv_sync.tv_usec / 1000);
  int ms_late        = ms_in_sec_sync - (1000 - SCROLL_TOTAL_MS);
  if (ms_late < 0) ms_late = 0;
  const int ms_per_frame = SCROLL_TOTAL_MS / (STEPS + 1);
  int startStep          = min(ms_late / ms_per_frame, STEPS);
  unsigned long last_t   = millis() - (unsigned long)(ms_late % ms_per_frame);

  for (int step = startStep; step <= STEPS; step++) {
    int offset = (step * FONT_H) / STEPS;

    // Basis-Frame wiederherstellen (statischer Inhalt + unveränderte Zeichen korrekt)
    memcpy(buf, base_buf, DISPLAY_BUF_SIZE);

    // Jede geänderte Ziffer einzeln scrollen
    u8g2.setFont(u8g2_font_logisoso28_tf);
    for (int d = 0; d < 6; d++) {
      if (!digs[d].changed) continue;
      int gx = digs[d].clip_x;
      int gw = digs[d].clip_w;

      u8g2.setClipWindow(gx, CLIP_Y0, gx + gw - 1, CLIP_Y1);
      u8g2.setDrawColor(0);
      u8g2.drawBox(gx, CLIP_Y0, gw, CLIP_Y1 - CLIP_Y0 + 1);
      u8g2.setDrawColor(1);
      // Vollständige Strings zeichnen – Clip lässt nur die eine Ziffer sichtbar
      u8g2.drawStr(x0, 28 - offset,          oldTxt);  // alt scrollt nach oben
      u8g2.drawStr(x0, 28 - offset + FONT_H, newTxt);  // neu kommt von unten
      u8g2.setMaxClipWindow();
    }

    u8g2.sendBuffer();
    if (step < STEPS) {
      last_t += (unsigned long)ms_per_frame;
      long wait_ms = (long)(last_t - millis());
      if (wait_ms > 0) delay((int)wait_ms);
    }
  }
}

// ---------- Crossfade-Animation (Bayer-Dithering) ----------

// 8×8 Bayer-Matrix (Werte 0..63)
static const uint8_t bayer8[8][8] = {
  { 0,32, 8,40, 2,34,10,42},
  {48,16,56,24,50,18,58,26},
  {12,44, 4,36,14,46, 6,38},
  {60,28,52,20,62,30,54,22},
  { 3,35,11,43, 1,33, 9,41},
  {51,19,59,27,49,17,57,25},
  {15,47, 7,39,13,45, 5,37},
  {63,31,55,23,61,29,53,21}
};

void animateCrossfade(const uint8_t *old_buf, const char *newTxt,
                      const tm &t, bool ok,
                      char (&dateTxt)[40], int &last_mday) {
  static const int STEPS   = 3;    // Anzahl Schritte – wie beim Scroll anpassbar
  static uint8_t   new_buf[DISPLAY_BUF_SIZE]; // neuer Frame, einmal gerendert

  // Neuen Frame einmalig aufbauen und sichern
  u8g2.clearBuffer();
  drawStaticContent(t, ok, dateTxt, last_mday);
  drawTimeAnchored(28, newTxt, u8g2_font_logisoso28_tf);
  memcpy(new_buf, u8g2.getBufferPtr(), DISPLAY_BUF_SIZE);

  uint8_t *buf = u8g2.getBufferPtr();

  // Subsekunden-Kompensation (identisch zum Scroll-Ansatz)
  struct timeval tv_sync;
  gettimeofday(&tv_sync, NULL);
  int ms_in_sec_sync = (int)(tv_sync.tv_usec / 1000);
  int ms_late        = ms_in_sec_sync - (1000 - SCROLL_TOTAL_MS);
  if (ms_late < 0) ms_late = 0;
  const int ms_per_frame = SCROLL_TOTAL_MS / (STEPS + 1);
  int startStep          = min(ms_late / ms_per_frame, STEPS);
  unsigned long last_t   = millis() - (unsigned long)(ms_late % ms_per_frame);

  for (int step = startStep; step <= STEPS; step++) {
    // thresh_max skaliert immer von 0..64, unabhängig von STEPS
    int thresh_max = (step * 64) / STEPS;

    // Per-Pixel Bayer-Mischung
    // Buffer-Layout: Byte[page*128 + x], Bit b = Pixel(x, page*8+b)
    // Unveränderte Pixel werden nicht gedithert → keine Artefakte in statischen Bereichen
    for (int page = 0; page < 8; page++) {
      for (int x = 0; x < 128; x++) {
        int     idx      = page * 128 + x;
        uint8_t old_byte = old_buf[idx];
        uint8_t new_byte = new_buf[idx];
        uint8_t mix      = 0;
        for (int b = 0; b < 8; b++) {
          uint8_t old_bit = (old_byte >> b) & 1;
          uint8_t new_bit = (new_byte >> b) & 1;
          uint8_t bit;
          if (old_bit == new_bit) {
            bit = new_bit; // Pixel unverändert – kein Dithering
          } else {
            bit = (bayer8[b][x & 7] < thresh_max) ? new_bit : old_bit;
          }
          mix |= (bit << b);
        }
        buf[idx] = mix;
      }
    }
    u8g2.sendBuffer();
    if (step < STEPS) {
      last_t += (unsigned long)ms_per_frame;
      long wait_ms = (long)(last_t - millis());
      if (wait_ms > 0) delay((int)wait_ms);
    }
  }

  // Neuen Frame abschließend im Buffer hinterlassen (für cf_old_buf-Sicherung)
  memcpy(buf, new_buf, DISPLAY_BUF_SIZE);
}

// ---------- Hauptanzeige ----------
// t_override: optionaler Zeit-Override für Pre-Trigger (nullptr = aktuelle Zeit holen)
void drawMainScreen(const tm *t_override = nullptr) {
  static char    prevTimeTxt[16]  = "";
  static char    dateTxt[40]      = "--. Monat ----";
  static int     last_mday        = -1;
  static uint8_t cf_old_buf[DISPLAY_BUF_SIZE];  // Letzter gesendeter Frame für Crossfade

  tm t_local;
  bool ok;
  if (t_override) {
    t_local = *t_override;
    ok = true;
  } else {
    ok = getLocalTime(&t_local, 10);
  }

  char timeTxt[16];
  if (ok) snprintf(timeTxt, sizeof(timeTxt), "%02d:%02d:%02d",
                   t_local.tm_hour, t_local.tm_min, t_local.tm_sec);
  else    snprintf(timeTxt, sizeof(timeTxt), "--:--:--");

  bool timeChanged = (strcmp(timeTxt, prevTimeTxt) != 0);
  bool doAnim      = timeChanged && prevTimeTxt[0] != '\0';

  if (doAnim && g_anim_mode == ANIM_SCROLL) {
    animateScroll(prevTimeTxt, timeTxt, t_local, ok, dateTxt, last_mday);

  } else if (doAnim && g_anim_mode == ANIM_CROSSFADE) {
    animateCrossfade(cf_old_buf, timeTxt, t_local, ok, dateTxt, last_mday);

  } else {
    // Normal oder erster Frame nach Boot
    u8g2.clearBuffer();
    drawTimeAnchored(28, timeTxt, u8g2_font_logisoso28_tf);
    drawStaticContent(t_local, ok, dateTxt, last_mday);
    u8g2.sendBuffer();
  }

  strcpy(prevTimeTxt, timeTxt);

  // Aktuellen Frame für nächsten Crossfade sichern
  if (g_anim_mode == ANIM_CROSSFADE) {
    memcpy(cf_old_buf, u8g2.getBufferPtr(), DISPLAY_BUF_SIZE);
  }
}
