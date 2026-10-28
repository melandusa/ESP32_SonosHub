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
- Bei mehr als 6 Favoriten zeigt die letzte Kachel „› Seite x/y“ und blättert weiter.
- `−` / `+`: Lautstärke in 3er-Schritten (gedrückt halten wiederholt), `AUS`: Stopp.
- Nach 2 min ohne Berührung wird gedimmt (in der Simulation nur als Log-Zeile `Backlight -> 15 %`).

Es werden nur Radio-Favoriten angezeigt (`x-sonosapi-stream:`, `x-sonosapi-radio:`,
`x-rincon-mp3radio:`, `aac:`, `hls-radio:`). Playlists, Alben, einzelne Titel und
Sonos-Radio-Shortcuts ohne Stream-URI werden ausgeblendet und geloggt.

## Architektur

```
core/                      portabel: nur C++17-Standardbibliothek, LVGL, tinyxml2
  platform/   Interfaces: IHttpClient, IDiscovery, IClock, IBacklight, ILogger (+ Log.h)
  sonos/      SoapBuilder, SoapParser, DidlParser, DeviceDescription, SonosClient
  app/        Commands, IWorker, SonosService (Worker-Seite), AppController (UI-Seite)
  ui/         MainScreen, Styles, Fonts
platform_linux/            CurlHttpClient, PosixSsdpDiscovery, ThreadWorker, MockSonos, main.cpp
tests/                     doctest; fixtures/ (Format-Beispiele) und fixtures/real/ (Mitschnitt Sonos Ray)
```

**Threading:** Die UI ruft nur `AppController` auf. Dieser schickt `Command`s an einen
`IWorker` und übernimmt die `Result`s in `tick()`, das ein `lv_timer` alle 30 ms im
UI-Thread aufruft. LVGL wird nur im UI-Thread angefasst. Der Worker (Linux: `std::thread`)
führt die Befehle blockierend über `SonosService` aus. Für den ESP32 wird `ThreadWorker`
durch einen FreeRTOS-Task mit Queue ersetzt, `core/` bleibt gleich.

**Verhalten:**
- Status (`GetTransportInfo`, `GetVolume`, `GetMediaInfo`) alle 2 s, Favoriten beim Start und alle 10 min.
- `GetMediaInfo` wird bei jedem Poll gelesen, nicht nur bei Statuswechsel: Ein Senderwechsel
  über die Sonos-App bleibt oft durchgehend bei `PLAYING` und würde sonst nicht erkannt.
- Lautstärke wird sofort angezeigt und höchstens alle 200 ms gesendet (letzter Wert gewinnt).
- Antworten auf Anfragen, die vor einer lokalen Änderung abgeschickt wurden, werden ignoriert
  (Sequenznummern), damit die Anzeige nicht zurückspringt.
- 3 Netzwerkfehler in Folge → „Lautsprecher nicht erreichbar – suche…“, neue Discovery
  nach 5 s, 10 s, 20 s, dann alle 30 s. SOAP-Fehler (z. B. „Stop“ im Stopp-Zustand) zählen nicht.
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
- Senderlogos (M7) sind vorbereitet (`Favorite::albumArtUri`, absolute URLs), aber nicht umgesetzt.
