/*
 * xp_wellys_devfr_atc - AI-powered ATC voice communication for X-Plane 12
 * Copyright (C) 2026 thWelly & Claude (Anthropic)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef CORE_CROSS_COUNTRY_LOG_HPP
#define CORE_CROSS_COUNTRY_LOG_HPP

#include <string>
#include <vector>

// Per-transmission failure logging for the cross-country measurement
// session. PURELY OBSERVATIONAL — this module never feeds matching,
// routing or classification. It records one JSON-Lines record per
// processed pilot transmission so the raw material to decide whether a
// phraseology fuzzy-layer or a location-aware proper-name recogniser is
// needed can be evaluated offline.
//
// SDK-free. Lives in the xp_atc_engine OBJECT lib (file I/O only, no
// XPLM headers) so atc_repl and the scenario tests can exercise it too.
namespace cross_country_log {

struct Entry {
  // ── Raw observation ────────────────────────────────────────────────
  std::string transcript; // raw Whisper output, unmodified (engine in.transcript)
  float quality = 0.0f;   // Whisper quality value

  // ── Classification result ──────────────────────────────────────────
  std::string intent;     // classified / rule-hinted intent name
  float confidence = 0.0f;

  // Which path carried this transmission:
  //   "rule_skip_lm"    — high-conf rule parse (or LM-not-ready), no LM call
  //   "lm_fallback"     — low-conf -> LM classification round-trip
  //   "clearance_match" — deterministic readback recognition vs clearance
  std::string path;

  bool lm_used = false; // true iff the LM backend was invoked
  bool lm_ready = false; // meaningful only when lm_used

  // "classified" | "unknown" | "tower_reported_garbled"
  std::string outcome;

  // ── Context at the moment the pilot spoke ──────────────────────────
  std::string state;        // ATC state name
  std::string flight_phase; // flight phase name

  // ── Raw material for offline failure attribution ───────────────────
  // The set of intents plausibly expected in this state (valid_intents
  // CSV). Combined with transcript + vrp_name this lets the evaluator
  // decide by hand whether a miss was a phraseology token or a proper
  // name — no behaviour-changing heuristic is involved.
  std::string expected_intent;
  bool vrp_name_set = false;
  std::string vrp_name;

  // READBACK only. When set, readback_missing_elements is serialised as
  // a JSON array; otherwise the field is written as null.
  bool is_readback = false;
  std::vector<std::string> readback_missing_elements;

  // Non-binding suggestion. Emitted (as a string) ONLY when the caller
  // flags a failure (outcome != "classified") or the LM fallback fired;
  // otherwise the field is written as null. The raw fields above remain
  // authoritative and let this suggestion be overruled by hand.
  // "phraseology" | "proper_name" | "mixed" | "unclear"
  bool emit_failure_locus = false;
  std::string failure_locus;
};

// Backend label ("openai" | "mistral" | "local") recorded by the loader
// at backend bring-up. Engine code never inspects settings::backend_mode
// (Backend Adapter Rule) — it only reads the label the backend layer
// reported here, which this module folds into each record.
void set_lm_backend(const std::string &backend);

// Override the output file path. Default: "cross_country_session.log"
// (relative -> lands in the X-Plane working directory, next to Log.txt).
void set_path(const std::string &path);

// Append one JSON-Lines record (one object per line). Thread-safe.
void write(const Entry &e);

} // namespace cross_country_log

#endif // CORE_CROSS_COUNTRY_LOG_HPP
