# Welly's ATC — KI-Sprechfunk-ATC für X-Plane 12

![Welly's ATC Panel mit ATIS-Ansage in LSZB Bern-Belp](images/atc-atis-example.jpg)

> **Deutschland-VFR-ATC-Plugin für X-Plane 12 mit drei Backends: lokale Inferenz, OpenAI Cloud oder Mistral Cloud — deine Wahl.**
>
> Das Plugin modelliert ausschliesslich **deutsche VFR-Phraseologie** nach
> NfL Sprechfunk 2024 (DACH-VFR), mit optionalem **BZF-Strict-Mode**.
>
> - **Local (Apple Silicon, Standard)** — `whisper.cpp` (Metal) + `llama.cpp`
>   (Metal) + Piper TTS, nach dem Modell-Download vollständig offline.
>   Keine Daemons, keine Hilfs-Apps, keine API-Keys.
> - **OpenAI Cloud (jeder Mac)** — Whisper API + Chat Completions +
>   TTS API. Eigener API-Key (im macOS Keychain gespeichert).
> - **Mistral Cloud (jeder Mac)** — Voxtral STT + Mistral Chat
>   Completions + Voxtral TTS. Eigener API-Key (separater
>   Keychain-Eintrag, sodass OpenAI- und Mistral-Key koexistieren).
>   Pro Token günstiger als OpenAI; der einzige Cloud-Modus, in dem die
>   ATIS Deutsch ohne US-Akzent spricht (Voxtral TTS ist multilingual).
>
> Das Plugin wird als **Universal Binary** ausgeliefert: der arm64-Slice
> trägt alle drei Backends, der x86_64-Slice ist Cloud-only (OpenAI oder
> Mistral). Den Modus wählst du zur Laufzeit in den Einstellungen.
>
> **Gemessene Pipeline-Latenz** (warm, M4, lokale Inferenz, End-to-End-Spike):
> STT 321 ms · LM 634 ms · TTS 200 ms · **gesamt ≈ 1,16 s pro Anfrage** —
> deutlich unter dem 3-s-Akzeptanzziel. Die Cloud-Modi (OpenAI / Mistral)
> sind typischerweise langsamer: 2–3 s warm, dominiert von der API-Latenz.

---

KI-gestütztes Sprechfunk-ATC-Plugin für VFR-Flüge in X-Plane 12.

Sprich per Push-to-Talk über dein Mikrofon mit dem ATC. Das Plugin
transkribiert deine Sprache (lokal mit whisper.cpp, über die OpenAI
Whisper API oder über Mistrals Voxtral STT — deine Wahl), interpretiert
deine Absicht über eine regelbasierte ATC-State-Machine — mit einem
Fallback bei geringer Konfidenz auf einen lokalen Llama-3.2-3B-Classifier,
OpenAIs `gpt-4o-mini` oder Mistral Small — und spielt die ATC-Antworten
zurück, lokal mit Piper synthetisiert oder über die TTS-API von OpenAI /
Mistral.

## Inhaltsverzeichnis

- [Funktionen](#funktionen)
- [Hardware-Anforderungen](#hardware-anforderungen)
- [Software-Anforderungen](#software-anforderungen)
- [Schnellstart](#schnellstart-vorgefertigtes-release)
- [Backend-Modi](#backend-modi)
- [Funkstörungs-Wiederherstellung (TTS-Fehler-Schutz)](#funkstörungs-wiederherstellung-tts-fehler-schutz)
- [Aus Quellcode bauen](#aus-quellcode-bauen)
- [Lokale Inferenz-Modelle](#lokale-inferenz-modelle)
- [Konfiguration](#konfiguration)
- [Benutzung](#benutzung)
- [Make-Targets](#make-targets)
- [Bekannte Einschränkungen](#bekannte-einschränkungen)
- [FAQ](#faq)
- [Projektstruktur](#projektstruktur)
- [Drittanbieter-Abhängigkeiten](#drittanbieter-abhängigkeiten)
- [Entwicklungs-Workflow](#entwicklungs-workflow)
- [Lizenz](#lizenz)

## Funktionen

- **Push-to-Talk** — über X-Plane-Befehlsbindung (Tastatur oder Joystick)
- **Drei-Backend-Inferenz** — wähle **Local** (nur Apple Silicon),
  **OpenAI Cloud** oder **Mistral Cloud** (beide auf jedem Mac, eigener
  API-Key) im Einstellungs-Tab. Umschaltung zur Laufzeit, kein
  Plugin-Neustart. Jeder Inferenz-Aufruf wird in der `Log.txt` von
  X-Plane mit `[STT-LOCAL]` / `[STT-OPENAI]` / `[STT-MISTRAL]` (und
  entsprechend für LM/TTS) getaggt, sodass du nachvollziehen kannst,
  welche Seite jede Anfrage bedient hat.
- **Lokales Speech-to-Text** — `whisper.cpp` `large-v3-turbo-q5_0`
  (multilingual, Deutsch), Metal-beschleunigt
- **Lokales LLM** — `llama.cpp` mit Llama 3.2 3B Instruct (Q4_K_M),
  Metal-beschleunigt; dient zur Absichts-Disambiguierung, wenn der
  regelbasierte Parser unsicher ist. Die Reparatur-Ausgabe wird
  ziffernvalidiert, um halluzinierte Pisten oder Frequenzen zu
  unterdrücken.
- **Lokales Text-to-Speech** — Piper, deutsche Stimme
  (`de_DE-thorsten-medium`), CPU + onnxruntime
- **OpenAI-Cloud-Option** — `whisper-1` für STT, `gpt-4o-mini` für den
  Absichts-Classifier (JSON-Modus für eingeschränkte Ausgabe), `tts-1`
  mit sechs wählbaren Stimmen (`alloy/echo/fable/onyx/nova/shimmer`;
  `onyx` kommt echtem ATC am nächsten). Key im macOS Keychain über
  `Security.framework`, nie in `settings.json`, nie vollständig
  geloggt (nur die letzten 4 Zeichen erscheinen in Audit-Zeilen).
- **Mistral-Cloud-Option** — `voxtral-mini-2507` für STT (mit
  `context_bias[]` Flugplatz-Biasing), `mistral-small-latest` für den
  Absichts-Classifier (JSON-Modus), `voxtral-mini-tts-2603` mit 30
  Preset-Stimmen britischer, amerikanischer und französischer Sprecher
  in 7–9 emotionalen Registern (Standard je Rolle: `gb_oliver_neutral`
  für ATIS, `en_paul_confident` für Tower, `en_paul_neutral` für
  Ground). Voxtral TTS ist multilingual und spricht Deutsch ohne
  US-Akzent. Separater Keychain-Eintrag, sodass OpenAI- und
  Mistral-Key koexistieren; ein Moduswechsel erfordert nie erneutes
  Einfügen.
- **ATC-State-Machine** — VFR-Phraseologie für kontrollierte und
  unkontrollierte Flugplätze
- **Flugphasen-Erkennung** — kontextbewusste Guards verhindern
  unrealistische ATC-Interaktionen je nach Flugzeugzustand (geparkt,
  Rollen, in der Luft usw.)
- **Live-Verkehrserkennung (v2.1) + Lande-Sequenzierung (v2.2)** —
  Provider-unabhängiger `sim/cockpit2/tcas/targets/...`-Reader, der
  einen 2-Hz-`TrafficContext`-Snapshot speist. Verkehrshinweise mit
  Sprachbestätigung („in Sicht" / „negativ" / „Ausschau") auf einem
  Seitenkanal, der den ATC-Hauptablauf nicht stört. **v2.2 ergänzt
  VFR-Lande-Sequenzierung** — Folgenummer und „folgen Sie dem Verkehr"
  wenn anderer Verkehr im Endteil oder in der Platzrunde ist, „setzen
  Sie den Anflug fort, Verkehr auf der Piste" wenn die aktive Piste
  blockiert ist, und ein unaufgeforderter, vom Tower angewiesener
  Durchstart innerhalb von 1 NM zur Schwelle, wenn die Piste belegt
  bleibt. Der Hauptschalter `traffic_features_enabled` in den
  Einstellungen deaktiviert das ganze Subsystem mit einem Klick.
- **ATIS-Generierung** — automatische ATIS-Ansagen aus Live-Wetterdaten
  des Sims, auf COM1 *oder* COM2 (aktiv oder Standby). Bricht die
  laufende Ansage ab, wenn der Pilot die spielende COM umstimmt.
- **Funkdisziplin-Coaching** — ATC erinnert höflich, wenn der Pilot
  unangemessene Sprache verwendet, mit Eskalation bei Wiederholung
- **Phraseologie-Hinweise** — kontextbewusster Spickzettel mit voller
  Phraseologie beim Überfahren
- **Cross-Country-Unterstützung** — vollständiger VFR-Abflug,
  En-route-Frequenzwechsel und Anflug-Ablauf zwischen Flugplätzen. Der
  Anflug-Lotse übergibt proaktiv mit der Zielfrequenz an den Tower.
- **Anzeige des Luftfahrzeugkennzeichens** — Piloten-Rufzeichen mit dem
  tatsächlichen, aus X-Plane gelesenen Kennzeichen verknüpft
- **„Disregard"-Wiederherstellung** — ablaufbewusstes Zurücksetzen
  (PATTERN_ENTRY in der Luft nahe dem Heimatplatz, EN_ROUTE im Transit,
  IDLE am Boden)
- **TTS-Fehler-Wiederherstellung (Funkstörungs-Schutz)** — wenn
  Sprachsynthese oder Wiedergabe scheitert (OpenAI-Timeout,
  Netzabbruch, Piper-IO-Fehler), strandet das Plugin den Piloten nicht
  in einem Zustand, den der Tower nie angesagt hat. Vor jeder
  Pilotenübertragung wird ein Snapshot der ATC-State-Machine erstellt;
  bei einem Fehler spielt das Plugin einen kurzen Squelch-Burst auf der
  aktiven COM und setzt entweder den Zustand zurück („Funkspruch
  wiederholen") oder — falls inzwischen eine Auto-Korrektur weitergelaufen
  ist — hält die ungesendete Freigabe über `REQUEST_REPEAT`
  („Wiederholen Sie") erreichbar. Siehe
  [Funkstörungs-Wiederherstellung](#funkstörungs-wiederherstellung-tts-fehler-schutz).
- **Funkstrom-Erkennung** — das ATC-Panel deaktiviert sich, wenn das
  COM-Funkgerät keinen Strom hat, mit optionalem Bypass für exotische
  Flugzeuge
- **Plugin-interner Modell-Downloader** — der erste Start zeigt einen
  ImGui-Dialog, HTTPS-fortsetzbare Downloads von HuggingFace,
  SHA256-verifiziert vor der Nutzung
- **ImGui-UI** — In-Sim-ATC-Panel mit Frequenzverwaltung,
  Phraseologie-Hinweisen, Transkript-Historie, einem Models-Tab für
  Download / Neuverifizierung und einem optionalen Traffic-Tab (Debug),
  der die 10 nächsten Flugzeuge auflistet

## Hardware-Anforderungen

Das Plugin wird als **Universal Binary** ausgeliefert — ein `.xpl`, zwei
Slices. X-Plane lädt automatisch den passenden.

| Mac | Geladener Slice | Verfügbare Backends |
|---|---|---|
| Apple Silicon (M1 / M2 / M3 / M4) | `arm64` | **Local**, **OpenAI Cloud** *oder* **Mistral Cloud** |
| Intel (x86_64) | `x86_64` | **Nur Cloud** — **OpenAI** oder **Mistral** (lokale Inferenz braucht Metal + Apple Silicon) |

| Ressource | Local-Modus | OpenAI- / Mistral-Cloud-Modus |
|---|---|---|
| RAM | 32 GB empfohlen (X-Plane 12 + ~3 GB Reserve für den Inferenz-Stack) | 16 GB (kein Modell im RAM — Aufrufe sind zustandslose HTTP-Requests) |
| Disk | ~2,5 GB frei für die Modelle | ~50 MB für das Plugin-Bundle (keine Modelle geladen) |
| GPU | jede Metal-fähige GPU auf demselben Apple-Silicon-Chip | nicht genutzt |
| Netzwerk | zur Laufzeit nicht genutzt (einmaliger Modell-Download von HuggingFace) | erforderlich — jeder PTT-Release löst HTTPS-Aufrufe an `api.openai.com` oder `api.mistral.ai` aus |

Beide Cloud-Modi kosten Geld pro Anfrage (STT- + LM- + TTS-APIs).
Mistral ist pro Token typischerweise günstiger als OpenAI
(`mistral-small` ≈ 33 % günstigerer Input / 50 % günstigerer Output als
`gpt-4o-mini`). STT und TTS liegen etwa auf Preisparität. Die Latenz
beider Clouds liegt typischerweise bei 2–3 s warm vs. 1–1,5 s warm bei
lokaler Inferenz.

## Software-Anforderungen

| Punkt | Anforderung |
|---|---|
| macOS | **13.3 oder neuer** (onnxruntime 1.22.0 verlangt dies auf dem arm64-Slice; der x86_64-Slice erbt dasselbe Deployment-Target, damit das lipo'd Binary konsistent bleibt) |
| X-Plane | X-Plane 12 (12.0 oder neuer) |
| OpenAI-/Mistral-Konto | Nur falls du einen Cloud-Modus nutzen willst — braucht einen API-Key mit aktivierter Abrechnung beim jeweiligen Anbieter. Der Local-Modus hat keine Cloud-Abhängigkeit. |
| Zum Bauen aus Quellcode | CMake 3.26+, Homebrew LLVM (`brew install llvm`), Xcode Command Line Tools |

## Schnellstart (vorgefertigtes Release)

1. Lade `xp_wellys_devfr_atc-vX.Y.Z.zip` von der GitHub-Releases-Seite. Das
   `.xpl` darin ist ein Universal Binary für arm64 und x86_64.
2. Entpacke nach `X-Plane 12/Resources/plugins/`. Ergebnis:
   ```
   X-Plane 12/Resources/plugins/xp_wellys_devfr_atc/
     ├── mac_x64/
     │     ├── xp_wellys_devfr_atc.xpl       (universal: arm64 + x86_64)
     │     ├── libpiper.dylib          (nur vom arm64-Slice genutzt)
     │     ├── libonnxruntime.1.22.0.dylib
     │     └── libonnxruntime.dylib
     ├── Resources/
     │     └── espeak-ng-data/   (~19 MB, nur vom arm64-Slice genutzt)
     └── data/
           └── (ATC-Profil-Bundle, Prompt-Templates, VRP-Datenbank, etc.)
   ```
3. Starte X-Plane. Öffne das Plugin-Fenster über *Plugins → Welly's ATC*.
4. **Wähle dein Backend** im **Settings**-Tab:
   - **Local** (Apple Silicon, Standard): der **Models**-Tab zeigt die
     Zeilen rot. Klicke **Download all missing** — das Plugin lädt
     ~2,0 GB von HuggingFace über HTTPS. Fortsetzbar; abbrechbar; nach
     jeder Datei SHA256-verifiziert. Sobald alle Zeilen **Ready**
     (grün) zeigen, verschwindet das PTT-deaktiviert-Banner im
     Status-Tab.
   - **OpenAI Cloud** (jeder Mac): füge deinen OpenAI-API-Key in das
     Feld **OpenAI API Key** in den Einstellungen ein (nutze den
     `[Paste]`-Button — Cmd+V im ImGui-Kontext von X-Plane ist
     unzuverlässig). Klicke **Save Key**. Der Key wird im macOS Keychain
     unter dem Service `com.xp_wellys_devfr_atc.openai` gespeichert. PTT ist
     sofort aktiv; kein Modell-Download.
   - **Mistral Cloud** (jeder Mac): füge deinen Mistral-API-Key in das
     Feld **Mistral API key** ein (gleiches `[Paste]`-Muster). Klicke
     **Save Key##mistral**. Der Key wird unter einem separaten
     Keychain-Eintrag `com.xp_wellys_devfr_atc.mistral` gespeichert, sodass
     der OpenAI-Key (falls vorhanden) unberührt bleibt und du ohne
     erneutes Einfügen zwischen Anbietern wechseln kannst. PTT ist
     sofort aktiv.
5. Fliege. Das Banner im Status-Tab zeigt dir den aktiven Modus, und
   die `Log.txt` trägt bei jedem Laden ein einzeiliges
   `BACKEND MODE: ...`-Banner, sodass du im Nachhinein belegen kannst,
   welche Seite die Sitzung bedient hat.

## Backend-Modi

Du kannst jederzeit im Settings-Tab umschalten — das Plugin fährt den
aktiven Inferenz-Stack herunter und einen anderen hoch, ohne
X-Plane-Neustart. Invariante auf Quellcode-Ebene: jede Backend-Familie
lebt in ihrem eigenen Satz `.cpp`-Dateien, und die drei Familien teilen
sich weder Header noch Code-Pfad. Die lokalen Backends
(`whisper_stt.cpp`, `llama_lm.cpp`, `piper_tts.cpp`) enthalten keinen
`#include` eines Cloud-Clients und null `curl_easy_perform`-Aufrufe; die
OpenAI-Clients (`openai_stt.cpp`, `openai_lm.cpp`, `openai_tts.cpp`)
enthalten keinen `#include` von `whisper.h` / `llama.h` / `piper.h` und
keine Mistral-Endpunkte; die Mistral-Clients (`mistral_stt.cpp`,
`mistral_lm.cpp`, `mistral_tts.cpp`) tragen weder lokale Header noch
`api.openai.com`. So kann in einem Modus kein Code-Pfad in die anderen
zwei aufrufen — zur Compile- und Grep-Zeit durch
`tests/test_audit_logging.cpp` verifiziert.

Nachvollziehen, welcher Modus eine Anfrage bedient hat: `Log.txt` grepen.

| Tag in `Log.txt` | Bedeutung |
|---|---|
| `[xp_wellys_devfr_atc] BACKEND MODE: LOCAL ...` | Der Loader hat die lokale Pipeline hochgefahren. |
| `[xp_wellys_devfr_atc] BACKEND MODE: OPENAI (api.openai.com) ...` | Der Loader hat die OpenAI-Cloud-Pipeline hochgefahren. |
| `[xp_wellys_devfr_atc] BACKEND MODE: MISTRAL (api.mistral.ai) ...` | Der Loader hat die Mistral-Cloud-Pipeline hochgefahren. |
| `[STT-LOCAL] / [LM-LOCAL] / [TTS-LOCAL]` | Per-Aufruf-Audit für jede lokale Inferenz. |
| `[STT-OPENAI] / [LM-OPENAI] / [TTS-OPENAI]` | Per-Aufruf-Audit für jede OpenAI-Cloud-Inferenz. Der API-Key wird auf seine letzten 4 Zeichen gekürzt (`sk-...ABCD`). |
| `[STT-MISTRAL] / [LM-MISTRAL] / [TTS-MISTRAL]` | Per-Aufruf-Audit für jede Mistral-Cloud-Inferenz. Der API-Key wird auf seine letzten 4 Zeichen gekürzt (`...ABCD`; kein `sk-`-Präfix — Mistral-Keys sind nicht OpenAI-formatiert). |

## Funkstörungs-Wiederherstellung (TTS-Fehler-Schutz)

Der pilotengetriebene TTS-Pfad ist in einen Snapshot/Revert-Guard
gewickelt, sodass ein Synthese- oder Wiedergabefehler (OpenAI
`curl error: Timeout`, transientes 5xx, abgebrochenes WLAN, lokaler
Piper-IO-Fehler, Audio-Bus-Störung) die ATC-State-Machine nicht über das
hinaus bringen kann, was der Pilot tatsächlich gehört hat. Der
Mechanismus ist über alle Backend-Modi einheitlich — derselbe Code-Pfad
behandelt Local und Cloud.

So funktioniert es:

- Bevor jede Pilotenübertragung in `atc_state_machine::process()` geht,
  erfasst das Plugin einen opaken Snapshot des vollständigen
  Maschinenzustands (aktueller State, Übergangshistorie, Pisten-Lock,
  Readback-Flag, Abflugtyp, letzter Freigabetext, letzte Tower-Äusserung).
  Ein monotoner Generations-Zähler wird bei jeder semantischen Mutation
  erhöht — Per-Frame-Heartbeats (Zeitstempel, Auto-Korrektur-Timer)
  werden nicht gezählt, sodass sie den Snapshot nicht invalidieren können.
- Bei TTS-Erfolg: nichts weiter passiert. Der State läuft wie zuvor
  weiter, der Pilot hört die Antwort, der Snapshot wird verworfen.
- Bei TTS-Fehler: ein kurzer Squelch-Burst (~350 ms rosa Rauschen plus
  ein Klick) wird auf der aktiven COM gespielt. Der Burst wird in-process
  aus einem deterministisch geseedeten PRNG erzeugt — er kann nicht auf
  dieselbe Weise scheitern wie der TTS-Aufruf gerade, und er
  funktioniert in VR oder unter der IFR-Haube, wenn das Panel nicht
  sichtbar ist. Dann läuft einer von zwei Zweigen:
  - **Restore-Zweig** — niemand sonst hat die State-Machine inzwischen
    verändert. Der Vor-Übertragungs-Snapshot wird wiederhergestellt, das
    Transkript-Panel zeigt einen gedämpft-bernsteinfarbenen
    System-Eintrag `-- Funkstörung — bitte den Funkspruch wiederholen --`,
    und der Pilot kann denselben Funkspruch sauber erneut absetzen.
  - **Stale-Zweig** — eine spätere Auto-Korrektur (oder ein anderer
    Callback) hat den Generations-Zähler bereits über den vom Snapshot
    erwarteten Wert hinaus bewegt. Ein Rollback würde diesen legitimen
    Übergang stillschweigend rückgängig machen, also wird der Rollback
    abgelehnt. Der Freigabetext, den der Pilot nie gehört hat, liegt
    weiterhin in `last_tower_response_text_` geparkt; ein System-Eintrag
    `-- Funkstörung — sagen Sie 'Wiederholen Sie' für die verpasste
    Anweisung --` lenkt den Piloten zum `REQUEST_REPEAT`-Pfad, der die
    verpasste Freigabe wortgetreu wiederholt. Nach der Wiederholung
    liest der Pilot normal zurück und die State-Machine synchronisiert
    sich neu.

ATIS-Ansagen, Verkehrshinweise und der unaufgeforderte
Durchstart-Prompt nutzen den ungeschützten TTS-Pfad — sie sind
zustandslose Render-only-Ereignisse. Fällt ein Tick aus, versucht es der
nächste Tick einfach erneut.

Implementierung:

- `src/atc/atc_state_machine.{hpp,cpp}` — `AtcStateSnapshot`,
  `capture_snapshot()`, `current_gen()`, `restore_snapshot_if_gen()`,
  Generations-Zähler-Disziplin (ein Banner-Kommentar in der cpp-Datei
  legt dar, welche Felder gen erhöhen und welche nur Heartbeat sind).
- `src/atc/atc_session.cpp` — `speak_response_guarded()` umhüllt den
  `engine::process_transcript`-Callback für zustandsändernde
  Tower-Antworten.
- `src/audio/audio_player.{hpp,cpp}` — `play_squelch_burst(com)`, kein
  WAV-Asset, kein Netzwerk.
- `tests/test_state_revert_guard.cpp` — vier Verhaltensfälle:
  Snapshot+Restore-Round-Trip, Generations-Monotonie,
  Stale-Zweig-Ablehnung, `REQUEST_REPEAT`-nach-Stale-Wiederherstellung.

## Aus Quellcode bauen

```sh
git clone --recurse-submodules <repo-url>
cd xp_wellys_vfr_atc
make setup     # X-Plane SDK, Dear ImGui, nlohmann/json, Catch2, Spike-Submodule
make build     # Universal-Release-Build → build/xp_wellys_devfr_atc.xpl (arm64
               # mit allen drei Backends + x86_64 cloud-only, zu einem
               # .xpl lipo'd). Das ist das einzige Build-Target — es gibt
               # keinen arm64-only-Schnellpfad mehr.
make install   # Code-Signing + Installation ins X-Plane-Plugins-Verzeichnis
```

`make build` führt CMake zweimal aus (arm64 mit
`XPWELLYS_USE_LOCAL_INFERENCE=ON` in `build-arm64/`, x86_64 mit demselben
Flag `OFF` in `build-x86_64/`) und `lipo`-merged die zwei `.xpl`s zu
einem Universal Binary. Die Build-Zeit ist etwa doppelt so lang wie ein
Single-Arch-Build; das ist der bewusste Kompromiss, damit Dev- und
Release-Artefakte in ihrer Form Byte-für-Byte identisch sind. Für
Tag-getriebene Release-Builds übergibst du `RELEASE_FLAG=-DRELEASE=ON`
(`make release-build` erledigt das für dich — bettet die Version aus
`VERSION.txt` ein).

Der Build lädt beim ersten Konfigurieren onnxruntimes vorgebautes
arm64-dylib (≈ 33 MB) nach
`spikes/spike_piper/third_party/piper1-gpl/libpiper/lib/`. Danach ist
alles lokal. Der x86_64-Slice hat keinerlei onnxruntime- / Piper- /
whisper- / llama-Abhängigkeit; er linkt nur gegen libcurl + die
System-Frameworks (Security, AudioToolbox usw.) und die Cloud-Clients.

## Lokale Inferenz-Modelle

Das Plugin wird **ohne** die Modell-Dateien ausgeliefert (~2,0 GB
zusammen). Sie liegen unter `<plugin>/Resources/models/` und werden beim
ersten Start über den **Models**-Tab geladen. Jeder Download ist HTTPS,
fortsetzbar (`Range`-Header), direkt auf das Installationsvolume
gestreamt (kein Temp-Umweg über die System-Disk — wichtig für Nutzer,
die X-Plane auf einer externen SSD betreiben) und SHA256-verifiziert,
bevor er von `<file>.part` zum finalen Dateinamen umbenannt wird.

### Manueller Fallback (restriktive Netzwerke)

Falls der Plugin-Downloader HuggingFace nicht erreicht (Firmen-Proxy,
Captive Portal etc.), lade diese Dateien manuell und lege sie in
`<plugin>/Resources/models/`. Das Plugin verifiziert beim nächsten Start
erneut und lädt sie automatisch, wenn die Hashes passen.

| Modell | Sprache | Grösse | SHA256 | URL |
|---|---|---:|---|---|
| `ggml-large-v3-turbo-q5_0.bin` | de (multilingual) | 547 MB | `394221709cd5ad1f40c46e6031ca61bce88931e6e088c188294c6d5a55ffa7e2` | [`huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo-q5_0.bin`](https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo-q5_0.bin) |
| `Llama-3.2-3B-Instruct-Q4_K_M.gguf` | — | 1.88 GB | `6c1a2b41161032677be168d354123594c0e6e67d2b9227c84f296ad037c728ff` | [`huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-Q4_K_M.gguf`](https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-Q4_K_M.gguf) |
| `de_DE-thorsten-medium.onnx` | de | 60 MB | `7e64762d8e5118bb578f2eea6207e1a35a8e0c30595010b666f983fc87bb7819` | [`huggingface.co/rhasspy/piper-voices/resolve/main/de/de_DE/thorsten/medium/de_DE-thorsten-medium.onnx`](https://huggingface.co/rhasspy/piper-voices/resolve/main/de/de_DE/thorsten/medium/de_DE-thorsten-medium.onnx) |
| `de_DE-thorsten-medium.onnx.json` | de | 4.7 KB | `974adee790533adb273a1ac88f49027d2a1b8f0f2cf4905954a4791e79264e85` | [`huggingface.co/rhasspy/piper-voices/resolve/main/de/de_DE/thorsten/medium/de_DE-thorsten-medium.onnx.json`](https://huggingface.co/rhasspy/piper-voices/resolve/main/de/de_DE/thorsten/medium/de_DE-thorsten-medium.onnx.json) |

Das Whisper-Modell ist die multilinguale Variante
(`ggml-large-v3-turbo-q5_0.bin`), da das DE-Profil deutsche Transkription
braucht — der destillierte large-v3-turbo-Decoder erkennt buchstabierte
Rufzeichen (NATO-Alphabet) deutlich zuverlässiger als das frühere
`small`-Modell, bei nahezu gleichem Tempo. Llama ist multilingual
und wird geteilt. Die Piper-Stimme ist die deutsche `de_DE-thorsten-medium`.

Nachdem du die Dateien abgelegt hast, öffne das Plugin-Fenster erneut —
der Models-Tab führt die SHA256-Verifizierung im Hintergrund aus und
schaltet die Zeilen auf **Ready**, sobald jeder Hash passt.

### SHA256-Verifizierungs-Prozedur (DE-Modelle)

Die DE-Hashes oben wurden am 2026-06-04 gegen HuggingFace `main` erfasst.
Zum erneuten Verifizieren (oder Neu-Pinnen nach einem Upstream-Update):

```bash
# Whisper large-v3-turbo multilingual (~547 MB)
curl -L -o /tmp/ggml-large-v3-turbo-q5_0.bin \
  https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo-q5_0.bin
shasum -a 256 /tmp/ggml-large-v3-turbo-q5_0.bin
stat -f%z /tmp/ggml-large-v3-turbo-q5_0.bin

# Piper de_DE-thorsten-medium (.onnx ~63 MB, .onnx.json ~5 KB)
curl -L -o /tmp/de_DE-thorsten-medium.onnx \
  https://huggingface.co/rhasspy/piper-voices/resolve/main/de/de_DE/thorsten/medium/de_DE-thorsten-medium.onnx
curl -L -o /tmp/de_DE-thorsten-medium.onnx.json \
  https://huggingface.co/rhasspy/piper-voices/resolve/main/de/de_DE/thorsten/medium/de_DE-thorsten-medium.onnx.json
shasum -a 256 /tmp/de_DE-thorsten-medium.onnx /tmp/de_DE-thorsten-medium.onnx.json
stat -f%z /tmp/de_DE-thorsten-medium.onnx /tmp/de_DE-thorsten-medium.onnx.json
```

Trage die Hashes + Grössen ein in:
- `src/persistence/model_manifest.cpp` `voice_catalog()` (Thorsten-Zeile:
  zwei Hashes + zwei Grössen)
- `src/persistence/model_manifest.cpp` `manifest()` (multilinguales
  Whisper: ein Hash + eine Grösse)
- Die Tabelle oben

### Erwartete Download-Zeit beim ersten Start

5–30 Minuten bei typischem Heim-Internet; der Flaschenhals ist
HuggingFaces Download-Durchsatz, nicht das Plugin. Der Downloader setzt
über HTTP-`Range` fort, wenn die Verbindung abbricht, sodass ein
WLAN-Aussetzer mitten im Llama-Download den 1,88-GB-Pull nicht von vorn
beginnt.

## Konfiguration

Die Einstellungen liegen in `<plugin>/data/settings.json`. Die OpenAI-
und Mistral-API-Keys sind die einzigen Geheimnisse — beide liegen im
macOS Keychain unter separaten Service-Einträgen
(`com.xp_wellys_devfr_atc.openai`, `com.xp_wellys_devfr_atc.mistral`), nie in dieser
Datei.

Das Plugin ist fest auf das **DE-Profil** (NfL DACH-VFR) eingestellt; es
gibt keine Profilauswahl mehr (`atc_profile` ist konstant `DE`, die
Backend-Sprache konstant `de`).

| Einstellung | Standard | Beschreibung |
|---|---|---|
| `pilot_callsign` | *(leer)* | Phonetisches Rufzeichen (in den Plugin-Einstellungen gesetzt) |
| `active_com` | `1` | Aktives COM-Funkgerät (1 oder 2) |
| `volume` | `1.0` | Wiedergabelautstärke (0.0–1.0) |
| `pattern_direction` | `left` | Standard-Platzrundenrichtung (left/right) — pro Flugplatz/Piste durch `airport_vrps.json` überschrieben |
| `disable_default_atc` | `false` | Unterdrückt das eingebaute Standard-ATC von X-Plane |
| `skip_radio_power_check` | `false` | Umgeht die Funkstrom-Erkennung (Workaround für exotische Flugzeuge) |
| `show_phraseology_hints` | `true` | Zeigt den Phraseologie-Spickzettel im ATC-Panel |
| `auto_correction_factor` | `1.0` | ATC-Erholungszeit-Multiplikator (0.5 = schneller, 2.0 = langsamer) |
| `start_mode` | `engines_running` | Vom State-Machine angenommener Startzustand. `engines_running` (Standard) setzt den Piloten mit warmen Triebwerken auf das Vorfeld → erster Funkspruch ist Ground zum Rollen; `cold_and_dark` erlaubt eine Clearance-Delivery- / Engine-Start-Sequenz vor dem Rollen. |
| `bzf_strict_mode` | `false` | Wenn `true`, prüft der Tower jeden READBACK gegen die NfL §25 b) Nr. 1 Pflichtliste (Piste, QNH, Frequenz, Squawk, Rufzeichen) und antwortet mit einem korrigierenden Hinweis, falls etwas fehlt — der State läuft **nicht** weiter, bis der Readback sauber ist. Der Toggle im Settings-Tab ist immer sichtbar. Siehe [DE-Profil & BZF-Phraseologie](#de-profil--bzf-phraseologie). |
| `debug_logging` | `false` | Aktiviert ausführliche Debug-Ausgabe |
| `debug_traffic` | `false` | Zeigt den Traffic-Tab im ATC-Panel (listet die 10 nächsten Flugzeuge aus den TCAS-DataRefs) |
| `debug_text_input` | `false` | Zeigt unter dem Transkript im Status-Tab ein InputText-Feld. Getippter Text wird direkt in `engine::process_transcript` via `atc_session::submit_text()` eingespeist — STT wird übersprungen, LM + State-Machine + TTS laufen wie im Sprachpfad. Hilfreich ohne Headset und zum Isolieren von ATC-Logik-Bugs von STT-Fehlern. PTT bleibt parallel aktiv; das Kürzel `REG` expandiert zum phonetischen Rufzeichen. |
| `traffic_features_enabled` | `true` | Hauptschalter für das Verkehrs-Subsystem (Hinweise, Lande-Sequenzierung, Durchstart-Trigger). Aus → `traffic_context::update()` liefert einen leeren Snapshot und jeder nachgelagerte Konsument wird zum No-op. Braucht ohnehin einen Verkehrs-Provider (LiveTraffic, xPilot, swift, X-IvAp, native AI). |
| `backend_mode` | `local` | `local` (whisper + llama + Piper, nur arm64), `openai` (Whisper API + Chat Completions + TTS API) oder `mistral` (Voxtral STT + Mistral Chat Completions + Voxtral TTS). Der x86_64-Slice schreibt `local` beim Start still auf `openai` um, da Local dort nicht verfügbar ist; `mistral` wird auf beiden Slices honoriert. |
| `api_key_saved` | `false` | Nur Flag — automatisch gesetzt, wenn der Nutzer in den Einstellungen **Save Key** klickt. Der echte OpenAI-Key liegt im macOS Keychain unter Service `com.xp_wellys_devfr_atc.openai` / Account `default`. Durch **Delete Key** gelöscht. |
| `openai_stt_model` | `whisper-1` | OpenAI-Whisper-Modell-ID für den STT-Aufruf. |
| `openai_lm_model` | `gpt-4o-mini` | OpenAI-Chat-Completions-Modell-ID für den Absichts-Classifier. JSON-Modus wird automatisch aktiviert. |
| `openai_tts_model` | `tts-1` | OpenAI-TTS-Modell-ID. Setze `tts-1-hd` für höhere (langsamere) Qualität. |
| `openai_tts_voice_atis` / `openai_tts_voice_tower` / `openai_tts_voice_ground` | `onyx` / `echo` / `alloy` | OpenAI-Stimme je Rolle. Eine von `alloy / echo / fable / onyx / nova / shimmer`. `onyx` kommt echtem ATC am nächsten. |
| `mistral_api_key_saved` | `false` | Nur Flag — gesetzt, wenn **Save Key##mistral** geklickt wird. Der echte Mistral-Key liegt im macOS Keychain unter Service `com.xp_wellys_devfr_atc.mistral` / Account `default`, getrennt vom OpenAI-Eintrag. |
| `mistral_stt_model` | `voxtral-mini-2507` | Voxtral-STT-Modell-ID. |
| `mistral_lm_model` | `mistral-small-latest` | Mistral-Chat-Completions-Modell-ID für den Absichts-Classifier. JSON-Modus automatisch. `ministral-3b-latest` / `ministral-8b-latest` funktionieren ebenfalls und sind günstiger. |
| `mistral_tts_model` | `voxtral-mini-tts-2603` | Voxtral-TTS-Modell-ID. |
| `mistral_tts_voice_atis` / `mistral_tts_voice_tower` / `mistral_tts_voice_ground` | `gb_oliver_neutral` / `en_paul_confident` / `en_paul_neutral` | Voxtral-Preset-Stimme je Rolle. Das UI-Dropdown listet 30 Stimmen britischer (`gb_oliver_*`, `gb_jane_*`), amerikanischer (`en_paul_*`) und französischer (`fr_marie_*`) Sprecher in 7–9 emotionalen Registern. Voxtral TTS ist multilingual und spricht Deutsch ohne US-Akzent. Eigene Voice-Clones aus dem Mistral-Dashboard lassen sich durch direktes Editieren dieses Felds in `settings.json` setzen. |

Die ATC-Antwort-Templates liegen in
`data/atc_profiles/de/atc_templates.json`. Flugphasen-Schwellwerte,
ATC-Vorbedingungs-Guards, Frequenz-Guards und Auto-Korrektur-Regeln
stehen in `data/atc_profiles/de/flight_rules.json`. Alle Daten-Dateien
lassen sich ohne Neubau des Plugins editieren.

### Flugplatz-Datenbank (`data/vrps/airport_vrps.json`)

Pro-Flugplatz-Konfiguration für Sichtmeldepunkte (VRPs) und
Platzrundenrichtungen. Eine globale Datei — VRPs sind geografische
Fakten aus dem AIP, keine Phraseologie. Vorbefüllt für gängige
Schweizer und deutsche VFR-Plätze; andere Plätze liefern nur
`pattern_direction` (`vrps: []`), bis sie gegen eine autoritative
Quelle geprüft sind. Jeder Top-Level-Schlüssel ist ein ICAO-Code mit
optionalen Feldern:

- `name` — Anzeigename
- `pattern_direction` — pro Piste `"left"` / `"right"` (überschreibt die
  globale `pattern_direction`-Einstellung); akzeptiert einen String für
  einen unbedingten Standard oder ein nach Pistenbezeichnung
  geschlüsseltes Objekt mit optionalem `_default`
- `vrps` — Array von `{ name, lat, lon, alt_ft }`; `name` ist die
  phonetische Schreibweise (z. B. `"November"`), damit Whisper und Piper
  sie sauber verarbeiten
- `arrival_routes` — pro Piste geordnete Liste von VRP-Namen für die
  Anflug-Führung
- `_source` / `_comment` — optionale Audit-Annotationen; vom Loader
  ignoriert

#### Optionaler Benutzer-Override (Navigraph-Charts-Workflow)

Mit einem **Navigraph-Charts**-Abo kannst du eigene VRP-Koordinaten
liefern, ohne das Plugin zu forken:

1. Lege eine JSON-Datei unter
   `<X-Plane>/Output/preferences/xp_wellys_devfr_atc/airport_vrps.json` ab. Das
   Verzeichnis wird beim ersten Plugin-Start erstellt. Dieser Pfad
   übersteht Plugin-Neuinstallationen.
2. Nutze dasselbe Schema wie die mitgelieferte Datei. Pro-ICAO-Einträge
   ersetzen die Plugin-Standards vollständig — es gibt kein
   Feld-Merging, also nimm den kompletten Eintrag für jeden Flugplatz,
   den du überschreiben willst.
3. Starte X-Plane neu (oder `Reload Settings` aus dem Menü) — ein
   Log-Banner in `Log.txt` bestätigt das Laden:
   `Airport VRPs loaded: N airports (X plugin, Y user overrides: Z replaced, W added) from <path>`

Navigraph-Charts-Workflow pro Flugplatz:
- Öffne die **VFR-Anflugkarte** (deutsche Karten: AD 2 EDxx, Abschnitt
  *Visual Approach* bzw. *VFR-Anflug*).
- Lies den VRP-Code (W/N/E/S/Z…), übersetze ihn in den phonetischen
  Namen (`W` → `Whiskey`, `N` → `November`, …) — das ist, was Whisper
  transkribiert und Piper ausspricht.
- Überfahre die Karte für die Cursor-lat/lon (Navigraph Charts zeigt die
  Zeiger-Koordinaten in der Toolbar).
- Lies die veröffentlichte Transit-Höhe aus der Kartenlegende.
- Notiere die Platzrundenrichtung pro Piste aus dem AIP AD 2.22 (Flight
  Procedures).

Das Navigraph-**FMS-Data**-Add-on für X-Plane Custom Data enthält *keine*
VRPs (ARINC-424 ist IFR-only). Du brauchst das Navigraph-**Charts**-Produkt.

### ATC-Antwort-Templates (`data/atc_profiles/de/atc_templates.json`)

Definiert den ATC-Antworttext für jede Kombination aus Flugplatztyp,
ATC-State und Pilotenabsicht. Abschnitte `towered` (voller ATC-Ablauf)
und `uncontrolled` (CTAF/UNICOM-Selbstansage); jeder Eintrag hat
`response`, `next_state`, `requires_readback`. Der spezielle Schlüssel
`_INVALID` ist der Fallback. Variablen werden zur Laufzeit aus dem
`XPlaneContext` substituiert.

### Flugregeln (`data/atc_profiles/de/flight_rules.json`)

Abschnitte für Phasen-Erkennungs-Schwellwerte + Hysterese,
Absichts-Vorbedingungen, Auto-Korrektur-Regeln (State und Frequenz),
Absicht-zu-Frequenz-Mapping, Piloten-Phraseologie, State-Machine-Guards
(`state_frequency_validity`, `idle_redirects`, `state_reverts`,
`tower_only_auto_advance`) und Frequenz-Hinweistexte.

### LLM-Prompt-Templates (`data/atc_prompt_templates.json`)

Prompts, die die Engine an das Sprachmodell sendet:

| Schlüssel | Zweck |
|---|---|
| `whisper_prompt` | Initial-Prompt-Hinweis für whisper.cpp, um die Transkription auf Luftfahrt-Vokabular und das NATO-Alphabet zu biasen |
| `gpt_classify_prompt_de` | System-Prompt für die Absichts-Klassifizierung bei geringer Konfidenz (Variablen: `{state}`, `{valid_intents}`, `{transcript}`, `{frequency_type}`, `{on_ground}`, `{altitude_ft}`, `{groundspeed_kts}`, `{airport}`) |
| `gpt_fallback_prompt_de` | Reserve-Prompt für die Notfall-Antwortgenerierung |

Der Schlüsselname behält das `gpt_*`-Präfix aus Kompatibilitätsgründen;
die lokale Pipeline füttert diesen Prompt unverändert an Llama 3.2.

**Push-to-Talk** wird über die Tastatur- oder Joystick-Einstellungen von
X-Plane konfiguriert. Das Plugin registriert den Befehl
`xp_wellys_devfr_atc/ptt`, der an eine beliebige Taste oder einen
Joystick-Button gebunden werden kann.

## Benutzung

1. Stimme COM1/COM2 in X-Plane auf die passende Frequenz (oder klicke
   eine Frequenz im ATC-Panel, um sie als Standby zu setzen, dann
   Flip-Flop).
2. Halte die PTT-Taste und sprich deinen Funkspruch — das
   **Phraseologie-Hinweise**-Panel zeigt dir, was zu sagen ist (überfahren
   für die volle Phraseologie).
3. Lass PTT los — das Plugin transkribiert, verarbeitet durch die
   State-Machine und spielt die ATC-Antwort zurück.
4. Prüfe das ImGui-Overlay für Transkript-Historie und aktuellen
   ATC-State.
5. Wenn du in einer Schleife feststeckst, klicke **Disregard** zum
   Zurücksetzen.

**Kein Headset?** Schalte `debug_text_input` in den Einstellungen ein —
ein InputText-Feld erscheint unter dem Transkript im Status-Tab. Getippter
Text geht direkt in die Engine (STT wird übersprungen), aber LM,
State-Machine und TTS laufen weiter, sodass die Tower-Antwort normal über
das aktive Backend gesprochen wird. Das Kürzel `REG` expandiert zu deinem
phonetischen Rufzeichen.

## Make-Targets

```sh
make all           # clean + format + build + lint + test (volle lokale CI)
make build         # universal: arm64 (local + beide Clouds) + x86_64 (nur Clouds), lipo'd
make release-build # wie `make build`, aber mit -DRELEASE=ON (bettet VERSION.txt ein)
make test          # Unit-Tests + Szenario-Tests
make install       # Code-Signing + Installation in X-Plane
make repl          # baut das headless atc_repl-Tool
make format        # clang-format
make lint          # clang-tidy (manche Regeln als Fehler hochgestuft)
make clean         # entfernt build/, build-arm64/, build-x86_64/, build-lint/, build-sanitize/
make distclean     # entfernt zusätzlich sdk/, vendor/
```

## Bekannte Einschränkungen

### DE-Profil & BZF-Phraseologie

Das DE-Profil orientiert sich an der NfL Sprechfunk 2024 (DACH-VFR-Phraseologie).
Keine offizielle Zertifizierung, kein Prüfungsersatz — Korrekturen von BZF-Inhabern
ausdrücklich willkommen.

**Stand der Umsetzung** (BZF-Coverage-Matrix Re-Anchor 2026-06-05):

- **Wortlaut-Korrekturen (Bucket B)** — fünf NfL-Patches im `de`-Profil: Funkprobe-
  Antwort „Höre Sie fünf.", Touch-and-Go-Templates „frei zum Aufsetzen und
  Durchstarten" (3×), Pilot-Keyword „aufsetzen und durchstarten", Frequenzwechsel-
  Genehmigung „Verlassen der Frequenz genehmigt" (2×), Fallback „wiederholen Sie"
  statt „sagen Sie nochmals" (3×, NfL §18 c) Nr. 4).
- **Callsign-Aussprache verifiziert (Bucket C)** — `de_phraseology::expand_callsign_phonetic()`
  expandiert D-/HB-/N-Präfix ziffernweise (z. B. `N123AB` → „November eins zwo
  drei Alfa Bravo"), abgesichert durch 7 Catch2-Tests in `tests/test_de_phraseology.cpp`.
- **Strict-Mode-MVP (Bucket A)** — Settings-Toggle `bzf_strict_mode` (Default
  aus), SDK-freier `src/atc/bzf_compliance.{hpp,cpp}`, `apply_bzf_strict_check()`-
  Hook im State-Machine mit `last_clearance_text_`-Tracking. Greift beim READBACK-
  Intent gegen die NfL §25 b) Nr. 1 Pflichtliste (Piste, QNH, Frequenz, Squawk,
  Rufzeichen) — 18 Catch2-Tests in `tests/test_bzf_compliance.cpp`.

| Einschränkung | Auswirkung | Aufwand |
|---|---|---|
| **Lokale Inferenz nur auf Apple Silicon** | Intel-Macs können das Plugin über den x86_64-Slice fahren, aber nur im OpenAI- oder Mistral-Cloud-Modus (API-Key + Abrechnung nötig) | Durch das Universal Binary gelöst; die Intel-Beschränkung für den Local-Modus aufzuheben bräuchte Metal-Alternativen + einen x86_64-onnxruntime-Build |
| **Nur Deutsch** | Das Plugin modelliert ausschliesslich deutsche NfL-DACH-VFR-Phraseologie; andere Sprachen sind nicht vorgesehen | Per Design — dies ist ein reines Deutschland-VFR-Plugin |
| **OpenAI-Stimmen sprechen Deutsch mit US-Akzent** | Im `backend_mode=openai` transkribiert Whisper korrekt und das LM antwortet korrekt auf Deutsch, aber die `tts-1`-Stimmen (`alloy`, `echo`, `fable`, `onyx`, `nova`, `shimmer`) sind englisch-trainiert und geben Deutsch mit hörbarem US-Akzent wieder — besonders NATO-Buchstaben klingen anglophon (z. B. „Tschaar-lie" statt „Tschar-li"). Für lockeres Üben akzeptabel, für BZF/AZF-Training unrealistisch. | Für den Local-Modus durch Piper `de_DE-thorsten` gelöst. Für Cloud-Nutzer ist **Mistral Cloud** die Alternative — Voxtral TTS ist nativ multilingual und spricht Deutsch ohne US-Akzent. |
| **Single-Voice-TTS** | Alle ATC-Sprecher (Tower, Ground, ATIS) nutzen im Local-Modus dieselbe Piper-Stimme; ATIS spricht langsamer via `length_scale=1.18` | Gering — könnte mehr Stimmen ausliefern und einen Per-Frequenz-Selektor ergänzen |
| **„via Alpha" hartkodiert** — der Rollweg-Name ist immer Alpha | Unrealistisch an Flugplätzen mit anderem Rollweg-Layout | Hoch — bräuchte Rollweg-Daten aus apt.dat oder WED |
| **Keine Wirbelschleppen-Staffelung** — die Sequenzierung in v2.2 wählt nur nach Distanz, keine Light/Medium/Heavy-Trennung | Für GA-Platzrunden akzeptabel; fehlt für gemischte Gewichtsklassen | Phase 5 auf der Roadmap |
| **Keine Rufzeichen-Validierung** | ATC akzeptiert jedes Rufzeichen | Geringe Priorität im Single-Player |
| **Grosse Hub-Flugplätze (LSZH, LSGG, …) nicht offiziell unterstützt** — Pilot kann an-/abfliegen, aber Delivery-Workflow (Slot/VFR-Clearance), RWY-spezifisches Tower-Routing und AIP-VFR-Meldepunkte sind nicht modelliert | Generische Hinweise an grossen Hubs entsprechen nicht den realen Verfahren | Hoch — bräuchte AIP-Recherche pro Flugplatz + neuen Delivery-Intent + Slot-Einstellung + Multi-Tower-Disambiguierung |

## FAQ

**Unterstützt das Plugin IFR oder Flugplanung?**
Nein — das Plugin ist VFR-only. Keine IFR-Freigaben, keine
Flugplanaufgabe, keine FMS-/Routen-Integration.

**Wird es einen virtuellen Co-Piloten oder Checklisten-Reader geben?**
Aktuell nicht geplant. Das Plugin ist eine Single-Pilot-Pilot↔ATC-
Sprachschnittstelle; Intercom und Checklisten sind nicht implementiert.

**Ist es mit allen XP12-Flugzeugen und Add-ons kompatibel?**
Im Prinzip ja. Das Plugin ist flugzeug-agnostisch und nutzt nur
Standard-X-Plane-DataRefs — keine flugzeugspezifischen Code-Pfade, keine
Kompatibilitätsliste. Es funktioniert mit der Standard-Flotte (C172 etc.)
und jedem Add-on, das die Standard-`sim/cockpit/radios/*`-DataRefs
bereitstellt. Für exotische Flugzeuge ohne `com_power` setze
`skip_radio_power_check: true` in `settings.json`. Laminars Standard-ATC
lässt sich über `disable_default_atc` unterdrücken.

**Kann ich am Steuerhorn fliegen, ohne das Plugin-Fenster zu fokussieren?**
Ja — so ist es gedacht. Binde Push-to-Talk einmal an einen Yoke-Button
oder eine Taste (X-Plane-Befehl `xp_wellys_devfr_atc/ptt`). Danach ist jede
Interaktion Sprache: PTT drücken, sprechen, loslassen, ATC-Antwort hören.
Das Plugin-Fenster braucht im Flug keinen Tastaturfokus, und jede
Inferenz läuft auf Hintergrund-Threads, sodass X-Plane nie stockt.

**Liest das Plugin meine COM1/COM2-Frequenzen automatisch?**
Ja. Aktive und Standby-Frequenzen beider COM-Funkgeräte werden live aus
X-Plane-DataRefs gelesen. Das Plugin erkennt auch, welches Funkgerät
aktiv ist, und klassifiziert den Frequenztyp (ATIS / Ground / Tower /
Approach / UNICOM) automatisch gegen die apt.dat-Frequenz-Datenbank.
Keine manuelle Frequenzeingabe.

**Setzt das Plugin den Transponder / Squawk-Code?**
Nein — nur gesprochen. ATC kann „Squawk 7000" sagen, aber das Plugin
liest oder schreibt die Transponder-DataRefs nicht. Du stellst den
Squawk manuell ein.

**Wie verhält es sich zu BeyondATC oder SayIntentions?**
Stärken: 100 % Offline-Option auf Apple Silicon (kein Abo, keine Cloud,
kein dauerndes Internet nötig — nach Ermessen des Nutzers), ~1,16 s
warme Pipeline-Latenz im Local-Modus, deutsche NfL-DACH-VFR-Phraseologie
mit realistischen Tower-Reaktionen auf Pilotenfehler. Zwei
Cloud-Optionen — **OpenAI** und **Mistral** — sind als bezahlte Opt-ins
(eigener Key) verfügbar. Mistral kostet pro Token meist weniger und ist
die sauberere Wahl für deutsches ATC, da Voxtral TTS Deutsch nativ
spricht.
Heutige Grenzen: VFR-only, kein IFR, kein Routing, keine
Wirbelschleppen-Staffelung (Sequenzierung in v2.2 nur distanzbasiert —
Phase 5 auf der Roadmap), kein Transponder-Datenlink, kein Co-Pilot.

**Gibt es ein Einführungsvideo?**
Noch nicht.

**Wie verhält es sich zu OpenSquawk?**
Noch nicht bewertet.

## Projektstruktur

```
src/
├── main.cpp                # XPlugin*-Einstiegspunkte, Menü, Flight-Loop
├── atc/                    # Session-Koordinator, State-Machine, Intent-
│                           #   Parser + Rules, Templates, ATIS, Flug-
│                           #   phase, Engine, traffic_advisor /
│                           #   traffic_dialog, landing_sequence,
│                           #   phraseology_hints, DE-spezifisch:
│                           #   bzf_compliance + de_phraseology, plus
│                           #   flows/ (ground_operations, pattern_flow,
│                           #   crosscountry_flow, flow_coordinator)
├── audio/                  # Push-to-talk, Mic-Capture, PCM-Wiedergabe
│                           #   auf dem X-Plane-Funkbus (COM1 oder COM2),
│                           #   Mic-Berechtigung
├── backends/               # Strategie-Interfaces + manager (async
│                           #   Dispatch) + loader (verify + load) +
│                           #   downloader (libcurl + resume + SHA256).
│                           #   Konkrete Backends nach Modus getrennt:
│                           #   Local: WhisperStt / LlamaLm / PiperTts
│                           #     (nur arm64-Slice, gated auf
│                           #     XPWELLYS_USE_LOCAL_INFERENCE).
│                           #   OpenAI: OpenAiStt / OpenAiLm / OpenAiTts
│                           #     (beide Slices, libcurl + JSON).
│                           #   Mistral: MistralStt / MistralLm /
│                           #     MistralTts (beide Slices, libcurl +
│                           #     JSON; Voxtral TTS liefert ein JSON-
│                           #     Envelope mit base64-kodiertem WAV).
│                           #   Die drei Client-Sätze teilen weder Header
│                           #   noch Code-Pfad — Audit-Invariante durch
│                           #   Tests erzwungen.
├── core/                   # Logging, XPlaneContext (SDK-freier Struct +
│                           #   SDK-gekoppelter DataRef-Reader)
├── data/                   # Airport-VRPs, apt.dat-abgeleiteter Airspace-
│                           #   Index, traffic_context (Struct + 2-Hz-
│                           #   TCAS-Reader), traffic_geometry +
│                           #   traffic_phase_classifier
├── persistence/            # settings.json, keychain (OpenAI- + Mistral-
│                           #   API-Keys), model_paths, model_manifest,
│                           #   models_catalog
└── ui/                     # Dear-ImGui-ATC-Panel + Models- + Traffic-
                            #   Tabs, ui_strings (i18n), clipboard-Helfer
```

Die CMake-**OBJECT**-Bibliothek `xp_atc_engine` kompiliert die SDK-freien
Übersetzungseinheiten (`atc/`, `core/logging`, `core/xplane_context`-Struct,
`data/`, `backends/manager.cpp`, `persistence/model_manifest`,
`persistence/models_catalog`). Sowohl das Plugin-Modul als auch das
headless `atc_repl`-Tool nutzen sie wieder. Das Plugin-Modul ergänzt die
SDK-gekoppelten Einheiten (`main.cpp`, `audio/`,
`core/xplane_context_runtime.cpp`,
`backends/{loader,downloader,openai_*,mistral_*}.cpp`,
`persistence/{settings,model_paths,keychain}.cpp`, `ui/atc_ui.cpp`). Der
arm64-Slice kompiliert zusätzlich
`backends/{whisper_stt,llama_lm,piper_tts}.cpp` und linkt statisch gegen
`whisper`, `llama`, `common` plus ein geteiltes `libpiper.dylib`, das
`libonnxruntime.1.22.0.dylib` über `@loader_path` auflöst — beide dylibs
liegen im Plugin-Bundle neben dem `.xpl`. Der x86_64-Slice hat keine
dieser Abhängigkeiten; er linkt nur libcurl + Security + die
Audio-Frameworks und liefert beide Cloud-Provider-Clients.

## Drittanbieter-Abhängigkeiten

Siehe [`THIRD_PARTY.md`](THIRD_PARTY.md) für die vollständige Liste der
gebündelten oder gelinkten Bibliotheken, ihre Lizenzen und wie sie
vendored sind.

## Entwicklungs-Workflow

### CI-Pipeline

Die GitHub-Actions-Pipeline läuft nur in zwei Situationen:

- **Pull Request gegen `main`** — validiert die Änderung (Build +
  Szenario-Tests), bevor sie gemergt werden kann
- **Push eines Versions-Tags `v*`** — baut das Release-Artefakt und
  veröffentlicht ein GitHub-Release mit dem gepackten ZIP

Direkte Pushes auf `main` lösen keinen Build mehr aus. Alle
Code-Änderungen müssen über einen Pull Request laufen.

### Merge nach `main`

Der Branch-Schutz verlangt:

1. PR (keine direkten Pushes)
2. Status-Check `build-macos` grün (`make all` erfolgreich)
3. PR-Branch aktuell mit main

## Lizenz

Dieses Projekt steht unter der
[GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.html).
GPLv3 ist erforderlich, weil espeak-ng (GPL-3.0-or-later) statisch in das
gebündelte `libpiper.dylib` gelinkt ist. Kompatibel mit allen anderen
gebündelten Drittanbieter-Bibliotheken; siehe
[`THIRD_PARTY.md`](THIRD_PARTY.md) für die Aufschlüsselung pro
Abhängigkeit.
