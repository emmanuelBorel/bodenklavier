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