# Prüfstand: WebUI und Lua-Menü im Browser

`pruefstand.html` zeigt links die echte Weboberfläche der Firmware und rechts
das Lua-Menü so, wie es am Sender (ELRS-Lua) erscheint. Alles kommt aus dem
Firmware-Code; nur die Antworten des Moduls sind simuliert. Es geht nichts an
ein echtes Modul.

Varianten: CRSF-Router8 (RP2350): RP2350-Zero

**Ansehen:** [Prüfstand öffnen](https://raw.githack.com/mnorden81/RC-CRSF-Router-RP2350/main/pruefstand/pruefstand.html)
(oder die Datei `pruefstand.html` herunterladen und im Browser öffnen).

## Was aus dem Code kommt

- Weboberfläche: die HTML-Seite aus der Firmware, unverändert.
- Lua-Menü: Feld-IDs, Ordner, Namen, Grenzen und Auswahllisten liest
  `luaparse.py` direkt aus `buildLuaMenu()` (je Variante mit den
  passenden `#if`-Schaltern).
- Startwerte der Antworten (`/api/config`, `/api/status` usw.): `build.py`
  führt die JSON-Funktionen der Firmware trocken aus, nach der
  Werkseinstellung (z. B. `Reset_all()`, `nvsReset()`, `cfgDefaults()`).
- Pins, Versionsnummer und Board-Namen aus den Konstanten im Code.

Simuliert sind nur Dinge, die erst im laufenden Modul entstehen: Kanalwerte,
Zähler, Dateien im Sound-Speicher, Funk-Gegenstellen.

## Aktuell halten

Nach jeder Änderung an der Firmware:

```
python3 pruefstand/build.py
```

und `pruefstand/pruefstand.html` mit committen. Die GitHub-Prüfung
„Prüfstand" (`.github/workflows/pruefstand.yml`) führt
`python3 pruefstand/build.py --check` aus und schlägt fehl, wenn

- die Seite nicht mehr zum Code passt (neu bauen vergessen),
- Lua-Feld-IDs doppelt sind, Lücken haben oder nicht zur Feldanzahl passen,
- die Firmware eine neue Web-Route oder ein neues JSON-Feld hat, das die
  Simulation noch nicht kennt.

## Dateien

| Datei | Inhalt |
|---|---|
| `config.json` | welche Firmware, welche Dateien, welche Varianten |
| `build.py` | baut `pruefstand.html`, `--check` für die CI |
| `luaparse.py` | liest Lua-Menü und JSON-Funktionen aus dem C++-Code |
| `template.html` | Seite des Prüfstands |
| `mock_common.js` | gemeinsame Simulation (Sicherungen, Funk, Sound-Dateien) |
| `mock_*.js` | Simulation dieses Moduls |

`build.py`, `luaparse.py`, `template.html` und `mock_common.js` sind in
allen Modul-Repos gleich.
