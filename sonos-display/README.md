# Sonos-Radio-Display – Linux-Simulation

Einfache Sonos-Fernbedienung (Radiosender, Lautstärke, Aus) für ein 480×480-Touch-Display.
Diese Stufe läuft als SDL-Fenster unter Linux (Maus = Touch) und steuert einen echten
Sonos-Lautsprecher im Heimnetz. Der Ordner `core/` ist plattformunabhängig und wird später
unverändert auf den ESP32-S3 (Guition 4848S040) übernommen.

## Abhängigkeiten (Debian/Ubuntu)

```sh
sudo apt install build-essential cmake ninja-build pkg-config libsdl2-dev libcurl4-openssl-dev
```

CMake ≥ 3.25. LVGL 9.2.2, tinyxml2 11.0.0 und doctest 2.5.3 lädt CMake beim ersten
Konfigurieren per `FetchContent` (Internetzugang nötig).

## Bauen und testen

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure      # oder: ./build/tests/sonos_tests
```

## Starten

```sh
./build/sonos_display                 # Sonos per SSDP suchen
./build/sonos_display --ip 192.168.178.106
./build/sonos_display --mock          # simulierter Lautsprecher, kein Netzwerk nötig
```

| Parameter | Bedeutung |
|---|---|
| `--ip <adresse>` | Diesen Lautsprecher verwenden, keine Discovery |
| `--mock` | Simulierter Lautsprecher mit 8 Sendern (+ 1 Playlist, die ausgeblendet wird). Taste **`o`** im Fenster steckt ihn aus/ein. |
| `--max-volume <n>` | Lautstärke-Obergrenze 0..100 (Standard 60). Auch Werte, die über die Sonos-App höher gestellt werden, regelt das Display auf die Grenze herunter. |
| `--dump-xml` | Rohe XML-Antworten ins Log schreiben (für Test-Fixtures) |
| `--verbose` | Debug-Log |

## Bedienung

- Kachel antippen: Sender starten. Der laufende Sender ist gelb hervorgehoben.
- Senderlogos: Hat der Lautsprecher ein Logo zum Sender, zeigt die Kachel es (88 px) mit dem
  Namen einzeilig darunter, sonst nur den Namen.
- Bei mehr als 6 Favoriten zeigt die letzte Kachel „› Seite x/y“ und blättert weiter.
- `−` / `+`: Lautstärke in 1er-Schritten (gedrückt halten wiederholt), `AUS`: Stopp.
- Bildschirmschoner: nach 2 min ohne Berührung auf 15 % gedimmt, nach 10 min Hintergrundbeleuchtung aus.
  Im Aus-Zustand weckt die erste Berührung nur auf und löst nichts aus.
  (In der Simulation nur als Log-Zeile `Backlight -> …`.)

Es werden nur Radio-Favoriten angezeigt (`x-sonosapi-stream:`, `x-sonosapi-radio:`,
`x-rincon-mp3radio:`, `aac:`, `hls-radio:`). Playlists, Alben, einzelne Titel und
Sonos-Radio-Shortcuts ohne Stream-URI werden ausgeblendet und geloggt.

## ESP32-S3 (Guition 4848S040)

PlatformIO-Projekt im selben Ordner (`platformio.ini`), Quellen in `platform_esp32/`, dazu
`core/` und `lv_conf.h` unverändert. Plattform espressif32 8.0.0 (Arduino-Core 3.3),
LVGL 9.2.2, GFX Library for Arduino 1.6.9 (ST7701-RGB-Panel), WiFiManager 2.0.17, tinyxml2 11.0.0.

```sh
pio run                        # bauen (oder in VS Code: PlatformIO → Build)
pio run -t upload              # flashen
pio device monitor             # serielles Log, 115200 Baud
```

Beim ersten Start ohne gespeichertes WLAN öffnet das Display den Hotspot
**„SonosDisplay-Setup“**. Mit dem Handy verbinden, WLAN auswählen und Passwort eingeben.
Danach verbindet es sich bei jedem Start automatisch.

Lautsprecher-IP und Lautstärke-Limit sind vorerst Konstanten in `platform_esp32/main.cpp`
(`kFixedSpeakerIp`, `kMaxVolume`).

**Hardware-Stand:**
- Bildausrichtung: `kRotation = 3` (passt in den Halter; 0 = Panel-Grundausrichtung), Touch wird automatisch mitgedreht
- ~~Touch-Controller~~ geprüft: GT911 an 0x5D
- ~~Flackern~~ behoben durch Bounce-Buffer (`kLcdBounceBufferPx`)
- Hintergrundbeleuchtung per PWM auf GPIO 38 (Dimmen/Aus)

## Architektur

```
core/                      portabel: nur C++17-Standardbibliothek, LVGL, tinyxml2
  platform/   Interfaces: IHttpClient, IDiscovery, IClock, IBacklight, ILogger (+ Log.h)
  sonos/      SoapBuilder, SoapParser, DidlParser, DeviceDescription, SonosClient, Ssdp, LogoUrl
  app/        Commands, IWorker, SonosService (Worker-Seite), AppController (UI-Seite)
  ui/         MainScreen, Styles, Fonts, LogoDecoder
platform_linux/            CurlHttpClient, PosixSsdpDiscovery, ThreadWorker, MockSonos, main.cpp
platform_esp32/            EspHttpClient, WifiUdpSsdpDiscovery, FreeRtosWorker, Display, Gt911Touch, main.cpp
tests/                     doctest; fixtures/ (Format-Beispiele) und fixtures/real/ (Mitschnitt Sonos Ray)
```

**Threading:** Die UI ruft nur `AppController` auf. Dieser schickt `Command`s an einen
`IWorker` und übernimmt die `Result`s in `tick()`, das ein `lv_timer` alle 30 ms im
UI-Thread aufruft. LVGL wird nur im UI-Thread angefasst. Der Worker (Linux: `std::thread`)
führt die Befehle blockierend über `SonosService` aus. Auf dem ESP32 übernimmt
`FreeRtosWorker` (Task auf Core 0, Queues mit Zeigern) diese Rolle, `core/` bleibt gleich.

**Verhalten:**
- Status (`GetTransportInfo`, `GetVolume`, `GetMediaInfo`) alle 2 s, Favoriten beim Start und alle 10 min.
- `GetMediaInfo` wird bei jedem Poll gelesen, nicht nur bei Statuswechsel: Ein Senderwechsel
  über die Sonos-App bleibt oft durchgehend bei `PLAYING` und würde sonst nicht erkannt.
- Lautstärke wird sofort angezeigt und höchstens alle 200 ms gesendet (letzter Wert gewinnt).
- Antworten auf Anfragen, die vor einer lokalen Änderung abgeschickt wurden, werden ignoriert
  (Sequenznummern), damit die Anzeige nicht zurückspringt.
- 3 Netzwerkfehler in Folge → „Lautsprecher nicht erreichbar – suche…“, neue Discovery
  nach 5 s, 10 s, 20 s, dann alle 30 s. SOAP-Fehler (z. B. „Stop“ im Stopp-Zustand) zählen nicht.
- Senderlogos kommen vom Bild-Proxy des Lautsprechers (`http://<ip>:1400/getaa?s=1&u=<Stream-URI>`,
  bei TuneIn 600×600-PNG). Die `albumArtURI`s der Favoriten leiten meist nur per HTTPS auf dasselbe
  Bild weiter, der Proxy braucht kein TLS. Der Worker lädt ein Logo nach dem anderen und nur, wenn
  sonst nichts ansteht, dekodiert es mit LVGLs lodepng und verkleinert es auf 88×88 (ARGB8888).
  Fehlt ein Logo (404) oder ist es kein PNG (z. B. progressives JPEG), bleibt die Text-Kachel.
  Netzwerkfehler beim Logo zählen nicht als Lautsprecher-Ausfall und werden nach 1 min wiederholt.
- Discovery prüft jedes gefundene Gerät mit `GetTransportInfo`. Surround-Lautsprecher und Subs
  eines Heimkino-Sets antworten mit HTTP 500 und werden übersprungen.

**Schrift:** Die eingebauten LVGL-Fonts haben keine Umlaute. Die UI nutzt deshalb LVGLs
`Montserrat-Medium.ttf` (beim Build als C-Array eingebettet, ca. 240 KB) mit dem Tiny-TTF-Renderer.
Symbole (▶ ■ ♪ …) kommen aus der eingebauten Montserrat als Fallback.
Für den ESP32 kann man später alternativ mit `lv_font_conv` vorgerenderte Fonts erzeugen.

## Portabilität prüfen

```sh
grep -rnE '#include\s*[<"](unistd|pthread|sys/|netinet/|arpa/|poll\.h|curl/|SDL|thread>|mutex>)' core/
```

muss leer sein.

## Bekannte Grenzen / TODO

- Gruppen: Befehle gehen an das gewählte Gerät, nicht an einen Gruppen-Koordinator (`TODO(groups)` in `SonosClient.h` und `SonosService.cpp`).
- Bei mehreren steuerbaren Lautsprechern wird der erste genommen (nach einer Wiederverbindung bevorzugt der bisherige).
- Senderlogos nur als PNG; JPEG-Logos (z. B. Sonos-Radio-Sender, Amazon) bleiben Text-Kacheln.
- Ein Logo zu laden dauert auf dem ESP32 1,5–2,5 s. Ein Tipp in dieser Zeit (nur kurz nach dem
  Start oder nach geänderten Favoriten) wartet, bis das laufende Logo fertig ist.
