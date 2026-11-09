# Sonos-Radio-Display – Linux-Simulation

Einfache Sonos-Fernbedienung (Radiosender, Lautstärke, Aus) für ein 480×480-Touch-Display.
Diese Stufe läuft als SDL-Fenster unter Linux (Maus = Touch) und steuert einen echten
Sonos-Lautsprecher im Heimnetz. Der Ordner `core/` ist plattformunabhängig und wird später
unverändert auf den ESP32-S3 (Guition 4848S040) übernommen.

## Abhängigkeiten (Debian/Ubuntu)

```sh
sudo apt install build-essential cmake ninja-build pkg-config libsdl2-dev libcurl4-openssl-dev libssl-dev
```

CMake ≥ 3.25. LVGL 9.2.2, tinyxml2 11.0.0, ArduinoJson 7.4.2 und doctest 2.5.3 lädt CMake beim ersten
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
| `--hcu <adresse>` | Homematic IP Home Control Unit (IP oder `hcu1-XXXX.local`): Seite „Haus“ mit Steckdosen und Rollläden |
| `--hcu-pair <schlüssel>` | Mit `--hcu`: einmalig koppeln (Aktivierungsschlüssel aus HCUweb), Token wird gespeichert |
| `--hcu-token-file <datei>` | Token-Datei für `--hcu` (Standard `hcu_token.txt`) |
| `--hcu-mock` | Simulierte HCU (3 Rollläden, 4 Steckdosen). Taste **`h`** steckt sie aus/ein. |

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

## Homematic IP (Steckdosen und Rollläden)

Mit einer **Homematic IP Home Control Unit (HCU)** zeigt die Kopfzeile rechts eine Taste
**„⌂ Haus“** bzw. **„♪ Musik“**, die zwischen den Seiten wechselt. Die Haus-Seite zeigt alle
Schaltaktoren/Steckdosen und Rollläden/Jalousien/Markisen der HCU als Kacheln (2×3, nach Namen
sortiert, ab 7 Geräten mit „› Seite x/y“):

- **Steckdose:** Kachel antippen schaltet um. Eingeschaltet = gelb, „AN“/„AUS“ darunter.
- **Rollladen:** Tasten ▲ (ganz auf), ■ (Stopp), ▼ (ganz zu). Darunter „offen“, „zu“,
  „40 % zu“ oder „fährt…“.
- Nicht erreichbare Geräte (Funkstörung, Batterie) und die ganze Seite bei getrennter HCU
  werden ausgegraut. Der Bildschirmschoner kehrt immer zur Musik-Seite zurück.

Angesteuert wird die HCU lokal über die offizielle **Homematic IP Connect API** (WebSocket auf
Port 9001, ohne Cloud). Geräte-Kanäle: `SWITCH_CHANNEL`, `SWITCH_MEASURING_CHANNEL`
(`setSwitchState`), `SHUTTER_CHANNEL`, `BLIND_CHANNEL` (`setShutterLevel`), `SHADING_CHANNEL`
(`setPrimaryShadingLevel`), Stopp mit `stop`. Änderungen, auch aus der App, kommen als
Push-Events sofort an.

**Einmalig in HCUweb** (`https://hcu1-XXXX.local`, XXXX = letzte 4 Stellen der SGTIN auf der
Unterseite):

1. Entwicklermodus aktivieren.
2. „Connect API WebSocket freigeben“ einschalten.
3. Einen **Aktivierungsschlüssel** erzeugen. Er gilt nur einmal und nur kurze Zeit.

Danach am Display (siehe Einrichtung unten) bzw. in der Simulation mit `--hcu <adresse> --hcu-pair
<schlüssel>` koppeln. Das Display erscheint dann in der Homematic IP App unter den Benutzern
(dort kann man es auch wieder entfernen) und in HCUweb als Plugin „Sonos-Display“.
Hinweis von eQ-3: Ist einmal ein Connect-API-Client angelegt, lässt sich der Entwicklermodus
nicht mehr abschalten.

Die TLS-Verbindung zur HCU prüft das Zertifikat nicht (die HCU nutzt ein selbst signiertes).
Das Token wird nur im eigenen Netz gesendet.

## ESP32-S3 (Guition 4848S040)

PlatformIO-Projekt im selben Ordner (`platformio.ini`), Quellen in `platform_esp32/`, dazu
`core/` und `lv_conf.h` unverändert. Plattform espressif32 8.0.0 (Arduino-Core 3.3),
LVGL 9.2.2, GFX Library for Arduino 1.6.9 (ST7701-RGB-Panel), WiFiManager 2.0.17, tinyxml2 11.0.0,
ArduinoJson 7.4.2.

```sh
pio run                        # bauen (oder in VS Code: PlatformIO → Build)
pio run -t upload              # flashen
pio device monitor             # serielles Log, 115200 Baud
```

### Einrichtung (WLAN, Lautsprecher, Lautstärke-Grenze)

Findet das Display beim Start kein gespeichertes WLAN (oder ist es nach 15 s nicht erreichbar),
öffnet es den Hotspot **„SonosDisplay-Setup“**. Mit dem Handy verbinden, die Seite öffnet sich
meist von selbst (sonst `192.168.4.1`). Unter „Configure WiFi“ WLAN und Passwort eingeben,
darunter optional:

- **Lautsprecher-IP**: leer lassen = automatisch suchen
- **Maximale Lautstärke** (1–100, Standard 60)
- **Homematic IP HCU**: IP-Adresse oder `hcu1-XXXX.local`, leer = ohne Haus-Seite
- **HCU-Aktivierungsschlüssel**: nur zum Koppeln. Beim nächsten Start tauscht das Display ihn
  gegen ein Token (Anzeige „Homematic IP gekoppelt“ oder die Fehlermeldung). Für eine geänderte
  HCU-Adresse ist ein neuer Schlüssel nötig.

Um das bei einem schon eingerichteten Display nachzutragen: beim Einschalten 5 s auf das Display
drücken (siehe unten), dann das WLAN-Passwort erneut eingeben und die HCU-Felder ausfüllen.

Nach dem Speichern verbindet sich das Display und startet einmal neu. Die Einstellungen liegen
im NVS und bleiben bei Updates erhalten.

**WLAN neu einrichten am Gerät:** Beim Einschalten den Finger auf das Display legen und
5 s liegen lassen (Countdown oben). Dann sind die WLAN-Daten gelöscht und der Hotspot öffnet sich.

### Firmware-Updates per WLAN

Das Display prüft 2 min nach dem Start und danach täglich um 3 Uhr
`https://github.com/melandusa/ESP32_SonosHub/releases/latest/download/version.json`.
Ist dort eine höhere Versionsnummer, installiert es `firmware.bin`, sobald niemand das Display
nutzt (Bildschirm aus, keine Musik), und startet neu. HTTPS mit Zertifikatsprüfung
(Mozilla-Stammzertifikate in `platform_esp32/certs/`).

Sicherheit: Die neue Firmware landet im zweiten App-Bereich des Flash, die laufende bleibt
unangetastet. Erreicht die neue Firmware nach dem Neustart nicht innerhalb von 5 min das WLAN
(oder stürzt sie vorher ab), startet der Bootloader wieder die alte.

**Neue Version veröffentlichen:**

```sh
echo 2 > VERSION                       # Versionsnummer erhöhen
git commit -am "Version 2"
git tag v2 && git push && git push origin v2
```

Der Workflow `.github/workflows/release.yml` baut die Firmware und legt das Release mit
`firmware.bin` und `version.json` an (Tag und `VERSION` müssen übereinstimmen).
Die erste Firmware mit Update-Funktion muss einmal per USB geflasht werden.

**Hardware-Stand:**
- Bildausrichtung: `kRotation = 3` (passt in den Halter; 0 = Panel-Grundausrichtung), Touch wird automatisch mitgedreht
- ~~Touch-Controller~~ geprüft: GT911 an 0x5D
- ~~Flackern~~ behoben durch Bounce-Buffer (`kLcdBounceBufferPx`)
- Hintergrundbeleuchtung per PWM auf GPIO 38 (Dimmen/Aus)

## Architektur

```
core/                      portabel: nur C++17-Standardbibliothek, LVGL, tinyxml2
  platform/   Interfaces: IHttpClient, IStreamSocket, IDiscovery, IClock, IBacklight, ILogger (+ Log.h)
  sonos/      SoapBuilder, SoapParser, DidlParser, DeviceDescription, SonosClient, Ssdp, LogoUrl
  app/        Commands, IWorker, SonosService (Worker-Seite), AppController (UI-Seite), UpdateManifest
  hcu/        WebSocketClient, SocketHttp, HcuClient (Connect API, Kopplung)
  home/       Types, Commands, IHomeBackend, HomeService (Worker-Seite), HomeController (UI-Seite)
  ui/         MainScreen, HomePage, Styles, Fonts, LogoDecoder
platform_linux/            CurlHttpClient, PosixSsdpDiscovery, OpenSslSocket, ThreadWorker, MockSonos,
                           MockHome, main.cpp
platform_esp32/            EspHttpClient, WifiUdpSsdpDiscovery, EspTlsSocket, FreeRtosWorker, Display,
                           Gt911Touch, Settings, OtaUpdater, main.cpp
tests/                     doctest; fixtures/ (Format-Beispiele) und fixtures/real/ (Mitschnitt Sonos Ray)
```

**Threading:** Die UI ruft nur `AppController` auf. Dieser schickt `Command`s an einen
`IWorker` und übernimmt die `Result`s in `tick()`, das ein `lv_timer` alle 30 ms im
UI-Thread aufruft. LVGL wird nur im UI-Thread angefasst. Der Worker (Linux: `std::thread`)
führt die Befehle blockierend über `SonosService` aus. Auf dem ESP32 übernimmt
`FreeRtosWorker` (Task auf Core 0, Queues mit Zeigern) diese Rolle, `core/` bleibt gleich.
Homematic IP hat einen eigenen Worker gleicher Bauart (`HomeController` → `HomeService` →
`HcuClient`): Der `HomeController` hält dort immer genau einen `Poll` am Laufen, der bis 250 ms
auf Push-Events der HCU wartet. Ein Tipp wird sofort angezeigt und spätestens nach diesem Poll
gesendet. Das große Systemabbild der HCU parst ArduinoJson mit Filter (nur Name, Typ, Zustand)
und auf dem ESP32 im PSRAM.

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
- Nach einem Firmware-Update (oder anderen längeren Schreibvorgängen im Flash) zeigt das Display
  manchmal Streifen, bis es einmal vom Strom getrennt wird. Ursache: Während der Flash beschrieben
  wird, kommt der ESP32 nicht an den Bildspeicher im PSRAM, das RGB-Panel bekommt Datenmüll und der
  ST7701 bleibt danach gestört. Neustart des ESP32, Neu-Synchronisieren der Bildausgabe,
  Software-Reset und Schlafmodus des Panels helfen nicht (getestet); der Reset-Pin des Panels ist
  nicht angeschlossen. Abhilfe wären `CONFIG_SPIRAM_XIP_FROM_PSRAM` und
  `CONFIG_LCD_RGB_ISR_IRAM_SAFE`, die in den vorkompilierten Arduino-Bibliotheken fehlen
  (eigene Bibliotheken bauen, z. B. mit der pioarduino-Plattform).
- Homematic IP: nur einzelne Geräte, keine Gruppen/Räume und keine Lamellen (Jalousie-Winkel).
  Es werden alle Steckdosen und Rollläden der HCU angezeigt; eine Auswahl gibt es noch nicht.
  Lange Namen werden mit „…“ gekürzt. Auf echter Hardware noch nicht getestet (Speicherbedarf
  der dauerhaften TLS-Verbindung neben LVGL und einem gleichzeitigen Firmware-Update).
- Ein Logo zu laden dauert auf dem ESP32 1,5–2,5 s. Ein Tipp in dieser Zeit (nur kurz nach dem
  Start oder nach geänderten Favoriten) wartet, bis das laufende Logo fertig ist.
