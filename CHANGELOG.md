# Changelog – NTP-Uhr ESP32-POE-ISO

Alle wichtigen Änderungen an diesem Projekt werden hier dokumentiert.
Das Format basiert auf [Keep a Changelog](https://keepachangelog.com/de/1.0.0/)
und dieses Projekt verwendet [Semantische Versionierung](https://semver.org/lang/de/).

Hinweis: Für ältere Einträge vor `1.4.5` liegen in diesem Arbeitsverzeichnis
keine belastbaren Datumsquellen mehr vor. Diese Einträge bleiben daher bis zu
einer späteren Verifikation undatiert, statt ein Datum zu raten.

## [Unreleased]

### Behoben
- `platformio.ini`: OTA-Environment `esp32-poe-iso_ota` erbt nun von
  `env:esp32-poe-iso` statt dem falschen `env:nodemcuv2` (ESP8266).
  Der Fehler führte zu einem Binary für die falsche Architektur.
  (db00298)

## 1.5.0 - 2026-04-07
### Neu
- Reset-Grund wird beim Boot seriell ausgegeben und im System-Tab der Web-UI
  angezeigt, um spontane Neustarts ohne serielle Verbindung besser einzugrenzen.
- Status-Tab zeigt Gateway, DNS-Server, Netzwerkmodus und konfigurierten DNS,
  damit DNS-Probleme bei lokalen Hostnamen per Webinterface sichtbar sind.
- NTP-Diagnose wird im System-Tab und per `/api/status` ausgegeben; der
  manuelle NTP-Pfad nutzt weiter `WiFi.hostByName()` wie im stabilen 1.4.4-Stand.

### Behoben
- 100-Mbit/s-Full-Duplex-Erzwingung wieder auf den bewährten 1.4.2-Pfad
  zurückgestellt: Beim Ethernet-Start wird ausschließlich `BMCR=0x2100`
  geschrieben; bei Link-Up wird der ausgehandelte Link nur diagnostiziert.
- Ethernet-PHY wird vor `ETH.begin()` kurz per GPIO12/`ETH_PHY_POWER` aus- und
  wieder eingeschaltet, damit Soft-Neustarts nicht auf einem hängenden
  10M-Link-Zustand hängen bleiben.
- Alle relevanten Konfigurationsstrings werden jetzt sicher begrenzt kopiert;
  fehlende Nullterminierung durch `strncpy` ist damit beseitigt.
- Web-UI gegen HTML-Injection durch Konfigurationswerte gehärtet:
  NTP-Server- und Netzwerkfelder werden vor der HTML-Ausgabe escaped.
- ArduinoOTA-Fortschrittsberechnung robuster gemacht (`progress * 100 / total`
  statt Division über `total / 100`).
- Die Hauptseite der Web-UI wird mit `Cache-Control: no-store` ausgeliefert,
  damit nach OTA-Updates keine alte HTML-Version im Browser hängen bleibt.

### Geändert
- Ethernet-Diagnose im Webinterface erweitert: Link-Speed, Duplex und letzter
  100-Mbit-Erzwingungsstatus sind per Status/System-Ansicht und `/api/status`
  sichtbar.
- OTA-Upload über Netzwerk ist wieder als aktiver Upload-Pfad in
  `platformio.ini` eingestellt; der USB-Recovery-Port bleibt auskommentiert
  dokumentiert.

## 1.4.5 - 2026-04-07
### Neu
- Recovery-Funktion im System-Tab ergänzt: Die Web-UI kann die persistenten
  Projekt-Einstellungen aus den NVS-Namensräumen `ntp` und `net` löschen und
  den ESP32 anschließend neu starten.

### Geändert
- Build-Konfiguration explizit auf die offizielle `default_16MB.csv`
  Partitionstabelle für das Olimex ESP32-POE-ISO-16MB-Board gestellt.
- Upload-Konfiguration für Recovery-Builds auf den erkannten seriellen Port
  `/dev/ttyUSB0` gestellt; ESPOTA bleibt auskommentiert in `platformio.ini`
  erhalten.

## 1.4.4
- Stabilitätsanzeige korrigiert: statt inversem Variationskoeffizient (σ/μ)
  wird nun absoluter Jitter verwendet (σ / 5 ms). Der CV-Ansatz benachteiligte
  lokale Server mit niedrigem RTT, obwohl deren absoluter Jitter geringer ist.

## 1.4.3
- Subsekunden-Kompensation in Scroll und Crossfade: `startStep = ms_late / ms_per_frame`
  überspringt bereits vergangene Frames; `last_t` rückdatiert den Frame-Timer auf den
  Framestart; `last_t += ms_per_frame` statt `delay()` verhindert Drift.
  Zwei Uhren am gleichen NTP-Server synchronisieren auf ≤ 1 Frame (≈ 20 ms).

## 1.4.2
- 100 Mbit/s Full-Duplex erzwingen: PHY-Register BMCR (Reg 0 = 0x2100)
  per MDIO via `esp_eth_mediator_t` auf `ETHERNET_EVENT_START` gesetzt
  (vor AN-Beginn; ESP-IDF 4.4 hat kein `ETH_CMD_S_SPEED`)

## 1.4.1
- Netzwerkeinstellungen im Web-UI: DHCP oder statische IP (IP, Subnetz, Gateway, DNS)
  – persistent in NVS (Namespace `"net"`), wird vor `ETH.begin()` angewendet;
  Änderungen erfordern Neustart (automatisch nach Speichern)

## 1.4.0
- Web-UI komplett überarbeitet: 3 Tabs (Status / Einstellungen / System),
  Toast-Benachrichtigungen, AJAX-Speichern ohne Seitenreload,
  MAC-Adresse + Link-Speed in Status, OTA-Upload im System-Tab inline,
  Neustart-Button (`/reboot` POST), AJAX-Polling auf 3 s beschleunigt

## 1.3.4
- Crossfade: STEPS=5/FRAME_MS=5 ms, `thresh_max`-Formel auf `(step*64)/STEPS`
  korrigiert (funktioniert nun mit beliebiger STEPS-Anzahl, erreicht immer 100 %)

## 1.3.3
- Scroll: Basis-Frame einmalig rendern + `memcpy` statt `clearBuffer` pro Frame
  (drastisch weniger Overhead); STEPS=5/FRAME_MS=5 ms → ~60–80 ms Gesamtdauer;
  `SCROLL_TOTAL_MS=80` → Pre-Trigger bei 920 ms für Synchronität

## 1.3.2
- Scroll: Clip-Breite +1 px (Glyph-Artefakte rechts behoben), Geschwindigkeit
  auf 12×20 ms=240 ms (wie Dottie); Pre-Trigger via `gettimeofday()`: Animation
  startet 240 ms vor Sekundengrenze → endet synchron mit anderen Uhren

## 1.3.1
- Scroll: Einzelziffern statt Paare, exakte Zeichenpositionen via Präfix-Breite,
  Geschwindigkeit 300 ms → 128 ms
- Crossfade: Dithering nur auf veränderten Pixeln, Geschwindigkeit 360 ms → 240 ms

## 1.3.0
- Display-Animationen: Vertical Scroll (Ziffern von unten nach oben) und
  Dither-Crossfade (Bayer-Überblendung) – per Web-UI umschaltbar, in NVS persistent;
  nur geänderte Ziffern (HH/MM/SS) werden animiert

## 1.2.0
- OTA-Update (ArduinoOTA + Browser-Upload `/update`)
- AJAX-Statuspolling (`/api/status`), Lux/Helligkeit/Uptime im Web-UI
- Semantische Versionierung (Major.Minor.Patch)

## 1.1.1 / REV3
- Code-Optimierungen: `__builtin_bswap32`, `wday`/`monate` file-scope, Datums-Cache,
  `drawIPScreen`-Throttle, `fmtLocal`-Variablenname, `LUX_ALPHA` entfernt,
  `IT_50MS`-Kommentar korrigiert, Forward-Deklarationen entfernt

## REV2
- NTP-Abfrage in eigenem FreeRTOS-Task (Core 0) – Loop blockiert nicht mehr
- `g_rtt_ms` von `double` auf `float` (32-bit atomar auf ESP32, kein Race-Condition-Risiko)

## REV1
- Bugfixes: UTF-8/März, Donnerstag
- Web-UI Dark-Theme
