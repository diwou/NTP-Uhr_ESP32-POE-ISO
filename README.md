# NTP-Uhr ESP32-POE-ISO

Firmware fuer eine NTP-synchronisierte Uhr auf Basis des Olimex ESP32-POE-ISO
mit ST7920-Display, Ethernet, Web-UI, OTA-Update und automatischer
Backlight-Regelung ueber einen VEML7700-Luxsensor.

## Hardware

- Olimex ESP32-POE-ISO
- ST7920 128x64 LCD im seriellen 3-Leiter-Modus
- 74HCT245-Pegelwandler fuer das Display
- VEML7700-Luxsensor per I2C
- MOSFET-Treiber fuer PWM-Backlight

Wichtige Pins:

| Funktion | GPIO |
|---|---:|
| LCD CS / RS | 16 |
| LCD CLK / E | 14 |
| LCD SID / R/W | 13 |
| LCD RST | 4 |
| VEML7700 SDA | 32 |
| VEML7700 SCL | 33 |
| Backlight PWM | 5 |
| ETH MDC | 23 |
| ETH MDIO | 18 |
| ETH Power | 12 |
| ETH CLK | 17 |

## Funktionen

- NTP-Zeitsynchronisation per Ethernet
- Statusanzeige fuer Stratum, RTT und Stabilitaet
- Web-UI fuer Status, NTP-Einstellungen, Netzwerkparameter und OTA-Update
- Browser-OTA ueber `/update`
- ArduinoOTA ueber PlatformIO
- Automatische Backlight-Regelung ueber VEML7700
- Display-Animationen: normal, Vertical Scroll und Crossfade
- HTTP-Basic-Auth zum Schutz der Einstellungen (Default: `admin`/`admin`)

## Authentifizierung

Die Web-UI verwendet HTTP Basic Authentication fuer alle Aenderungen
(POST-Endpunkte und OTA-Update). Die Statusseite und API bleiben ohne
Auth zugaenglich.

- **Default-Username:** `admin`
- **Default-Passwort:** `admin`

Das Passwort kann im System-Tab der Web-UI geaendert werden und wird
persistent im NVS gespeichert.

## Build

Das Projekt verwendet PlatformIO:

```bash
pio run
```

OTA-Upload ueber die in `platformio.ini` konfigurierte Adresse:

```bash
pio run -t upload
```

Alternativ kann die erzeugte Firmware-Datei per Web-UI hochgeladen werden:

```text
.pio/build/esp32-poe-iso/firmware.bin
```

## Projektstruktur

- `src/main.cpp`: Setup, Loop, Ethernet-Events, OTA-Handler-Registrierung
- `src/config.h`: Globale Konfiguration, NTP-Logik, Backlight-Regelung
- `src/display.h`: Display-Ausgabe und Animationen
- `src/webserver.h`: Web-UI und HTTP-Handler
- `platformio.ini`: PlatformIO-Zielkonfiguration
- `CHANGELOG.md`: Versionshistorie

## Lizenz

Dieses Projekt steht unter der Creative Commons
Attribution-NonCommercial-ShareAlike 4.0 International Lizenz
(CC BY-NC-SA 4.0).

Kurzfassung:

- Namensnennung erforderlich
- keine kommerzielle Nutzung
- Weitergabe von Bearbeitungen nur unter gleichen Bedingungen

Der vollstaendige Lizenztext ist in `LICENSE` zusammengefasst und unter
https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode verfuegbar.
