# Sonos-Radio-Display – Schritt 1: Simulation unter Linux

Umsetzungsauftrag für Claude Code. Lies dieses Dokument vollständig, bevor du Code schreibst.
Bei größeren Abweichungen von Architektur oder Scope erst nachfragen.

---

## 1. Ziel

Eine einfache Sonos-Fernbedienung für ältere Menschen (Radiosender starten, Lautstärke, Aus).
Endgerät später: **ESP32-S3 Guition 4848S040** (4", 480×480, kapazitiver Touch, C++ / LVGL).

**Dieser Schritt:** Die komplette App läuft als **Simulation auf Linux** in einem 480×480-SDL-Fenster
(Maus = Touch) und steuert einen **echten Sonos-Lautsprecher im Heimnetz**.
Der plattformunabhängige Kern wird später unverändert auf den ESP32 übernommen.

## 2. Scope

**Im Scope**
- Sonos im Netz finden (SSDP) oder feste IP per Parameter
- Sonos-Favoriten automatisch laden (Fokus: Radiosender)
- Favorit per Kachel starten
- Lautstärke +/−, Aus (Stop)
- Anzeige: aktueller Sender, Wiedergabestatus, Lautstärke
- Periodisches Aktualisieren von Status und Favoriten
- Mock-Modus ohne Netzwerk für UI-Entwicklung
- Unit-Tests für SOAP-Aufbau und XML-Parsing

**Nicht im Scope (spätere Schritte)**
- ESP32-Build, WLAN-Setup/Hotspot, OTA, Gehäuse
- Gruppierung, mehrere Räume, Warteschlange, Playlists/Alben (Container-Favoriten)
- Senderlogos (optionaler Meilenstein M7, nur wenn Zeit ist)

## 3. Rahmenbedingungen

- **Sprache:** C++17. Code, Bezeichner, Kommentare auf Englisch; **alle UI-Texte auf Deutsch**.
- **GUI:** LVGL 9.x mit eingebautem SDL2-Treiber (`LV_USE_SDL`).
- **Build Linux:** CMake. Abhängigkeiten: SDL2, libcurl, tinyxml2 (per FetchContent oder System-Paket), LVGL per FetchContent.
- **Tests:** doctest oder GoogleTest (per FetchContent).
- **Portabilität:** Der Ordner `core/` darf **keine** Linux-, POSIX-, curl- oder SDL-Header einbinden.
  Erlaubt: C++-Standardbibliothek, LVGL, tinyxml2. Kein `std::thread` in `core/`.
- **Speicher:** Sparsam bleiben (ESP32-S3 mit 8 MB PSRAM). Keine großen Puffer, keine Exceptions im Kern nötig.
- Zielsystem des Entwicklers: Linux-Desktop, Sonos im selben LAN.

## 4. Architektur

```
┌──────────────────────── core/ (portabel) ────────────────────────┐
│  ui/            LVGL-Screens, nur im UI-Thread                    │
│  app/           AppController: Zustand, Befehle, Polling-Logik   │
│  sonos/         SonosClient: SOAP-Envelopes bauen, Antworten     │
│                 parsen (tinyxml2), DIDL-Lite-Parser              │
│  platform/      Interfaces: IHttpClient, IDiscovery, ILogger,    │
│                 IClock                                            │
└──────────────────────────────────────────────────────────────────┘
          ▲ implementiert von
┌──────── platform_linux/ ────────┐   ┌─── platform_esp32/ (später) ───┐
│ CurlHttpClient                  │   │ HTTPClient, WiFiUDP, FreeRTOS  │
│ PosixSsdpDiscovery (UDP)        │   └────────────────────────────────┘
│ Worker-Thread + Queue           │
│ main.cpp (SDL, Argumente)       │
└─────────────────────────────────┘
```

**Threading-Regel:** Netzwerkaufrufe blockieren nie die UI.
- Die UI schickt Befehle (`PlayFavorite`, `VolumeUp`, …) in eine Befehls-Queue.
- Ein Worker (Linux: `std::thread` in `platform_linux/`) führt sie aus und liefert Ergebnisse zurück.
- Ergebnisse werden im UI-Thread übernommen (z. B. in einem `lv_timer`), LVGL wird nur dort angefasst.
- Die Queue-/Worker-Abstraktion so schneiden, dass sie später durch einen FreeRTOS-Task ersetzbar ist.

**Interfaces (Vorschlag, Details frei):**
```cpp
struct HttpResponse { int status; std::string body; };
class IHttpClient {
public:
  virtual ~IHttpClient() = default;
  virtual HttpResponse post(const std::string& url,
                            const std::vector<std::pair<std::string,std::string>>& headers,
                            const std::string& body, int timeoutMs) = 0;
  virtual HttpResponse get(const std::string& url, int timeoutMs) = 0;
};
class IDiscovery {
public:
  virtual ~IDiscovery() = default;
  // Liefert IPs gefundener ZonePlayer, Timeout in ms
  virtual std::vector<std::string> discover(int timeoutMs) = 0;
};
```

## 5. Sonos-Schnittstelle (lokal, UPnP/SOAP, Port 1400)

Alle Aufrufe: `POST http://<ip>:1400<controlPath>`
Header: `Content-Type: text/xml; charset="utf-8"`, `SOAPACTION: "<serviceType>#<Action>"`

Envelope:
```xml
<?xml version="1.0" encoding="utf-8"?>
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"
            s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
  <s:Body>
    <u:ACTION xmlns:u="SERVICE_TYPE"> ...Argumente... </u:ACTION>
  </s:Body>
</s:Envelope>
```
Argumentwerte müssen XML-escaped werden (`& < > " '`).

### 5.1 Discovery (SSDP)
- UDP-Multicast an `239.255.255.250:1900`:
  ```
  M-SEARCH * HTTP/1.1
  HOST: 239.255.255.250:1900
  MAN: "ssdp:discover"
  MX: 1
  ST: urn:schemas-upnp-org:device:ZonePlayer:1
  ```
- Antworten auswerten: Header `LOCATION: http://<ip>:1400/xml/device_description.xml` → IP extrahieren.
- Optional: Raumname aus `device_description.xml` (`roomName`) lesen und anzeigen.
- Mehrere Geräte gefunden: erstes nehmen, Liste loggen (Mehrraum ist nicht im Scope).
- Fallback: Parameter `--ip <adresse>` überspringt die Discovery.

### 5.2 Benötigte Aktionen

| Zweck | Control-Pfad | Service-Type | Action | Argumente |
|---|---|---|---|---|
| Favoriten laden | `/MediaServer/ContentDirectory/Control` | `urn:schemas-upnp-org:service:ContentDirectory:1` | `Browse` | `ObjectID=FV:2`, `BrowseFlag=BrowseDirectChildren`, `Filter=dc:title,res,upnp:albumArtURI`, `StartingIndex=0`, `RequestedCount=100`, `SortCriteria=` |
| Sender setzen | `/MediaRenderer/AVTransport/Control` | `urn:schemas-upnp-org:service:AVTransport:1` | `SetAVTransportURI` | `InstanceID=0`, `CurrentURI=<res>`, `CurrentURIMetaData=<resMD>` |
| Abspielen | wie oben | wie oben | `Play` | `InstanceID=0`, `Speed=1` |
| Aus | wie oben | wie oben | `Stop` | `InstanceID=0` |
| Status | wie oben | wie oben | `GetTransportInfo` | `InstanceID=0` → `CurrentTransportState` |
| Aktueller Sender | wie oben | wie oben | `GetMediaInfo` | `InstanceID=0` → `CurrentURI`, `CurrentURIMetaData` (darin `dc:title`) |
| Lautstärke lesen | `/MediaRenderer/RenderingControl/Control` | `urn:schemas-upnp-org:service:RenderingControl:1` | `GetVolume` | `InstanceID=0`, `Channel=Master` → `CurrentVolume` |
| Lautstärke setzen | wie oben | wie oben | `SetVolume` | `InstanceID=0`, `Channel=Master`, `DesiredVolume=0..100` |

### 5.3 Favoriten parsen (Achtung: doppelt escaped)
- Die `Browse`-Antwort enthält `<Result>` mit **escaptem DIDL-Lite-XML** → Text auslesen (tinyxml2 dekodiert einmal) und erneut parsen.
- Pro `<item>`:
  - `dc:title` → Kachelbeschriftung
  - `res` → `CurrentURI` für `SetAVTransportURI`
  - `r:resMD` → enthält wiederum escaptes DIDL; **dekodierter Text** wird als `CurrentURIMetaData` gesendet (beim Einbetten in den SOAP-Body wieder escapen)
  - `upnp:albumArtURI` → für M7; relative Pfade mit `http://<ip>:1400` ergänzen
- **Radio-Erkennung:** `res` beginnt mit `x-sonosapi-stream:`, `x-sonosapi-radio:`, `x-rincon-mp3radio:`, `aac:` oder `hls-radio:`.
  Favoriten mit `x-rincon-cpcontainer:` o. Ä. (Playlists/Alben) **im MVP ausblenden** und loggen.
- Lege echte Beispielantworten als Test-Fixtures unter `tests/fixtures/` ab (der Entwickler kann reale Antworten per Log-Option `--dump-xml` mitschneiden).

### 5.4 Polling und Verhalten
- Alle 2 s: `GetTransportInfo`, `GetVolume`, bei Änderung `GetMediaInfo`.
- Favoriten: beim Start und alle 10 min neu laden.
- Lautstärke: Schrittweite 3; Tastendruck ändert die Anzeige sofort (optimistisch), Senden entprellt (max. 1 Request pro 200 ms, letzter Wert gewinnt).
- Obergrenze Lautstärke konfigurierbar (Standard 60), damit niemand versehentlich voll aufdreht.
- Timeout pro Request 2 s. Nach 3 Fehlern in Folge: Status „Lautsprecher nicht erreichbar“ und Discovery neu starten (Backoff 5 s → 30 s).
- Bei Gruppen müssten Befehle an den Koordinator – im MVP nicht behandeln, nur im Code als TODO markieren.

## 6. Oberfläche (480 × 480)

```
┌──────────────────────────────────────────┐
│  ♪ SWR1 Baden-Württemberg      ▶ läuft   │  Kopfzeile, ca. 70 px
├──────────────────────────────────────────┤
│ ┌────────────┐┌────────────┐┌──────────┐ │
│ │   SWR1     ││  SWR3      ││ Antenne  │ │
│ └────────────┘└────────────┘└──────────┘ │  Kachel-Raster 3 × 2
│ ┌────────────┐┌────────────┐┌──────────┐ │  ca. 290 px
│ │  DLF       ││  hr1       ││  ‹  ›    │ │
│ └────────────┘└────────────┘└──────────┘ │
├──────────────────────────────────────────┤
│   [  −  ]      [  AUS  ]      [  +  ]    │  Bedienleiste, ca. 120 px
│   ███████████░░░░░░░░  Lautstärke 35     │
└──────────────────────────────────────────┘
```

- Max. 6 Kacheln pro Seite; bei mehr Favoriten Blättern über eine Pfeil-Kachel (Seite x/y).
- Laufender Sender: Kachel farbig hervorgehoben.
- Schrift mindestens 24 px auf Kacheln, 28 px Kopfzeile; deutsche Umlaute müssen in der Schrift enthalten sein (LVGL-Font mit Latin-1 erzeugen oder Montserrat mit passendem Zeichenbereich).
- Hoher Kontrast, dunkler Hintergrund, große Trefferflächen, keine Wischgesten, keine Untermenüs.
- Statusanzeigen: „läuft“, „gestoppt“, „verbindet…“, „Lautsprecher nicht erreichbar – suche…“.
- Feedback bei Tippen: Kachel kurz eingedrückt darstellen.
- Auto-Dimmen vorbereiten: Interface `IBacklight` mit `setLevel(0..100)`; Linux-Implementierung loggt nur. Logik: nach 2 min ohne Berührung dimmen, bei Berührung hell.

## 7. Projektstruktur (Vorschlag)

```
sonos-display/
├─ CMakeLists.txt
├─ README.md                 Build, Start, Parameter
├─ lv_conf.h
├─ core/
│  ├─ app/        AppController, Commands, State
│  ├─ sonos/      SonosClient, SoapBuilder, DidlParser, Favorite
│  ├─ ui/         MainScreen, Styles, Fonts
│  └─ platform/   Interfaces
├─ platform_linux/
│  ├─ main.cpp
│  ├─ CurlHttpClient.*
│  ├─ PosixSsdpDiscovery.*
│  ├─ Worker.*
│  └─ MockSonos.*   (Mock-Modus)
└─ tests/
   ├─ fixtures/     echte Sonos-XML-Antworten
   ├─ test_soap.cpp
   └─ test_didl.cpp
```

Startparameter: `--ip <adresse>`, `--mock`, `--max-volume <n>`, `--dump-xml`, `--verbose`.

## 8. Meilensteine

Nach jedem Meilenstein: bauen, Tests laufen lassen, kurz zusammenfassen, **Git-Commit-Nachricht vorschlagen**.

| # | Inhalt | Fertig, wenn … |
|---|---|---|
| M1 | Projektgerüst: CMake, LVGL + SDL-Fenster 480×480, leerer Screen | Fenster öffnet sich, Mausklick wird als Touch erkannt |
| M2 | UI mit Mock-Daten (`--mock`): Kopfzeile, 6 Kacheln, Bedienleiste, Blättern | Bedienung komplett ohne Netzwerk klickbar |
| M3 | SoapBuilder, DidlParser, Unit-Tests mit Fixtures | Tests grün, inkl. doppelt escaptem `resMD` |
| M4 | Linux-Plattform: CurlHttpClient, SSDP, Worker/Queue | Sonos wird gefunden, Favoriten werden geloggt |
| M5 | Integration: Kacheln = echte Favoriten, Play/Stop, Lautstärke | Radio startet am echten Sonos per Klick |
| M6 | Polling, Fehlerzustände, Wiederverbindung, Lautstärke-Limit, Dim-Logik | Sonos ausstecken/einstecken → App erholt sich ohne Neustart |
| M7 (optional) | Senderlogos laden und auf Kacheln anzeigen | Logos sichtbar, Fallback auf Text |

## 9. Abnahmekriterien Schritt 1

- [ ] `cmake --build` baut ohne Warnungen auf aktuellem Linux (gcc/clang)
- [ ] Sonos wird automatisch gefunden; `--ip` funktioniert als Alternative
- [ ] Favoriten aus der Sonos-App erscheinen als Kacheln; neue Favoriten erscheinen nach spätestens 10 min oder Neustart
- [ ] Antippen einer Kachel startet den Sender innerhalb von ca. 2 s
- [ ] +/− und AUS funktionieren; Lautstärke übersteigt nie das Limit
- [ ] Änderungen über die Sonos-App (anderer Sender, Lautstärke) erscheinen innerhalb von ca. 2 s im Display
- [ ] UI friert bei Netzwerkfehlern nicht ein
- [ ] `core/` enthält keine Linux-/POSIX-/curl-/SDL-Abhängigkeiten (per Grep prüfbar)
- [ ] Unit-Tests grün
- [ ] README beschreibt Abhängigkeiten (Paketnamen für Debian/Ubuntu), Build und Start

## 10. Ausblick Schritt 2 (nur zur Orientierung, nicht umsetzen)

PlatformIO-Projekt für den Guition ESP32-S3-4848S040 (ST7701-Display über RGB-Bus mit SPI-Init, GT911-Touch),
`platform_esp32/` mit HTTPClient, WiFiUDP-SSDP, FreeRTOS-Task statt Worker-Thread, WiFiManager für die WLAN-Einrichtung, PWM-Backlight.
