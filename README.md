# Welly's VFR ATC — AI Voice ATC for X-Plane 12

![Welly's VFR ATC panel with ATIS broadcast at LSZB Bern-Belp](images/atc-atis-example.jpg)

> **Talk to the tower via push-to-talk — AI-powered VFR radio in German or
> English for X-Plane 12, running locally on your Mac or through the cloud.**

Welly's VFR ATC turns your VFR flights in X-Plane 12 into a real radio
conversation: you press the push-to-talk key, speak your call into the
microphone, and the tower answers you back by voice — in either **German
(NfL/BZF)** or **English (ICAO)** VFR phraseology, with realistic reactions
even to pilot mistakes.

---

## What the plugin covers

- **🇩🇪 🇬🇧 Two language profiles — DE (NfL/BZF) & EN (ICAO)** — switchable
  at runtime in the settings; **German** stays the default. **German**
  models VFR radio phraseology per **NfL Sprechfunk 2024** (DACH region)
  with an optional **BZF strict mode** that checks your readbacks for
  completeness the way an examiner would. **English** follows the
  self-contained **ICAO VFR phraseology** (ICAO Annex 10 Vol II / Doc 4444 /
  EASA SERA) — not a translation of the German profile, but its own standard
  wording and number/callsign pronunciation. The **operating interface**
  (menus, buttons, labels) has its own language switch, independent of the
  spoken phraseology — so you can run the UI in English while training German
  NfL radio, and the switch applies instantly without a restart.
- **🛬 Traffic pattern** — the full pattern flow at controlled and
  uncontrolled airfields: entry, downwind, base, final, landing,
  touch-and-go, go-around — including landing sequencing ("you are number
  two, follow the traffic").
- **🗺️ Cross country** — the complete cross-country flight: departure
  clearance, en-route frequency changes and the approach flow between
  airfields. The approach controller proactively hands you off to the tower
  with the destination frequency.
- **🗼 Airfield types** — the plugin automatically detects the kind of air
  traffic service at each airfield and adapts phraseology and flow
  accordingly:
  - **Uncontrolled** — self-information via UNICOM/CTAF (self-announcements,
    no clearances).
  - **With tower** — controlled field with clearances over the tower
    frequency.
  - **With tower and ground** — additionally a separate ground control for
    taxi clearances.
  - **With AFIS** — airfield with a flight information service (information
    rather than control): traffic and airfield information without binding
    clearances.
- **🎙️ Voice radio** — natural speech via push-to-talk (keyboard or
  joystick). Context-aware **phraseology hints** show you what to say, and
  the tower politely coaches you on poor radio discipline.
- **🤖 AI support** — speech recognition → intent understanding → speech
  synthesis, plus automatic **ATIS broadcasts** from live weather and
  **traffic advisories** about surrounding aircraft.
- **💻 Local AI & ☁️ Cloud AI** — choose your mode at runtime:
  - **Local** (Apple Silicon) — runs **100% offline** after a one-time model
    download. No subscription, no API key, no constant internet.
  - **OpenAI Cloud** (any Mac) — with your own API key.
  - **Mistral Cloud** (any Mac) — with your own API key; speaks German
    without a US accent.
- **🪟 Windows (cloud-only, fully supported)** — a pure cloud build for
  X-Plane 12 on Windows is included (`win_x64/xp_wellys_vfr_atc.xpl`, OpenAI
  **or** Mistral, API key in the Windows Credential Manager). **Verified
  end-to-end on real Windows 11 hardware** (Shadow cloud PC, Windows 11,
  NVIDIA GPU): a complete VFR round trip out of **Friedrichshafen (EDNY)** —
  plugin loading, microphone/PTT, the full STT→ATC→TTS pipeline and the API
  key in the Credential Manager all work flawlessly. Local offline AI is
  **not** available on Windows (no Apple Silicon / no Metal); the cloud
  (OpenAI or Mistral, API key required) is the only backend option.

## What Welly's ATC is for

> **This plugin is first and foremost oriented toward REALITY.** The goal is
> to reproduce VFR radio procedures as authentically as possible — in German
> per NfL/BZF, in English per ICAO — so that you can **train and practice for
> future exams and tests, such as the BZF**. We work continuously toward
> bringing the phraseology and the ATC flows even closer to real practice.

**Disclaimer.** Welly's ATC is a practice and training tool for flight
simulation. It is **not an official certification, not an educational
resource in the sense of accredited training, and not a substitute for real
exam preparation**. We accept **no responsibility and give no guarantee
whatsoever regarding passing any test or exam**. Use is at your own risk; no
warranty is given for the correctness of the phraseology shown. Corrections
from BZF holders are expressly welcome.

## What it can't do (yet)

- **No IFR** — no instrument flight, no IFR clearances, no flight planning,
  no FMS/routing.
- **German & English, no FR/IT** — VFR phraseology comes as a German
  (NfL/BZF, default) and an English (ICAO) profile. Further languages — such
  as French or Italian for western Switzerland or Ticino — are not planned.
- **Local mode on Apple Silicon only** — Intel Macs can use the plugin, but
  only in cloud mode (OpenAI or Mistral, API key required).
- **No local offline AI on Windows** — the Windows build is fully supported
  and verified on real Windows 11 hardware, but runs exclusively in cloud
  mode (OpenAI or Mistral, API key required); local models need Apple
  Silicon / Metal.
- **Not modeled yet** — wake-turbulence separation, freely selectable taxi
  routes (currently always "via Alpha"), large hub airports (LSZH, LSGG …)
  with a delivery workflow, and a virtual co-pilot / checklist reader.

> A detailed breakdown of the limitations including effort estimates is in
> the [technical documentation](docs/README.md#known-limitations).

## Companion plugin: Welly's VFR Trainer

**[Welly's VFR Trainer](https://github.com/rwellinger/xp_wellys_vfr_trainer)** is
an optional gamification layer on top of Welly's ATC: it suggests VFR flights
in the DACH region by airfield difficulty and, after landing, rates your
flight by time-correlating this plugin's ATC radio calls with the flight data
and having the LLM judge them. Welly's ATC works fully on its own — the
trainer is purely optional.

## 📖 Technical documentation

Installation, quick start, backend modes, building from source, local
inference models, configuration, architecture and development workflow:

**→ [docs/README.md](docs/README.md)**

## License

[GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.html)
— details and the breakdown of third-party dependencies are in the
[technical documentation](docs/README.md#license) and in
[`THIRD_PARTY.md`](THIRD_PARTY.md).
