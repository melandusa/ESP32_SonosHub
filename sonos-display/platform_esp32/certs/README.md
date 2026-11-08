# CA-Zertifikate für HTTPS (Firmware-Updates)

`x509_crt_bundle.bin`: Mozillas Stammzertifikate im Bundle-Format von ESP-IDF, wird in die
Firmware eingebettet (`board_build.embed_files`). Damit prüft das Display die Zertifikate von
github.com und den GitHub-Download-Servern, egal welche bekannte Zertifizierungsstelle sie nutzen.

Erneuern (z. B. einmal im Jahr):

```sh
curl -o cacert.pem https://curl.se/ca/cacert.pem
python3 ~/.platformio/packages/framework-arduinoespressif32/tools/gen_crt_bundle.py -i cacert.pem
mv x509_crt_bundle platform_esp32/certs/x509_crt_bundle.bin
```

(benötigt das Python-Paket `cryptography`)
