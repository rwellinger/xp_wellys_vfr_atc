# Welly's ATC — KI-Sprechfunk-ATC für X-Plane 12

![Welly's ATC Panel mit ATIS-Ansage in LSZB Bern-Belp](images/atc-atis-example.jpg)

> **Sprich per Push-to-Talk mit dem Tower — KI-gestützter, deutscher
> VFR-Sprechfunk für X-Plane 12, lokal auf dem Mac oder über die Cloud.**

Welly's ATC verwandelt deine VFR-Flüge in X-Plane 12 in ein echtes
Funkgespräch: Du drückst die Push-to-Talk-Taste, sprichst deinen Funkspruch
ins Mikrofon, und der Tower antwortet dir gesprochen zurück — in
**deutscher VFR-Phraseologie**, mit realistischen Reaktionen auch auf
Pilotenfehler.

---

## Was das Plugin abdeckt

- **🇩🇪 BZF · NfL · DACH · VFR** — modelliert ausschliesslich **deutsche
  VFR-Sprechfunk-Phraseologie** nach **NfL Sprechfunk 2024** (DACH-Raum).
  Optionaler **BZF-Strict-Mode** prüft deine Readbacks auf
  Vollständigkeit, wie es ein Prüfer täte.
- **🛬 Platzrunde** — vollständiger Platzrunden-Ablauf an kontrollierten
  und unkontrollierten Flugplätzen: Anflug, Gegenanflug, Queranflug,
  Endteil, Landung, Touch-and-Go, Durchstart — inklusive
  Lande-Sequenzierung („Sie sind Nummer zwei, folgen Sie dem Verkehr").
- **🗺️ Cross Country** — kompletter Überlandflug: Abflugfreigabe,
  En-route-Frequenzwechsel und Anflug-Ablauf zwischen Flugplätzen. Der
  Anflug-Lotse übergibt dich proaktiv mit der Zielfrequenz an den Tower.
- **🗼 Flugplatztypen** — das Plugin erkennt automatisch die Art der
  Flugverkehrsdienste am jeweiligen Flugplatz und passt Phraseologie und
  Ablauf entsprechend an:
  - **Ohne Flugverkehrskontrolle** — Selbstinformation über UNICOM/CTAF
    (Selbstansagen, keine Freigaben).
  - **Mit Tower** — kontrollierter Platz mit Freigaben über die
    Turm-Frequenz.
  - **Mit Tower und Ground** — zusätzlich getrennte Rollkontrolle
    (Ground) für Rollfreigaben.
  - **Mit AFIS** — Flugplatz mit Fluginformationsdienst (Information
    statt Kontrolle): Verkehrs- und Platzinformationen ohne
    verbindliche Freigaben.
- **🎙️ Funksprache** — natürliche Sprache per Push-to-Talk (Tastatur oder
  Joystick). Kontextbewusste **Phraseologie-Hinweise** zeigen dir, was zu
  sagen ist, und der Tower coacht höflich bei unpassender Funkdisziplin.
- **🤖 KI-Unterstützung** — Spracherkennung → Absichtsverständnis →
  Sprachausgabe, dazu automatische **ATIS-Ansagen** aus dem Live-Wetter und
  **Verkehrshinweise** zu umgebenden Flugzeugen.
- **💻 Lokale KI & ☁️ Cloud-KI** — wähle deinen Modus zur Laufzeit:
  - **Local** (Apple Silicon) — läuft nach einem einmaligen Modell-Download
    **100 % offline**. Kein Abo, kein API-Key, kein dauerndes Internet.
  - **OpenAI Cloud** (jeder Mac) — mit eigenem API-Key.
  - **Mistral Cloud** (jeder Mac) — mit eigenem API-Key; spricht Deutsch
    ohne US-Akzent.
- **🪟 Windows (Cloud-only, funktioniert)** — ein reiner Cloud-Build für
  X-Plane 12 unter Windows ist enthalten (`win_x64/xp_wellys_devfr_atc.xpl`,
  OpenAI **oder** Mistral, API-Key im Windows Credential Manager). Auf echter
  Windows-Hardware (X-Plane 12) verifiziert: Laden, Mikrofon/PTT, die
  STT→ATC→TTS-Pipeline und der API-Key im Credential Manager laufen. Lokale
  Offline-KI gibt es unter Windows **nicht** (kein Apple Silicon / kein Metal);
  Cloud ist die einzige Option.

## Wofür Welly's ATC gedacht ist

> **Dieses Plugin ist in erster Linie an der REALITÄT orientiert.** Das Ziel
> ist, die deutschen VFR-Funkverfahren so echt wie möglich nachzubilden,
> damit du **für künftige Examen und Prüfungen — etwa das BZF — trainieren
> und üben** kannst. Wir arbeiten kontinuierlich darauf hin, die
> Phraseologie und die ATC-Abläufe noch näher an die Praxis zu bringen.

**Haftungsausschluss.** Welly's ATC ist ein Übungs- und Trainingswerkzeug
für die Flugsimulation. Es ist **keine offizielle Zertifizierung, kein
Lehrmittel im Sinne einer anerkannten Ausbildung und kein Ersatz für eine
echte Prüfungsvorbereitung**. Wir übernehmen **keine Verantwortung und geben
keinerlei Garantie auf das Bestehen einer Prüfung oder eines Examens**. Die
Nutzung erfolgt auf eigene Verantwortung; für die Korrektheit der
abgebildeten Phraseologie wird keine Gewähr übernommen. Korrekturen von
BZF-Inhabern sind ausdrücklich willkommen.

## Was es (noch) nicht kann

- **Kein IFR** — kein Instrumentenflug, keine IFR-Freigaben, keine
  Flugplanung, kein FMS-/Routing.
- **Nur Deutsch** — modelliert ausschliesslich NfL-DACH-VFR-Phraseologie;
  andere Sprachen oder Profile sind nicht vorgesehen.
- **Lokaler Modus nur auf Apple Silicon** — Intel-Macs können das Plugin
  nutzen, aber nur im Cloud-Modus (OpenAI oder Mistral, API-Key nötig).
- **Keine lokale Offline-KI unter Windows** — der Windows-Build funktioniert,
  aber ausschliesslich im Cloud-Modus (OpenAI oder Mistral, API-Key nötig);
  lokale Modelle brauchen Apple Silicon / Metal.
- **Noch nicht modelliert** — Wirbelschleppen-Staffelung, frei wählbare
  Rollwege (aktuell immer „via Alpha"), grosse Hub-Flugplätze (LSZH, LSGG …)
  mit Delivery-Workflow, sowie ein virtueller Co-Pilot / Checklisten-Reader.

> Eine ausführliche Aufstellung der Einschränkungen samt Aufwandsschätzung
> steht in der [technischen Dokumentation](docs/README.md#bekannte-einschränkungen).

## 📖 Technische Dokumentation

Installation, Schnellstart, Backend-Modi, Build aus Quellcode, lokale
Inferenz-Modelle, Konfiguration, Architektur und Entwicklungs-Workflow:

**→ [docs/README.md](docs/README.md)**

## Lizenz

[GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.html)
— Details und die Aufschlüsselung der Drittanbieter-Abhängigkeiten in der
[technischen Dokumentation](docs/README.md#lizenz) bzw. in
[`THIRD_PARTY.md`](THIRD_PARTY.md).
