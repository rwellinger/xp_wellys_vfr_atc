#include "persistence/model_manifest.hpp"

#include <catch2/catch_amalgamated.hpp>

using model_manifest::Kind;
using model_manifest::VoiceRole;

// ── Language-aware Whisper selection (M6) ────────────────────────────
// German-VFR-only build: the catalog ships a single (multilingual)
// Whisper model tagged "de" and a single German Piper voice.

TEST_CASE("get_for_language picks the multilingual Whisper for 'de'",
          "[model_manifest][m6]") {
    const auto &e = model_manifest::get_for_language(Kind::WhisperModel, "de");
    REQUIRE(e.kind == Kind::WhisperModel);
    REQUIRE(e.language == "de");
    REQUIRE(e.filename == "ggml-large-v3-turbo-q5_0.bin");
}

TEST_CASE("get_for_language falls back to language-agnostic Llama",
          "[model_manifest][m6]") {
    // Llama is multilingual — no language tag — so any language should
    // route to the same single entry via the empty-language fallback.
    const auto &en = model_manifest::get_for_language(Kind::LlamaModel, "en");
    const auto &de = model_manifest::get_for_language(Kind::LlamaModel, "de");
    REQUIRE(en.kind == Kind::LlamaModel);
    REQUIRE(de.kind == Kind::LlamaModel);
    REQUIRE(en.filename == de.filename);
    REQUIRE(en.language.empty());
}

// ── Voice catalog language tagging (M6) ──────────────────────────────

TEST_CASE("Thorsten voice is tagged 'de'", "[model_manifest][m6]") {
    REQUIRE(model_manifest::voice_language("de_DE-thorsten-medium") == "de");
}

TEST_CASE("voice_language returns empty for unknown ids",
          "[model_manifest][m6]") {
    REQUIRE(model_manifest::voice_language("does-not-exist").empty());
}

// ── default_voice_for(role, language) (M6) ───────────────────────────

TEST_CASE("German default voice covers every role",
          "[model_manifest][m6]") {
    // We ship a single German voice — Thorsten — and assign it to
    // every role at region-switch time.
    for (auto role : model_manifest::all_roles()) {
        REQUIRE(model_manifest::default_voice_for(role, "de") ==
                "de_DE-thorsten-medium");
    }
}

