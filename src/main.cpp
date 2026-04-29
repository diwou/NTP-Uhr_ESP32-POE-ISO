/*
 * ESP32-POE-ISO NTP-Uhr (ST7920 128x64) – Web-UI + Stratum/RTT/Stabilität
 * Erweiterung: VEML7700 Umgebungslicht -> PWM Backlight (heller Umgebung = heller Display)
 *
 * Aktuelle Version: 1.5.0 – Changelog: siehe CHANGELOG.md
 *
 * Verdrahtung (zusätzlich zum bestehenden Display):
 *   VEML7700  VIN  -> 3V3  (empfohlen, auch wenn Modul 3-5V kann)
 *            GND  -> GND
 *            SDA  -> GPIO32
 *            SCL  -> GPIO33
 *            3Vo  -> unbenutzt (3,3V-Regler-Ausgang vom Modul)
 *
 * Backlight-Dimmung (Low-Side, IRLZ44N empfohlen):
 *   LCD BLA (Backlight +) -> +5V
 *   LCD BLK (Backlight -) -> MOSFET DRAIN
 *   MOSFET SOURCE -> GND
 *   MOSFET GATE   -> GPIO5 über 100Ω
 *   Gate Pulldown -> 100kΩ nach GND
 *
 * Hinweis:
 * - Der MOSFET dimmt nur das Backlight. Logik bleibt wie gehabt über 74HCT245.
 * - GND von 5V-Backlight und ESP32 muss gemeinsam sein.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_sntp.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Adafruit_VEML7700.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <esp_system.h>
#include <esp_eth.h>          // esp_eth_mediator_t, ETHERNET_EVENT_START

#include "config.h"     // Schicht 0: Konstanten, Globals, NTP-/Backlight-Logik, Utils
#include "display.h"    // Schicht 1: Display-Funktionen (nutzen config.h)
#include "webserver.h"  // Schicht 2: Web-Handler (nutzen config.h + display.h)

// ---------- Ethernet-Events ----------
void WiFiEvent(arduino_event_id_t e) {
  if (e == ARDUINO_EVENT_ETH_GOT_IP) {
    eth_got_ip = true; ui_state = UI_SHOW_IP; ui_since = millis();
    char tz_str[64];
    buildTZString(tz_str, sizeof(tz_str));
    setenv("TZ", tz_str, 1); tzset();
    applySNTP();
    if (ntp_task_handle) xTaskNotifyGive(ntp_task_handle);
    if (!ota_ready) { ArduinoOTA.begin(); ota_ready = true; }
  }
  if (e == ARDUINO_EVENT_ETH_DISCONNECTED || e == ARDUINO_EVENT_ETH_STOP) {
    eth_got_ip = false; ui_state = UI_BOOT;
    g_rtt_ms = NAN; g_stratum = 0; rtt_count = rtt_index = 0;
  }
}

// ---------- NTP-Task (FreeRTOS, Core 0) ----------
static void ntpTask(void *) {
  Serial.printf("[NTP-Task] gestartet auf Core %d\n", (int)xPortGetCoreID());
  for (;;) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(QUERY_EVERY_MS));

    if (!eth_got_ip) continue;

    const char *host = ntp1[0] ? ntp1 : "pool.ntp.org";
    uint8_t s = 0;
    double  r = NAN;
    Serial.printf("[NTP-Task] Abfrage %s ...\n", host);
    unsigned long t0 = millis();
    bool ok = ntpQueryOnce(host, s, r);
    Serial.printf("[NTP-Task] %s  Dauer: %lu ms\n", ok ? "OK" : "FEHLER", millis() - t0);
    if (ok) {
      g_stratum = s;
      g_rtt_ms  = (float)r;
      addRttSample(r);
    }
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(50);
  captureResetReason();
  Serial.printf("[BOOT] Reset-Grund: %s\n", g_last_reset_reason);

  // PWM Backlight init (ESP32-Core 3.x kompatibel)
  pinMode(BL_PIN, OUTPUT);
  analogWriteResolution(8);
  analogWriteFrequency(20000); // 20 kHz
  pwm_current = BL_MAX;
  pwm_target_global = BL_MAX;
  setBacklight(BL_MAX);

  // I2C + VEML7700 init
  Wire.begin(I2C_SDA, I2C_SCL);
  veml_ok = veml.begin();
  if (veml_ok) {
    veml.setGain(VEML7700_GAIN_2);
    veml.setIntegrationTime(VEML7700_IT_50MS);
    veml.interruptEnable(false);
  } else {
    Serial.println("VEML7700 init failed");
  }
  bl_warmup_until_ms = millis() + 2500;

  // Display
  u8g2.begin();
  u8g2.setContrast(200);
  u8g2.clearBuffer();
  drawCenteredY(16, "Starte Ethernet...", u8g2_font_6x12_tf);
  drawCenteredY(32, "Warte auf Link...",  u8g2_font_6x12_tf);
  u8g2.sendBuffer();

  // NTP-Server und Einstellungen aus NVS laden
  prefs.begin("ntp", true);
  String s1 = prefs.getString("s1", "0.de.pool.ntp.org");
  String s2 = prefs.getString("s2", s1);
  String s3 = prefs.getString("s3", s1);
  uint32_t iv     = prefs.getULong("iv", 60 * 1000UL);
  uint8_t  anim_m = prefs.getUChar("anim", 0);
  prefs.end();
  copyTrunc(ntp1, s1.c_str());
  copyTrunc(ntp2, s2.c_str());
  copyTrunc(ntp3, s3.c_str());
  ntp_interval_ms = iv ? iv : 60 * 1000UL;
  g_anim_mode = (AnimMode)constrain((int)anim_m, 0, 2);

  // Zeitzone aus NVS laden
  g_tz_offset      = (int)prefs.getInt("tz_off", 60);
  g_tz_dst_mode    = (DstMode)constrain((int)prefs.getUChar("tz_dst", (uint8_t)DST_EU), 0, 3);
  String ds = prefs.getString("tz_ds", "M3.5.0/2");
  String de = prefs.getString("tz_de", "M10.5.0/3");
  g_tz_dst_minutes = (int)prefs.getUShort("tz_dm", 60);
  copyTrunc(g_tz_dst_start, ds.c_str());
  copyTrunc(g_tz_dst_end,   de.c_str());

  // Netzwerk-Konfiguration aus NVS laden
  prefs.begin("net", true);
  g_use_dhcp = prefs.getBool("dhcp", true);
  String sip  = prefs.getString("ip",  "192.168.1.100");
  String sgw  = prefs.getString("gw",  "192.168.1.1");
  String ssn  = prefs.getString("sn",  "255.255.255.0");
  String sdns = prefs.getString("dns", "8.8.8.8");
  prefs.end();
  copyTrunc(g_static_ip,  sip.c_str());
  copyTrunc(g_static_gw,  sgw.c_str());
  copyTrunc(g_static_sn,  ssn.c_str());
  copyTrunc(g_static_dns, sdns.c_str());

  // 100 Mbit/s erzwingen: PHY-Register BMCR (Reg 0) direkt per MDIO beschreiben.
  // Entspricht dem bewährten 1.4.2-Pfad: nur beim Ethernet-Start schreiben,
  // danach den ausgehandelten Link nur noch diagnostizieren.
  // Wert 0x2100: Bit 13 = 100 Mbit/s, Bit 8 = Full-Duplex, Bit 12 = AN-aus.
  auto forceEth100Handler = [](void*, esp_event_base_t, int32_t event_id, void* data) {
      const char *phase = (event_id == ETHERNET_EVENT_CONNECTED) ? "ETH_CONNECTED" : "ETH_START";
      if (event_id == ETHERNET_EVENT_CONNECTED) {
        snprintf(g_eth_force_status, sizeof(g_eth_force_status),
                 "%s: Link %uM %s", phase, (unsigned)ETH.linkSpeed(),
                 ETH.fullDuplex() ? "Full" : "Half");
        return;
      }

      if (!data) {
        forceEth100FullDuplex(nullptr, phase);
        Serial.printf("[ETH] 100-Mbit-Erzwingung fehlgeschlagen: %s\n", g_eth_force_status);
        return;
      }

      esp_eth_handle_t    h   = *(esp_eth_handle_t*)data;
      if (forceEth100FullDuplex(h, phase)) {
        Serial.printf("[ETH] BMCR=0x2100: 100 Mbit/s Full-Duplex erzwungen (%s)\n", phase);
      } else {
        Serial.printf("[ETH] 100-Mbit-Erzwingung fehlgeschlagen: %s\n", g_eth_force_status);
      }
    };
  esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_START, forceEth100Handler, nullptr);
  esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_CONNECTED, forceEth100Handler, nullptr);

  // Der Olimex ETH_PHY_POWER-Pin (GPIO12) schaltet den Ethernet-PHY. Ein kurzer
  // PHY-Power-Cycle vor ETH.begin() verhindert 10M-Links nach Soft-Neustarts.
  pinMode(ETH_POWER_PIN, OUTPUT);
  digitalWrite(ETH_POWER_PIN, LOW);
  delay(250);
  digitalWrite(ETH_POWER_PIN, HIGH);
  delay(250);

  // Ethernet (statische IP vor begin() konfigurieren, falls gewünscht)
  WiFi.onEvent(WiFiEvent);
  if (!g_use_dhcp) {
    IPAddress a_ip, a_gw, a_sn, a_dns;
    if (a_ip.fromString(g_static_ip) && a_gw.fromString(g_static_gw) &&
        a_sn.fromString(g_static_sn) && a_dns.fromString(g_static_dns)) {
      ETH.config(a_ip, a_gw, a_sn, a_dns);
    }
  }
  ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);

  // ArduinoOTA konfigurieren (begin() folgt in WiFiEvent nach IP-Erhalt)
  ArduinoOTA.setHostname("esp32-ntp-uhr");
  ArduinoOTA.onStart([]() {
    u8g2.clearBuffer();
    drawCenteredY(20, "Arduino OTA", u8g2_font_8x13B_tf);
    drawCenteredY(36, "Empfange...", u8g2_font_6x12_tf);
    u8g2.drawFrame(4, 46, 120, 8);
    u8g2.sendBuffer();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int pct = total ? (int)((progress * 100U) / total) : 0;
    char buf[8]; snprintf(buf, sizeof(buf), "%d%%", pct);
    u8g2.clearBuffer();
    drawCenteredY(20, "Arduino OTA", u8g2_font_8x13B_tf);
    drawCenteredY(36, buf,           u8g2_font_8x13B_tf);
    u8g2.drawFrame(4, 46, 120, 8);
    u8g2.drawBox(4, 46, (120 * pct) / 100, 8);
    u8g2.sendBuffer();
  });
  ArduinoOTA.onEnd([]() {
    u8g2.clearBuffer();
    drawCenteredY(28, "OTA fertig!", u8g2_font_8x13B_tf);
    drawCenteredY(44, "Neustart...", u8g2_font_6x12_tf);
    u8g2.sendBuffer();
  });
  ArduinoOTA.onError([](ota_error_t error) {
    char buf[20]; snprintf(buf, sizeof(buf), "Fehler #%u", (unsigned)error);
    u8g2.clearBuffer();
    drawCenteredY(20, "OTA Fehler!", u8g2_font_8x13B_tf);
    drawCenteredY(36, buf,           u8g2_font_6x12_tf);
    u8g2.sendBuffer();
    delay(2000);
  });

  // Web-Routen registrieren
  server.on("/",           handleRoot);
  server.on("/save",       HTTP_POST, handleSave);
  server.on("/sync",       HTTP_POST, handleSync);
  server.on("/api/status", HTTP_GET,  handleApiStatus);
  server.on("/update",     HTTP_GET,  handleOTAPage);
  server.on("/update",     HTTP_POST, handleOTAPost, handleOTAUpload);
  server.on("/reboot",     HTTP_POST, handleReboot);
  server.on("/factory-reset", HTTP_POST, handleFactoryReset);
  server.on("/savenet",    HTTP_POST, handleSaveNet);
  server.onNotFound([]() {
    server.sendHeader("Location", "/");
    server.send(303);
  });
  server.begin();

  // NTP-Task auf Core 0 starten (Arduino-Loop läuft auf Core 1)
  xTaskCreatePinnedToCore(ntpTask, "ntp_query", 4096, nullptr, 1, &ntp_task_handle, 0);
}

// ---------- Loop ----------
void loop() {
  server.handleClient();
  ArduinoOTA.handle();

  updateBacklightFromLux();   // Zielwert ~1 Hz aktualisieren (stabil)
  rampBacklight();            // Ausgabe weich nachführen (50 Hz)

  // IP-Screen 5 s anzeigen, dann in RUN wechseln
  if (ui_state == UI_SHOW_IP) {
    if (millis() - ui_since <= 5000UL) {
      drawIPScreen();
    } else {
      ui_state = UI_RUN;
    }
  }

  // Hauptanzeige sekündlich aktualisieren
  static int last_draw_sec = -1;
  if (ui_state == UI_RUN) {
    if (g_anim_mode != ANIM_NONE) {
      // Pre-Trigger: Animation startet SCROLL_TOTAL_MS vor der Sekundengrenze,
      // sodass sie exakt beim Sekundenwechsel abgeschlossen ist.
      // Damit scrollt die Uhr synchron zu nicht-animierten Uhren.
      struct timeval tv;
      gettimeofday(&tv, NULL);
      int ms_in_sec = (int)(tv.tv_usec / 1000);

      if (ms_in_sec >= (1000 - SCROLL_TOTAL_MS)) {
        static time_t last_anim_epoch = -1;
        time_t next_epoch = tv.tv_sec + 1;
        if (next_epoch != last_anim_epoch) {
          last_anim_epoch = next_epoch;
          tm t_next;
          localtime_r(&next_epoch, &t_next);
          last_draw_sec = t_next.tm_sec;
          drawMainScreen(&t_next);
        }
      }
    } else {
      // Normalmodus: exakt beim Sekundenwechsel zeichnen
      tm t;
      if (getLocalTime(&t, 1)) {
        if (t.tm_sec != last_draw_sec) {
          last_draw_sec = t.tm_sec;
          drawMainScreen();
        }
      } else {
        unsigned long s = millis() / 1000UL;
        if ((int)s != last_draw_sec) {
          last_draw_sec = (int)s;
          drawMainScreen();
        }
      }
    }
  }

  delay(5);
}
