# temp_lightbar

GoldHEN-Plugin für die PS4, das die CPU-Temperatur der Konsole als Farbe der DualShock-4-Lightbar anzeigt. Ein Blick auf den Controller zeigt, wie warm die Konsole gerade läuft, ohne Overlay im Bild.

**Version:** 2.0

## Funktionen

- Liest die CPU-Temperatur alle 10 Sekunden.
- Setzt die Lightbar aller angemeldeten Controller alle 2 Sekunden auf die passende Farbe. Neu verbundene Controller übernehmen die Farbe dadurch nach kurzer Zeit.
- Greift nicht in den Spielcode ein (keine Hooks) und funktioniert dadurch spielunabhängig.
- Zeigt beim Start eine kurze Meldung mit der aktuellen Temperatur.

## Farbskala

| Temperatur | Farbe |
|---|---|
| bis 45 °C | Blau |
| 58 °C | Grün |
| 68 °C | Gelb |
| ab 78 °C | Rot |

Zwischen den Stufen wird die Farbe stufenlos überblendet. Die Grenzen stehen in `source/main.c` in der Tabelle `g_stops` und lassen sich dort anpassen.

## Voraussetzungen

- PS4 mit GoldHEN 2.3 oder neuer
- Plugin-Loader in GoldHEN aktiviert (`PluginLoader_Enabled = 1` in `/data/GoldHEN/config.ini`)
- FTP-Zugang zur Konsole, z. B. über den GoldHEN-FTP-Server und FileZilla

## Installation

1. `temp_lightbar.prx` nach `/data/GoldHEN/plugins/` kopieren.
2. In `/data/GoldHEN/plugins.ini` eintragen (Datei anlegen, falls sie fehlt):

   ```ini
   [default]
   /data/GoldHEN/plugins/temp_lightbar.prx
   ```

   `[default]` lädt das Plugin in jedem Spiel. Soll es nur in bestimmten Spielen laufen, stattdessen die Title-ID als Abschnitt verwenden, z. B. `[CUSA00001]`.
3. Ein Spiel starten. Nach einigen Sekunden erscheint „temp_lightbar 2.0 läuft: … C“, und die Lightbar wechselt auf die Temperaturfarbe.

Plugins werden nur beim Start eines Spiels geladen. Nach Änderungen das Spiel vollständig schließen (Options-Taste → „Anwendung schließen“) und neu starten.

## Bauen

Benötigt werden die [OpenOrbis PS4 Toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain) und das [GoldHEN Plugins SDK](https://github.com/GoldHEN/GoldHEN_Plugins_SDK).

1. Den Ordner `temp_lightbar` nach `plugin_src/` im [GoldHEN Plugins Repository](https://github.com/GoldHEN/GoldHEN_Plugins_Repository) legen.
2. Umgebungsvariablen setzen:

   ```sh
   export OO_PS4_TOOLCHAIN=/pfad/zur/PS4Toolchain
   export GOLDHEN_SDK=/pfad/zum/GoldHEN_Plugins_SDK
   ```
3. Im Plugin-Ordner `make` ausführen. Die fertige `temp_lightbar.prx` liegt anschließend in `bin/plugins/prx_final/`.

## Einschränkungen

- Die Anzeige läuft nur, solange ein Spiel geöffnet ist. Im Home-Menü zeigt die Lightbar wieder die normale Farbe.
- Setzt ein Spiel selbst eine Lightbar-Farbe, wird diese spätestens nach 2 Sekunden wieder überschrieben.
- Getestet auf einer PS4 mit GoldHEN, u. a. mit Brotato. Rückmeldungen zu anderen Firmware-Versionen und Spielen sind willkommen.

## Fehlermeldungen

| Meldung | Bedeutung |
|---|---|
| „Controller-Funktionen nicht gefunden“ | Die Controller-Bibliothek konnte im Spielprozess nicht angesprochen werden. |
| „Temperatur-Gerät nicht verfügbar (…)“ | Die Temperatur kann in diesem Spiel nicht gelesen werden. Die Zahl in Klammern ist der Fehlercode. |
| „Temperatur nicht lesbar (…)“ | Eine einzelne Abfrage ist fehlgeschlagen. Das Plugin versucht es beim nächsten Durchlauf erneut. |

## Danksagung

- [GoldHEN](https://github.com/GoldHEN) für Plugin-Loader und SDK
- [OpenOrbis](https://github.com/OpenOrbis) für die PS4-Toolchain
