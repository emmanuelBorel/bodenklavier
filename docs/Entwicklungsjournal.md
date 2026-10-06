# Entwicklungsjournal – Kameragestütztes Bodenklavier

## Projekt

Entwicklung eines kameragestützten elektronischen Bodenklaviers auf einem Raspberry Pi 5.

---

## Entwicklungsstand

### Phase 1 – Inbetriebnahme der Tiefenkamera

**Status:** Abgeschlossen

**Durchgeführte Arbeiten:**

- Einrichtung des Raspberry Pi 5
- Anschluss der Orbbec Astra
- Installation und Konfiguration von OpenNI2
- Erfolgreicher Test mit NiViewer
- Erfolgreicher Test mit DepthReaderPoll
- Auslesen der Tiefendaten in Echtzeit

**Technisches Problem:**

Die Python-Anbindung über `primesense` führte beim Start des Depth-Streams zu einem Speicherfehler.

**Entscheidung:**

Für die stabile Anbindung der Orbbec Astra wird zunächst die native OpenNI2-C++-Schnittstelle verwendet.

---

### Phase 2 – Erster Bodenklavier-Prototyp

**Status:** Abgeschlossen

**Durchgeführte Arbeiten:**

- Definition erster virtueller Tastenzonen
- Erkennung einer Aktivierung mittels Tiefendaten
- Zustandslogik zur Vermeidung mehrfacher Auslösungen
- Integration von OpenCV
- Grafische Echtzeitdarstellung der Tiefendaten
- Visualisierung der aktiven Tastenzonen

---

### Phase 3 – Kalibrierung der Bodenmatte

**Status:** In Vorbereitung

**Referenzmatte:**

- Länge: 255 cm
- Breite: 80 cm
- Weiße Tasten: 14
- Schwarze Tasten: 10

**Nächstes Ziel:**

Entwicklung eines Kalibrierungsverfahrens zur Zuordnung der realen Bodenmatte zu den Bild- und Tiefenkoordinaten.

## 06.10.2026 – Integration realistischer Klavierklänge und erfolgreicher End-to-End-Test

### Ziel

Ziel dieses Entwicklungsschrittes war es, die bisher verwendeten synthetischen
Sinustöne durch realistische Klavierklänge zu ersetzen und die vollständige
Verarbeitungskette des 3D-basierten Bodenklaviers auf dem Raspberry Pi 5 zu testen.

### Ausgangssituation

Die kamerabasierte Erkennung mit der Orbbec Astra war bereits funktionsfähig.
Nach der manuellen Kalibrierung des Bodenklaviers konnte ein Tiefenreferenzbild
des leeren Bodens aufgenommen werden.

Die Verarbeitungskette bestand zu diesem Zeitpunkt aus:

Orbbec Astra
→ Tiefenbild
→ perspektivische Transformation
→ TouchDetector
→ ConflictResolver
→ KeyStateManager
→ NoteMapper
→ AudioEngine

Für erste Audiotests wurden zunächst künstlich erzeugte Sinustöne verwendet.

### Integration realistischer Klavierklänge

Als neue Klangquelle wurde die Sample-Bibliothek Salamander Grand Piano V3
verwendet. Die verwendeten Quelldateien liegen als WAV-Samples mit einer
Abtastrate von 44,1 kHz und einer Auflösung von 16 Bit vor.

Zur Integration in die bestehende Audioarchitektur wurde ein Python-Skript
`generate_piano_samples.py` erstellt.

Das Skript liest die Zuordnung der Samples aus der SFZ-Datei der
Klavierbibliothek aus und erzeugt die für das Bodenklavier benötigten
24 Audiodateien im Tonumfang C4 bis B5.

Als Zielanschlagstärke wurde eine MIDI-Velocity von 80 verwendet.

Erzeugt wurden:

C4, Cs4, D4, Ds4, E4, F4, Fs4, G4, Gs4, A4, As4, B4,
C5, Cs5, D5, Ds5, E5, F5, Fs5, G5, Gs5, A5, As5, B5.

Die erzeugten Dateien befinden sich unter:

`media/sounds_piano/`

Alle 24 Audiodateien wurden erfolgreich erzeugt und anschließend manuell
über die Audioausgabe des Raspberry Pi getestet.

### Integration in die Anwendung

Die bestehende Klasse `AudioEngine` auf Basis von SDL2 und SDL2_mixer konnte
weiterverwendet werden.

Der Audio-Pfad der Anwendung wurde auf

`media/sounds_piano`

umgestellt.

Beim Start der Anwendung konnten alle 24 WAV-Dateien erfolgreich geladen werden.

Die Audioausgabe erfolgte während des Tests über einen Bluetooth-Kopfhörer
über PipeWire/PulseAudio.

Die Anwendung wurde mit folgendem Befehl gestartet:

`SDL_AUDIODRIVER=pulseaudio ./build/bodenklavier_visual`

### End-to-End-Test

Anschließend wurde das vollständige System mit dem realen Bodenklavier getestet.

Nach der Vier-Punkt-Kalibrierung wurde bei leerem Bodenklavier ein
Referenz-Tiefenbild aufgenommen.

Beim Betreten der weißen Tasten wurden stabile PRESS- und RELEASE-Ereignisse
erzeugt.

Beispiel:

PRESS : C4
RELEASE : C4
PRESS : D4
RELEASE : D4
PRESS : E4
RELEASE : E4

Die entsprechenden realistischen Klavierklänge wurden erfolgreich abgespielt.

Damit konnte erstmals die vollständige Verarbeitungskette erfolgreich
demonstriert werden:

Fußbewegung
→ Orbbec-Astra-Tiefenbild
→ Tastenerkennung
→ Ereigniserkennung
→ musikalische Zuordnung
→ Audioausgabe
→ realistischer Klavierklang

### Schwarze Tasten

Auch die schwarzen Tasten wurden erkannt und über den `NoteMapper` auf
musikalische Notennamen abgebildet.

Beispielsweise wird eine physische Taste wie `BLACK_01` aktuell auf `Cs4`
abgebildet.

Die musikalische Zuordnung der schwarzen Tasten ist derzeit noch als
konfigurierbare Arbeitsannahme zu betrachten, da die physische Anordnung des
verwendeten Bodenklaviers visuell eine 3-2-3-2-Gruppierung besitzt.

Während des Tests zeigte sich außerdem, dass beim Wechsel zwischen benachbarten
schwarzen Tasten teilweise zwei schwarze Tasten gleichzeitig erkannt werden.

Beispiele waren unter anderem Kombinationen wie:

BLACK_10 / BLACK_09  
BLACK_07 / BLACK_06  
BLACK_05 / BLACK_04  
BLACK_03 / BLACK_02

Die bisherige Konfliktauflösung verbessert insbesondere Konflikte zwischen
schwarzen und weißen Tasten. Konflikte zwischen zwei benachbarten schwarzen
Tasten müssen noch genauer untersucht werden.

### Aktueller Entwicklungsstand

Der 3D-Prototyp ist nun grundsätzlich spielbar.

Folgende Kernfunktionen sind funktionsfähig:

- Aufnahme von RGB- und Tiefendaten mit der Orbbec Astra
- Registrierung von Tiefen- und Farbbild
- manuelle Vier-Punkt-Kalibrierung
- perspektivische Transformation des Bodenklaviers
- Definition von 14 weißen und 10 schwarzen Tastenbereichen
- Aufnahme eines Tiefenreferenzbildes
- Erkennung von Veränderungen innerhalb der Tastenbereiche
- zeitliche Stabilisierung durch PRESS-, HOLD- und RELEASE-Zustände
- Konfliktbehandlung zwischen schwarzen und weißen Tasten
- Zuordnung physischer Tasten zu musikalischen Noten
- polyphone Audioausgabe über SDL2_mixer
- Wiedergabe realistischer Klaviersamples

### Offene Punkte

Der wichtigste nächste Untersuchungspunkt ist die tatsächliche
Bodenkontakterkennung.

Der aktuelle Algorithmus erkennt eine Veränderung der Tiefe innerhalb eines
Tastenbereichs. Damit wird zuverlässig erkannt, dass sich beispielsweise ein
Fuß innerhalb beziehungsweise oberhalb einer Taste befindet.

Dies bedeutet jedoch noch nicht automatisch, dass der Fuß die Bodenmatte
tatsächlich berührt.

Für die weitere Entwicklung muss deshalb experimentell untersucht werden,
wie sich folgende Zustände anhand der Tiefendaten unterscheiden lassen:

1. leere Taste,
2. Fuß oberhalb der Taste ohne Bodenkontakt,
3. Fuß mit tatsächlichem Bodenkontakt.

Diese Untersuchung ist besonders relevant, da eine Taste des Bodenklaviers
erst bei einer tatsächlichen Berührung ausgelöst werden soll.

Weitere offene Punkte sind:

- Verbesserung von Konflikten zwischen benachbarten schwarzen Tasten
- automatische Kalibrierung des Bodenklaviers
- Implementierung einer Vergleichslösung mit klassischer RGB-Webcam
- Messung von Erkennungsgenauigkeit und Fehlaktivierungen
- Messung der Reaktionszeit
- Untersuchung verschiedener Lichtbedingungen
- Messung von FPS, CPU- und RAM-Auslastung
- systematischer Vergleich zwischen 3D-Tiefenkamera und RGB-Webcam
- optionale MIDI-Ausgabe

### Ergebnis

Mit diesem Entwicklungsschritt wurde ein wichtiger Meilenstein erreicht:
Der 3D-basierte Prototyp des Bodenklaviers kann auf dem Raspberry Pi 5
Tasteninteraktionen erkennen und die entsprechenden realistischen
Klavierklänge in Echtzeit wiedergeben.