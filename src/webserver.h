#pragma once
// Setzt voraus: config.h und display.h wurden bereits eingebunden
// (server, prefs, SyncState, currentSyncState, stabilityPercent,
//  fmtLocal, fmtUptime, drawCenteredY, u8g2, Update).

// ---------- HTML-Seite (SPA, 3 Tabs) ----------
static String htmlEscape(const String &input) {
  String out;
  out.reserve(input.length() + 16);
  for (size_t i = 0; i < input.length(); i++) {
    switch (input[i]) {
      case '&': out += F("&amp;");  break;
      case '<': out += F("&lt;");   break;
      case '>': out += F("&gt;");   break;
      case '"': out += F("&quot;"); break;
      case '\'': out += F("&#39;"); break;
      default: out += input[i];     break;
    }
  }
  return out;
}

static String jsonEscape(const String &input) {
  String out;
  out.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    switch (c) {
      case '\\': out += F("\\\\"); break;
      case '"':  out += F("\\\""); break;
      case '\n': out += F("\\n");  break;
      case '\r': out += F("\\r");  break;
      case '\t': out += F("\\t");  break;
      default:
        if ((uint8_t)c < 0x20) out += ' ';
        else out += c;
        break;
    }
  }
  return out;
}

String htmlPage() {
  String ip    = eth_got_ip ? ETH.localIP().toString() : String("-");
  String mac_s = ETH.macAddress();  // verfügbar nach ETH.begin()
  String link_s = eth_got_ip ? (String((int)ETH.linkSpeed()) + " Mbps") : String("Kein Link");
  String duplex_s = eth_got_ip ? (ETH.fullDuplex() ? String("Full") : String("Half")) : String("-");
  String gw_s = eth_got_ip ? ETH.gatewayIP().toString() : String("-");
  String dns_s = eth_got_ip ? ETH.dnsIP().toString() : String("-");
  String netmode_s = g_use_dhcp ? String("DHCP") : String("Statisch");
  String last  = have_synced ? fmtLocal(last_sync) : String("noch nie");
  String rtt_s = isnan(g_rtt_ms) ? String("-") : String((int)lround(g_rtt_ms)) + " ms";
  int sp = stabilityPercent();
  String stab_s = (sp < 0) ? String("-") : (String(sp) + " %");
  String lux_s  = isfinite(lux_hold) ? (String(lux_hold, 1) + " lx") : String("-");
  String bl_s   = String((int)((pwm_current * 100) / 255)) + " %";
  String uptime = fmtUptime(millis() / 1000UL);
  const char *animNames[] = {"Keine (Normal)", "Vertical Scroll", "Crossfade"};
  String anim_s = animNames[constrain((int)g_anim_mode, 0, 2)];
  String ntp1Esc = htmlEscape(String(ntp1));
  String ntp2Esc = htmlEscape(String(ntp2));
  String ntp3Esc = htmlEscape(String(ntp3));
  String staticIpEsc = htmlEscape(String(g_static_ip));
  String staticSnEsc = htmlEscape(String(g_static_sn));
  String staticGwEsc = htmlEscape(String(g_static_gw));
  String staticDnsEsc = htmlEscape(String(g_static_dns));
  String ntpDiagEsc  = htmlEscape(String(g_last_ntp_status));
  String ethForceEsc  = htmlEscape(String(g_eth_force_status));
  String resetEsc     = htmlEscape(String(g_last_reset_reason));

  SyncState st = currentSyncState();
  const char *stClass = (st==SYNC_OK?"ok":st==SYNC_SYNCING?"sync":st==SYNC_STALE?"stale":st==SYNC_OFFLINE?"off":"never");
  const char *stLabel = (st==SYNC_OK?"Synchron":st==SYNC_SYNCING?"Syncing ...":st==SYNC_STALE?"Stale":st==SYNC_OFFLINE?"Offline":"Nie sync.");

  String html;
  html.reserve(12000);

  html =
    "<!doctype html><html lang='de'><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>NTP-Uhr</title>"
    "<style>"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:system-ui,-apple-system,sans-serif;background:#0f172a;color:#cbd5e1;"
      "min-height:100vh;padding:16px 12px}"
    ".wrap{max-width:520px;margin:0 auto}"
    "header{display:flex;align-items:center;justify-content:space-between;padding:18px 0 14px}"
    "h1{font-size:20px;font-weight:700;color:#f1f5f9}"
    "h1 em{font-style:normal;color:#60a5fa}"
    ".ver{font-size:11px;color:#475569;margin-top:3px}"
    ".badge{padding:4px 12px;border-radius:99px;font-size:12px;font-weight:700;"
      "letter-spacing:.04em;border:1px solid transparent}"
    ".ok{background:#052e16;color:#4ade80;border-color:#166534}"
    ".sync{background:#0c1f3d;color:#60a5fa;border-color:#1d4ed8}"
    ".stale{background:#3b1400;color:#fb923c;border-color:#c2410c}"
    ".off{background:#1c0505;color:#f87171;border-color:#991b1b}"
    ".never{background:#18181b;color:#a1a1aa;border-color:#3f3f46}"
    ".tabs{display:flex;gap:4px;margin-bottom:14px;background:#1e293b;"
      "padding:5px;border-radius:12px;border:1px solid #2d3f55}"
    ".tab{flex:1;padding:8px 4px;border-radius:8px;font:inherit;font-size:13px;"
      "font-weight:600;cursor:pointer;border:0;background:transparent;"
      "color:#475569;transition:all .15s}"
    ".tab.active{background:#0f172a;color:#f1f5f9}"
    ".tab-panel{display:none}"
    ".card{background:#1e293b;border:1px solid #2d3f55;border-radius:14px;"
      "padding:16px;margin:10px 0}"
    ".card-hd{font-size:11px;font-weight:700;text-transform:uppercase;"
      "letter-spacing:.08em;color:#475569;margin-bottom:12px}"
    ".grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}"
    ".cell{background:#0f172a;border-radius:8px;padding:10px 12px}"
    ".cell.wide{grid-column:1/-1}"
    ".lbl{font-size:11px;color:#475569;margin-bottom:3px}"
    ".val{font-size:17px;font-weight:600;color:#f1f5f9}"
    ".val.sm{font-size:13px;word-break:break-all}"
    ".field{margin:10px 0}"
    ".field label{display:block;font-size:12px;font-weight:600;color:#64748b;margin-bottom:5px}"
    ".field input,.field select{width:100%;background:#0f172a;border:1px solid #2d3f55;"
      "border-radius:8px;padding:9px 12px;color:#f1f5f9;font:inherit;font-size:14px;"
      "outline:none;transition:border-color .15s}"
    ".field input:focus,.field select:focus{border-color:#3b82f6}"
    ".field input[type=file]{font-size:13px}"
    ".hint{font-size:11px;color:#334155;margin-top:8px;line-height:1.5}"
    "code{color:#60a5fa;font-size:11px}"
    ".actions{display:flex;gap:8px;margin-top:14px;flex-wrap:wrap;align-items:center}"
    ".btn{padding:9px 18px;border-radius:8px;font:inherit;font-size:14px;"
      "font-weight:600;cursor:pointer;border:0;transition:background .15s;"
      "text-decoration:none;display:inline-block}"
    ".btn-p{background:#3b82f6;color:#fff}"
    ".btn-p:hover{background:#2563eb}"
    ".btn-s{background:#1e293b;border:1px solid #2d3f55;color:#94a3b8}"
    ".btn-s:hover{background:#273549;color:#f1f5f9}"
    ".btn-d{background:#7f1d1d;border:1px solid #991b1b;color:#fca5a5}"
    ".btn-d:hover{background:#991b1b;color:#fff}"
    ".btn:disabled{opacity:.4;cursor:not-allowed}"
    ".ts{font-size:10px;color:#334155;margin-top:8px;text-align:right}"
    ".progress{display:none;margin-top:12px}"
    ".bar-bg{background:#2d3f55;border-radius:4px;height:8px;margin-top:8px}"
    ".bar{background:#3b82f6;height:8px;border-radius:4px;width:0%;transition:width .3s}"
    ".pct{font-size:14px;font-weight:600;color:#f1f5f9;margin-bottom:4px}"
    ".toast{position:fixed;bottom:24px;left:50%;transform:translateX(-50%) translateY(100px);"
      "background:#1e293b;border:1px solid #2d3f55;border-radius:10px;"
      "padding:10px 24px;font-size:14px;font-weight:600;"
      "transition:transform .25s;z-index:99;white-space:nowrap;pointer-events:none}"
    ".toast.ok{background:#052e16;color:#4ade80;border-color:#166534;"
      "transform:translateX(-50%) translateY(0)}"
    ".toast.err{background:#1c0505;color:#f87171;border-color:#991b1b;"
      "transform:translateX(-50%) translateY(0)}"
    ".toast.info{background:#0c1f3d;color:#60a5fa;border-color:#1d4ed8;"
      "transform:translateX(-50%) translateY(0)}"
    "</style></head><body>"
    "<div class='wrap'>"

    "<header>"
      "<div>"
        "<h1>ESP32 <em>PoE-Uhr</em></h1>"
        "<div class='ver'>Firmware " FW_VERSION_STR "</div>"
      "</div>"
      "<span class='badge ";
  html += stClass;
  html += "' id='sync-badge'>";
  html += stLabel;
  html +=
    "</span>"
    "</header>"

    "<div class='tabs'>"
      "<button class='tab' data-t='t-status'  onclick='showTab(\"t-status\")'>Status</button>"
      "<button class='tab' data-t='t-settings' onclick='showTab(\"t-settings\")'>Einstellungen</button>"
      "<button class='tab' data-t='t-system'  onclick='showTab(\"t-system\")'>System</button>"
    "</div>"

    // ===== Tab: Status =====
    "<div id='t-status' class='tab-panel'>"

    "<div class='card'>"
      "<div class='card-hd'>Netzwerk</div>"
      "<div class='grid'>"
        "<div class='cell wide'><div class='lbl'>IP-Adresse</div>"
          "<div class='val sm' id='v-ip'>";
  html += ip;
  html +=
        "</div></div>"
        "<div class='cell'><div class='lbl'>MAC-Adresse</div>"
          "<div class='val sm' id='v-mac'>";
  html += mac_s;
  html +=
        "</div></div>"
        "<div class='cell'><div class='lbl'>Link</div>"
          "<div class='val sm' id='v-link'>";
  html += link_s;
  html +=
        "</div></div>"
        "<div class='cell'><div class='lbl'>Duplex</div>"
          "<div class='val sm' id='v-duplex'>";
  html += duplex_s;
  html +=
        "</div></div>"
        "<div class='cell'><div class='lbl'>Gateway</div>"
          "<div class='val sm' id='v-gw'>";
  html += gw_s;
  html +=
        "</div></div>"
        "<div class='cell'><div class='lbl'>DNS-Server</div>"
          "<div class='val sm' id='v-dns'>";
  html += dns_s;
  html +=
        "</div></div>"
        "<div class='cell'><div class='lbl'>Netzwerkmodus</div>"
          "<div class='val sm' id='v-netmode'>";
  html += netmode_s;
  html +=
        "</div></div>"
        "<div class='cell'><div class='lbl'>Konfig. DNS</div>"
          "<div class='val sm' id='v-conf-dns'>";
  html += staticDnsEsc;
  html +=
        "</div></div>"
      "</div>"
    "</div>"

    "<div class='card'>"
      "<div class='card-hd'>NTP &amp; Zeitsynchronisation</div>"
      "<div class='grid'>"
        "<div class='cell'><div class='lbl'>Stratum</div>"
          "<div class='val' id='v-str'>";
  html += String((int)g_stratum);
  html +=
        "</div></div>"
        "<div class='cell'><div class='lbl'>RTT</div>"
          "<div class='val' id='v-rtt'>";
  html += rtt_s;
  html +=
        "</div></div>"
        "<div class='cell'><div class='lbl'>Stabilit&auml;t</div>"
          "<div class='val' id='v-stab'>";
  html += stab_s;
  html +=
        "</div></div>"
        "<div class='cell'><div class='lbl'>Intervall</div>"
          "<div class='val' id='v-iv'>";
  html += String(ntp_interval_ms / 60000) + " min";
  html +=
        "</div></div>"
        "<div class='cell wide'><div class='lbl'>Letzter Sync</div>"
          "<div class='val sm' id='v-sync'>";
  html += last;
  html +=
        "</div></div>"
      "</div>"
      "<div class='ts'>Aktualisiert: <span id='ts'>-</span></div>"
      "<div class='actions'>"
        "<button class='btn btn-s' id='btn-sync'>&#8635; Synchronisieren</button>"
      "</div>"
    "</div>"

    "<div class='card'>"
      "<div class='card-hd'>Display &amp; Helligkeit</div>"
      "<div class='grid'>"
        "<div class='cell'><div class='lbl'>Lux (VEML7700)</div>"
          "<div class='val' id='v-lux'>";
  html += lux_s;
  html +=
        "</div></div>"
        "<div class='cell'><div class='lbl'>Helligkeit</div>"
          "<div class='val' id='v-bl'>";
  html += bl_s;
  html +=
        "</div></div>"
        "<div class='cell wide'><div class='lbl'>Animation</div>"
          "<div class='val sm' id='v-anim'>";
  html += anim_s;
  html +=
        "</div></div>"
        "<div class='cell wide'><div class='lbl'>Uptime</div>"
          "<div class='val sm' id='v-up'>";
  html += uptime;
  html +=
        "</div></div>"
      "</div>"
    "</div>"

    "</div>" // end t-status

    // ===== Tab: Einstellungen =====
    "<div id='t-settings' class='tab-panel'>"
    "<form id='frm-settings'>"

    "<div class='card'>"
      "<div class='card-hd'>NTP-Server</div>"
      "<div class='field'><label>Server 1</label>"
        "<input name='s1' value='";
  html += ntp1Esc;
  html +=
        "' placeholder='0.de.pool.ntp.org'></div>"
      "<div class='field'><label>Server 2</label>"
        "<input name='s2' value='";
  html += ntp2Esc;
  html +=
        "' placeholder='1.de.pool.ntp.org'></div>"
      "<div class='field'><label>Server 3</label>"
        "<input name='s3' value='";
  html += ntp3Esc;
  html +=
        "' placeholder='2.de.pool.ntp.org'></div>"
      "<div class='field'><label>Abfrageintervall (Minuten)</label>"
        "<input name='iv' type='number' min='1' max='1440' value='";
  html += String(ntp_interval_ms / 60000);
  html +=
        "'></div>"
      "<div class='hint'>Ein Server gen&uuml;gt &ndash; leere Felder werden automatisch bef&uuml;llt.</div>"
    "</div>"

    "<div class='card'>"
      "<div class='card-hd'>Display-Animation</div>"
      "<div class='field'><label>Modus</label>"
        "<select name='anim'>"
          "<option value='0'";
  html += (g_anim_mode == ANIM_NONE      ? " selected" : "");
  html += ">Keine (Normal)</option>"
          "<option value='1'";
  html += (g_anim_mode == ANIM_SCROLL    ? " selected" : "");
  html += ">Vertical Scroll (von unten)</option>"
          "<option value='2'";
  html += (g_anim_mode == ANIM_CROSSFADE ? " selected" : "");
  html +=
        ">Crossfade (&Uuml;berblendung)</option>"
        "</select></div>"
    "</div>"

    "<div class='card'>"
      "<div class='card-hd'>Zeitzone</div>"
      "<div class='field'><label>UTC-Offset</label>"
        "<select name='tz_off'>"
          "<option value='-720'"; html += (g_tz_offset==-720?" selected":""); html += ">UTC-12</option>"
          "<option value='-660'"; html += (g_tz_offset==-660?" selected":""); html += ">UTC-11</option>"
          "<option value='-600'"; html += (g_tz_offset==-600?" selected":""); html += ">UTC-10</option>"
          "<option value='-540'"; html += (g_tz_offset==-540?" selected":""); html += ">UTC-9</option>"
          "<option value='-480'"; html += (g_tz_offset==-480?" selected":""); html += ">UTC-8</option>"
          "<option value='-420'"; html += (g_tz_offset==-420?" selected":""); html += ">UTC-7</option>"
          "<option value='-360'"; html += (g_tz_offset==-360?" selected":""); html += ">UTC-6</option>"
          "<option value='-300'"; html += (g_tz_offset==-300?" selected":""); html += ">UTC-5</option>"
          "<option value='-240'"; html += (g_tz_offset==-240?" selected":""); html += ">UTC-4</option>"
          "<option value='-180'"; html += (g_tz_offset==-180?" selected":""); html += ">UTC-3</option>"
          "<option value='-120'"; html += (g_tz_offset==-120?" selected":""); html += ">UTC-2</option>"
          "<option value='-60'";  html += (g_tz_offset==-60?" selected":"");  html += ">UTC-1</option>"
          "<option value='0'";    html += (g_tz_offset==0?" selected":"");    html += ">UTC±0</option>"
          "<option value='60'";   html += (g_tz_offset==60?" selected":"");   html += ">UTC+1</option>"
          "<option value='120'";  html += (g_tz_offset==120?" selected":"");  html += ">UTC+2</option>"
          "<option value='180'";  html += (g_tz_offset==180?" selected":"");  html += ">UTC+3</option>"
          "<option value='240'";  html += (g_tz_offset==240?" selected":"");  html += ">UTC+4</option>"
          "<option value='300'";  html += (g_tz_offset==300?" selected":"");  html += ">UTC+5</option>"
          "<option value='330'";  html += (g_tz_offset==330?" selected":"");  html += ">UTC+5:30</option>"
          "<option value='360'";  html += (g_tz_offset==360?" selected":"");  html += ">UTC+6</option>"
          "<option value='420'";  html += (g_tz_offset==420?" selected":"");  html += ">UTC+7</option>"
          "<option value='480'";  html += (g_tz_offset==480?" selected":"");  html += ">UTC+8</option>"
          "<option value='540'";  html += (g_tz_offset==540?" selected":"");  html += ">UTC+9</option>"
          "<option value='570'";  html += (g_tz_offset==570?" selected":"");  html += ">UTC+9:30</option>"
          "<option value='600'";  html += (g_tz_offset==600?" selected":"");  html += ">UTC+10</option>"
          "<option value='660'";  html += (g_tz_offset==660?" selected":"");  html += ">UTC+11</option>"
          "<option value='720'";  html += (g_tz_offset==720?" selected":"");  html += ">UTC+12</option>"
          "<option value='780'";  html += (g_tz_offset==780?" selected":"");  html += ">UTC+13</option>"
          "<option value='840'";  html += (g_tz_offset==840?" selected":"");  html += ">UTC+14</option>"
        "</select></div>"
      "<div class='field'><label>Sommerzeit</label>"
        "<select name='tz_dst' onchange='toggleDstFields()'>"
          "<option value='0'"; html += (g_tz_dst_mode==DST_OFF?" selected":"");    html += ">AUS</option>"
          "<option value='1'"; html += (g_tz_dst_mode==DST_EU?" selected":"");     html += ">EU (M3.5.0/2 – M10.5.0/3)</option>"
          "<option value='2'"; html += (g_tz_dst_mode==DST_US?" selected":"");     html += ">US (M3.2.0/2 – M11.1.0/2)</option>"
          "<option value='3'"; html += (g_tz_dst_mode==DST_CUSTOM?" selected":""); html += ">Benutzerdefiniert</option>"
        "</select></div>"
      "<div id='dst-custom' style='display:"; html += (g_tz_dst_mode==DST_CUSTOM?"block":"none"); html += "'>"
        "<div class='field'><label>Sommerzeit-Anfang (Regel)</label>"
          "<input name='tz_ds' value='";
  html += htmlEscape(String(g_tz_dst_start));
  html +=
          "' placeholder='M3.5.0/2'></div>"
        "<div class='field'><label>Sommerzeit-Ende (Regel)</label>"
          "<input name='tz_de' value='";
  html += htmlEscape(String(g_tz_dst_end));
  html +=
          "' placeholder='M10.5.0/3'></div>"
        "<div class='field'><label>Sommerzeit-Versatz (Minuten)</label>"
          "<input name='tz_dm' type='number' min='1' max='180' value='";
  html += String(g_tz_dst_minutes);
  html +=
          "'></div>"
      "</div>"
      "<div class='hint'>Zeitzonen&auml;nderungen werden nach dem Speichern sofort aktiv.</div>"
    "</div>"

    "<div class='actions'>"
      "<button class='btn btn-p' type='submit'>&#10003; Speichern</button>"
    "</div>"
    "</form>"

    // Netzwerk — eigenes Formular, da Änderung Neustart erfordert
    "<form id='frm-net'>"
    "<div class='card'>"
      "<div class='card-hd'>Netzwerk</div>"
      "<div class='field'><label>Modus</label>"
        "<select name='netmode' id='netmode' onchange='toggleStatic()'>"
          "<option value='1'";
  html += (g_use_dhcp ? " selected" : "");
  html +=
        ">DHCP (automatisch)</option>"
          "<option value='0'";
  html += (!g_use_dhcp ? " selected" : "");
  html +=
        ">Statische IP</option>"
        "</select></div>"
      "<div id='static-fields'>"
        "<div class='field'><label>IP-Adresse</label>"
          "<input name='ip' placeholder='192.168.1.100' value='";
  html += staticIpEsc;
  html +=
        "'></div>"
        "<div class='field'><label>Subnetzmaske</label>"
          "<input name='sn' placeholder='255.255.255.0' value='";
  html += staticSnEsc;
  html +=
        "'></div>"
        "<div class='field'><label>Gateway</label>"
          "<input name='gw' placeholder='192.168.1.1' value='";
  html += staticGwEsc;
  html +=
        "'></div>"
        "<div class='field'><label>DNS-Server</label>"
          "<input name='dns' placeholder='8.8.8.8' value='";
  html += staticDnsEsc;
  html +=
        "'></div>"
      "</div>"
      "<div class='hint'>Netzwerk&auml;nderungen werden nach einem Neustart aktiv.</div>"
      "<div class='actions'>"
        "<button class='btn btn-p' type='button' onclick='saveNet()'>"
          "&#10003; Speichern &amp; Neu starten</button>"
      "</div>"
    "</div>"
    "</form>"

    "</div>" // end t-settings

    // ===== Tab: System =====
    "<div id='t-system' class='tab-panel'>"

    "<div class='card'>"
      "<div class='card-hd'>Firmware</div>"
      "<div class='grid'>"
        "<div class='cell wide'><div class='lbl'>Version</div>"
          "<div class='val'>" FW_VERSION_STR "</div></div>"
        "<div class='cell wide'><div class='lbl'>Hostname</div>"
          "<div class='val sm'>esp32-ntp-uhr</div></div>"
      "</div>"
    "</div>"

    "<div class='card'>"
      "<div class='card-hd'>Systemdiagnose</div>"
      "<div class='grid'>"
        "<div class='cell wide'><div class='lbl'>Letzter Reset</div>"
          "<div class='val sm' id='v-reset'>";
  html += resetEsc;
  html +=
        "</div></div>"
        "<div class='cell wide'><div class='lbl'>NTP-Diagnose</div>"
          "<div class='val sm' id='v-ntpdiag'>";
  html += ntpDiagEsc;
  html +=
        "</div></div>"
        "<div class='cell wide'><div class='lbl'>Ethernet 100M</div>"
          "<div class='val sm' id='v-ethforce'>";
  html += ethForceEsc;
  html +=
        "</div></div>"
      "</div>"
    "</div>"

    "<div class='card'>"
      "<div class='card-hd'>Firmware-Update (OTA)</div>"
      "<div class='field'><label>Firmware-Datei (.bin)</label>"
        "<input id='ota-fi' type='file' accept='.bin'></div>"
      "<div class='actions'>"
        "<button class='btn btn-p' id='ota-btn' type='button' onclick='startOTA()'"
          ">&#8679; Flashen</button>"
      "</div>"
      "<div class='progress' id='ota-prog'>"
        "<div class='pct' id='ota-pct'>0%</div>"
        "<div class='bar-bg'><div class='bar' id='ota-bar'></div></div>"
      "</div>"
      "<div class='hint'>Build-Datei: "
        "<code>.pio/build/esp32-poe-iso/firmware.bin</code></div>"
    "</div>"

    "<div class='card'>"
      "<div class='card-hd'>Ger&auml;t</div>"
      "<div class='actions'>"
        "<button class='btn btn-d' id='btn-reboot'>&#9211; Neustart</button>"
      "</div>"
    "</div>"

    "<div class='card'>"
      "<div class='card-hd'>Passwort &auml;ndern</div>"
      "<form id='frm-passwd'>"
      "<div class='field'><label>Aktuelles Passwort</label>"
        "<input name='pw_old' type='password'></div>"
      "<div class='field'><label>Neues Passwort</label>"
        "<input name='pw_new' type='password'></div>"
      "<div class='field'><label>Neues Passwort (wiederholen)</label>"
        "<input name='pw_new2' type='password'></div>"
      "<div class='actions'>"
        "<button class='btn btn-p' type='submit'>&#10003; &Auml;ndern</button>"
      "</div>"
      "</form>"
      "<div class='hint'>Username: <strong>" + String(g_auth_user) + "</strong> (Default: admin / admin)</div>"
    "</div>"

    "<div class='card'>"
      "<div class='card-hd'>Recovery</div>"
      "<div class='hint'>Setzt NTP-, Anzeige- und Netzwerkeinstellungen auf die Firmware-Standardwerte zur&uuml;ck und startet neu.</div>"
      "<div class='actions'>"
        "<button class='btn btn-d' id='btn-factory' type='button' onclick='factoryReset()'>"
          "Werkseinstellungen</button>"
      "</div>"
    "</div>"

    "</div>" // end t-system

    "<div class='toast' id='toast'></div>"

    "<script>"
    // Tab switching
    "var TABS=['t-status','t-settings','t-system'];"
    "function showTab(id){"
      "TABS.forEach(function(t){"
        "document.getElementById(t).style.display=t===id?'block':'none';"
        "document.querySelector('[data-t=\"'+t+'\"]').classList.toggle('active',t===id);"
      "});"
      "history.replaceState(null,'','#'+id);"
    "}"
    "var _h=location.hash.slice(1);"
    "showTab(TABS.indexOf(_h)>=0?_h:'t-status');"

    // Toast
    "var _tt;"
    "function toast(msg,cls){"
      "clearTimeout(_tt);"
      "var e=document.getElementById('toast');"
      "e.textContent=msg;e.className='toast '+cls;"
      "_tt=setTimeout(function(){e.className='toast';},3000);"
    "}"

    // Uptime formatter
    "function fmt(s){"
      "var d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60),sc=s%60;"
      "var t=('0'+h).slice(-2)+':'+('0'+m).slice(-2)+':'+('0'+sc).slice(-2);"
      "return d>0?d+'d '+t:t;"
    "}"

    // AJAX save
    "document.getElementById('frm-settings').onsubmit=function(e){"
      "e.preventDefault();"
      "fetch('/save',{method:'POST',body:new FormData(this)})"
        ".then(function(r){return r.json();})"
        ".then(function(d){toast(d.ok?'Gespeichert!':'Fehler beim Speichern',d.ok?'ok':'err');if(d.ok)upd();})"
        ".catch(function(){toast('Verbindungsfehler','err');});"
    "};"

    // Sync
    "document.getElementById('btn-sync').onclick=function(){"
      "fetch('/sync',{method:'POST'})"
        ".then(function(r){return r.json();})"
        ".then(function(){toast('Synchronisation gestartet','info');})"
        ".catch(function(){toast('Fehler','err');});"
    "};"

    // Netzwerk: statische Felder ein-/ausblenden
    "function toggleStatic(){"
      "var v=document.getElementById('netmode').value;"
      "document.getElementById('static-fields').style.display=v==='0'?'block':'none';"
    "}"
    "toggleStatic();"

    // Zeitzone: benutzerdefinierte DST-Felder ein-/ausblenden
    "function toggleDstFields(){"
      "var v=document.querySelector('[name=tz_dst]').value;"
      "document.getElementById('dst-custom').style.display=v==='3'?'block':'none';"
    "}"

    // Netzwerk speichern (POST /savenet, dann Neustart serverseitig)
    "function saveNet(){"
      "if(!confirm('Netzwerkeinstellungen speichern und neu starten?'))return;"
      "fetch('/savenet',{method:'POST',body:new FormData(document.getElementById('frm-net'))})"
        ".then(function(r){return r.json();})"
        ".then(function(d){if(d.ok)toast('Neustart\u2026','info');})"
        ".catch(function(){toast('Fehler','err');});"
    "}"

    // Reboot
    "document.getElementById('btn-reboot').onclick=function(){"
      "if(!confirm('ESP32 wirklich neu starten?'))return;"
      "fetch('/reboot',{method:'POST'})"
        ".then(function(){toast('Neustart\u2026','info');})"
        ".catch(function(){});"
    "};"

    // Recovery reset
    "function factoryReset(){"
      "if(!confirm('Alle gespeicherten NTP-, Anzeige- und Netzwerkeinstellungen zuruecksetzen und neu starten?'))return;"
      "fetch('/factory-reset',{method:'POST'})"
        ".then(function(r){return r.json();})"
        ".then(function(d){toast(d.ok?'Werkseinstellungen gesetzt, Neustart\u2026':'Reset fehlgeschlagen',d.ok?'info':'err');})"
        ".catch(function(){toast('Fehler','err');});"
    "}"

    // Password change
    "document.getElementById('frm-passwd').onsubmit=function(e){"
      "e.preventDefault();"
      "var f=new FormData(this);"
      "if(f.get('pw_new')!==f.get('pw_new2')){toast('Passw\u00f6rter stimmen nicht \u00fcberein','err');return;}"
      "fetch('/change-password',{method:'POST',body:f})"
        ".then(function(r){return r.json();})"
        ".then(function(d){toast(d.ok?'Passwort ge\u00e4ndert!':d.msg||'Fehler',d.ok?'ok':'err');})"
        ".catch(function(){toast('Verbindungsfehler','err');});"
    "};"

    // OTA upload
    "function startOTA(){"
      "var fi=document.getElementById('ota-fi').files[0];"
      "if(!fi){toast('Keine Datei gew\u00e4hlt','err');return;}"
      "var fd=new FormData();fd.append('firmware',fi);"
      "var btn=document.getElementById('ota-btn');"
      "btn.disabled=true;"
      "document.getElementById('ota-prog').style.display='block';"
      "var xhr=new XMLHttpRequest();"
      "xhr.open('POST','/update');"
      "xhr.upload.onprogress=function(e){"
        "if(e.lengthComputable){"
          "var p=Math.round(e.loaded/e.total*100);"
          "document.getElementById('ota-pct').textContent=p+'%';"
          "document.getElementById('ota-bar').style.width=p+'%';"
        "}"
      "};"
      "xhr.onload=function(){"
        "if(xhr.status===200){"
          "document.getElementById('ota-pct').textContent='Fertig! Neustart\u2026';"
          "document.getElementById('ota-bar').style.background='#4ade80';"
          "toast('Firmware-Update erfolgreich!','ok');"
        "}else{"
          "document.getElementById('ota-pct').textContent='Fehler!';"
          "document.getElementById('ota-bar').style.background='#f87171';"
          "btn.disabled=false;"
          "toast('Flash fehlgeschlagen!','err');"
        "}"
      "};"
      "xhr.send(fd);"
    "}"

    // Status polling (3s)
    "var animN=['Keine','Scroll','Crossfade'];"
    "function upd(){"
      "fetch('/api/status')"
        ".then(function(r){return r.json();})"
        ".then(function(d){"
          "var e=function(i){return document.getElementById(i);};"
          "e('v-ip').textContent=d.ip;"
          "e('v-mac').textContent=d.mac||'-';"
          "e('v-link').textContent=d.link_mbps>0?d.link_mbps+' Mbps':'Kein Link';"
          "e('v-duplex').textContent=d.full_duplex?'Full':(d.link_mbps>0?'Half':'-');"
          "e('v-gw').textContent=d.gateway||'-';"
          "e('v-dns').textContent=d.dns||'-';"
          "e('v-netmode').textContent=d.netmode||'-';"
          "e('v-conf-dns').textContent=d.config_dns||'-';"
          "e('v-str').textContent=d.stratum;"
          "e('v-rtt').textContent=d.rtt_ms>=0?d.rtt_ms+' ms':'-';"
          "e('v-stab').textContent=d.stability_pct>=0?d.stability_pct+' %':'-';"
          "e('v-iv').textContent=d.interval_min+' min';"
          "e('v-sync').textContent=d.last_sync;"
          "e('v-lux').textContent=d.lux>=0?d.lux.toFixed(1)+' lx':'-';"
          "e('v-bl').textContent=d.backlight_pct+' %';"
          "e('v-up').textContent=fmt(d.uptime_s);"
          "e('v-anim').textContent=animN[d.anim]||'-';"
          "e('v-reset').textContent=d.reset||'-';"
          "e('v-ntpdiag').textContent=d.ntp_diag||'-';"
          "e('v-ethforce').textContent=d.eth_force||'-';"
          "var b=e('sync-badge');"
          "var lbl={ok:'Synchron',sync:'Syncing ...',stale:'Stale',off:'Offline',never:'Nie sync.'};"
          "b.textContent=lbl[d.sync_state]||d.sync_state;"
          "b.className='badge '+d.sync_state;"
          "e('ts').textContent=new Date().toLocaleTimeString('de-DE');"
        "})"
        ".catch(function(){});"
    "}"
    "upd();setInterval(upd,3000);"
    "</script>"

    "</div></body></html>";

  return html;
}

// ---------- Web-Handler ----------

static bool requireAuth() {
  if (server.authenticate(g_auth_user, g_auth_pass)) return true;
  server.requestAuthentication();
  return false;
}

void handleRoot() {
  if (!requireAuth()) return;
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html; charset=utf-8", htmlPage());
}

void handleSave() {
  if (!requireAuth()) return;
  auto norm = [](String s) -> String { s.trim(); return s; };
  if (server.hasArg("s1")) copyTrunc(ntp1, norm(server.arg("s1")).c_str());
  if (server.hasArg("s2")) copyTrunc(ntp2, norm(server.arg("s2")).c_str());
  if (server.hasArg("s3")) copyTrunc(ntp3, norm(server.arg("s3")).c_str());
  if (server.hasArg("iv")) {
    long m = server.arg("iv").toInt();
    ntp_interval_ms = (uint32_t)constrain(m, 1, 1440) * 60UL * 1000UL;
  }
  if (server.hasArg("anim")) {
    int m = server.arg("anim").toInt();
    if (m >= 0 && m <= 2) g_anim_mode = (AnimMode)m;
  }
  if (server.hasArg("tz_off")) {
    g_tz_offset = (int)server.arg("tz_off").toInt();
  }
  if (server.hasArg("tz_dst")) {
    int m = server.arg("tz_dst").toInt();
    if (m >= 0 && m <= 3) g_tz_dst_mode = (DstMode)m;
  }
  if (server.hasArg("tz_ds")) copyTrunc(g_tz_dst_start, server.arg("tz_ds").c_str());
  if (server.hasArg("tz_de")) copyTrunc(g_tz_dst_end,   server.arg("tz_de").c_str());
  if (server.hasArg("tz_dm")) {
    long m = server.arg("tz_dm").toInt();
    g_tz_dst_minutes = (int)constrain(m, 1, 180);
  }
  prefs.begin("ntp", false);
  prefs.putString("s1", ntp1); prefs.putString("s2", ntp2); prefs.putString("s3", ntp3);
  prefs.putULong("iv", ntp_interval_ms);
  prefs.putUChar("anim", (uint8_t)g_anim_mode);
  prefs.putInt("tz_off", g_tz_offset);
  prefs.putUChar("tz_dst", (uint8_t)g_tz_dst_mode);
  prefs.putString("tz_ds", g_tz_dst_start);
  prefs.putString("tz_de", g_tz_dst_end);
  prefs.putUShort("tz_dm", (uint16_t)g_tz_dst_minutes);
  prefs.end();
  applySNTP();
  char tz_str[64];
  buildTZString(tz_str, sizeof(tz_str));
  setenv("TZ", tz_str, 1); tzset();
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleSync() {
  if (!requireAuth()) return;
  have_synced = false; last_sync = 0; applySNTP();
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleReboot() {
  if (!requireAuth()) return;
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", "{\"ok\":true}");
  delay(200);
  ESP.restart();
}

void handleFactoryReset() {
  if (!requireAuth()) return;
  bool ok = true;
  ok = ok && prefs.begin("ntp", false);
  if (ok) ok = prefs.clear();
  prefs.end();

  ok = ok && prefs.begin("net", false);
  if (ok) ok = prefs.clear();
  prefs.end();

  server.sendHeader("Cache-Control", "no-cache");
  server.send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
  if (ok) {
    server.stop();
    delay(300);
    ESP.restart();
  }
}

void handleApiStatus() {
  SyncState st = currentSyncState();
  const char *stStr = (st==SYNC_OK?"ok":st==SYNC_SYNCING?"sync":st==SYNC_STALE?"stale":st==SYNC_OFFLINE?"off":"never");
  int sp = stabilityPercent();
  String lastStr = have_synced ? fmtLocal(last_sync) : String("noch nie");
  String mac_s = ETH.macAddress();
  String ip_s = eth_got_ip ? ETH.localIP().toString() : String("-");
  String gw_s = eth_got_ip ? ETH.gatewayIP().toString() : String("-");
  String dns_s = eth_got_ip ? ETH.dnsIP().toString() : String("-");
  String config_dns_json = jsonEscape(String(g_static_dns));
  String reset_json = jsonEscape(String(g_last_reset_reason));
  String ntp_diag_json = jsonEscape(String(g_last_ntp_status));
  String eth_force_json = jsonEscape(String(g_eth_force_status));
  char tz_built[64]; buildTZString(tz_built, sizeof(tz_built));
  String tz_json = jsonEscape(String(tz_built));
  char json[1550];
  snprintf(json, sizeof(json),
    "{\"ip\":\"%s\",\"mac\":\"%s\",\"link_mbps\":%d,"
    "\"full_duplex\":%s,"
    "\"gateway\":\"%s\",\"dns\":\"%s\",\"netmode\":\"%s\",\"config_dns\":\"%s\","
    "\"sync_state\":\"%s\",\"stratum\":%u,"
    "\"rtt_ms\":%d,\"stability_pct\":%d,\"interval_min\":%lu,"
    "\"last_sync\":\"%s\",\"lux\":%.1f,\"backlight_pct\":%d,"
    "\"uptime_s\":%lu,\"anim\":%d,\"fw\":\"%s\","
    "\"reset\":\"%s\",\"ntp_diag\":\"%s\",\"eth_force\":\"%s\","
    "\"tz_offset\":%d,\"tz_dst_mode\":%d,\"tz_dst_min\":%d,\"tz_str\":\"%s\"}",
    ip_s.c_str(),
    mac_s.c_str(),
    eth_got_ip ? (int)ETH.linkSpeed() : 0,
    eth_got_ip && ETH.fullDuplex() ? "true" : "false",
    gw_s.c_str(),
    dns_s.c_str(),
    g_use_dhcp ? "DHCP" : "Statisch",
    config_dns_json.c_str(),
    stStr, (unsigned)g_stratum,
    isnan(g_rtt_ms) ? -1 : (int)lround(g_rtt_ms), sp,
    (unsigned long)(ntp_interval_ms / 60000),
    lastStr.c_str(),
    isfinite(lux_hold) ? lux_hold : -1.0f,
    (int)((pwm_current * 100) / 255),
    millis() / 1000UL,
    (int)g_anim_mode,
    FW_VERSION_STR,
    reset_json.c_str(),
    ntp_diag_json.c_str(),
    eth_force_json.c_str(),
    g_tz_offset, (int)g_tz_dst_mode, g_tz_dst_minutes,
    tz_json.c_str());
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", json);
}

void handleSaveNet() {
  if (!requireAuth()) return;
  g_use_dhcp = !server.hasArg("netmode") || server.arg("netmode") == "1";
  if (server.hasArg("ip"))  copyTrunc(g_static_ip,  server.arg("ip").c_str());
  if (server.hasArg("gw"))  copyTrunc(g_static_gw,  server.arg("gw").c_str());
  if (server.hasArg("sn"))  copyTrunc(g_static_sn,  server.arg("sn").c_str());
  if (server.hasArg("dns")) copyTrunc(g_static_dns, server.arg("dns").c_str());
  prefs.begin("net", false);
  prefs.putBool("dhcp", g_use_dhcp);
  prefs.putString("ip",  g_static_ip);
  prefs.putString("gw",  g_static_gw);
  prefs.putString("sn",  g_static_sn);
  prefs.putString("dns", g_static_dns);
  prefs.end();
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", "{\"ok\":true}");
  delay(200);
  ESP.restart();
}

void handleChangePassword() {
  if (!requireAuth()) return;
  String oldPw = server.hasArg("pw_old") ? server.arg("pw_old") : "";
  String newPw = server.hasArg("pw_new") ? server.arg("pw_new") : "";

  if (strcmp(oldPw.c_str(), g_auth_pass) != 0) {
    server.sendHeader("Cache-Control", "no-cache");
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"Altes Passwort falsch\"}");
    return;
  }
  if (newPw.length() < 1 || newPw.length() > 31) {
    server.sendHeader("Cache-Control", "no-cache");
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"Passwort muss 1-31 Zeichen lang sein\"}");
    return;
  }
  copyTrunc(g_auth_pass, newPw.c_str());
  prefs.begin("ntp", false);
  prefs.putString("au", g_auth_user);
  prefs.putString("ap", g_auth_pass);
  prefs.end();
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleOTAPage() {
  if (!requireAuth()) return;
  // Weiterleitung auf Haupt-UI, System-Tab
  server.sendHeader("Location", "/#t-system");
  server.send(302);
}

void handleOTAUpload() {
  if (!requireAuth()) { return; }
  HTTPUpload &upload = server.upload();
  static size_t display_at = 0;
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("[OTA-Web] Start: %s\n", upload.filename.c_str());
    display_at = 32768;
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
    u8g2.clearBuffer();
    drawCenteredY(20, "Web OTA",     u8g2_font_8x13B_tf);
    drawCenteredY(36, "Empfange...", u8g2_font_6x12_tf);
    u8g2.drawFrame(4, 46, 120, 8);
    u8g2.sendBuffer();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
      Update.printError(Serial);
    if (upload.totalSize >= display_at) {
      display_at += 32768;
      char buf[20]; snprintf(buf, sizeof(buf), "%u KB", (unsigned)(upload.totalSize / 1024));
      u8g2.clearBuffer();
      drawCenteredY(20, "Web OTA", u8g2_font_8x13B_tf);
      drawCenteredY(36, buf,       u8g2_font_8x13B_tf);
      u8g2.drawFrame(4, 46, 120, 8);
      u8g2.sendBuffer();
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("[OTA-Web] OK: %u Bytes\n", upload.totalSize);
      u8g2.clearBuffer();
      drawCenteredY(28, "Update OK!", u8g2_font_8x13B_tf);
      drawCenteredY(44, "Neustart...", u8g2_font_6x12_tf);
      u8g2.sendBuffer();
    } else {
      Update.printError(Serial);
      u8g2.clearBuffer();
      drawCenteredY(28, "OTA Fehler!", u8g2_font_8x13B_tf);
      u8g2.sendBuffer();
    }
  }
}

void handleOTAPost() {
  if (!requireAuth()) { return; }
  server.sendHeader("Connection", "close");
  server.send(Update.hasError() ? 500 : 200, "text/plain",
              Update.hasError() ? "FEHLER" : "OK");
  if (!Update.hasError()) { delay(200); ESP.restart(); }
}
