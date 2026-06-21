#include "atc/atc_state_machine.hpp"
#include "atc/atc_templates.hpp"
#include "atc/atis_generator.hpp"
#include "atc/flight_phase.hpp"
#include "atc/flows/ground_operations.hpp"
#include "atc/initial_call_conformance.hpp"
#include "atc/intent_parser.hpp"
#include "core/xplane_context.hpp"
#include "persistence/settings.hpp"

#include <catch2/catch_amalgamated.hpp>

#include <algorithm>
#include <string>

// PR 1 — BZF first-call conformance mechanism (engine + data, no hint).
//
// Two layers:
//   * Pure detection: initial_call_conformance::evaluate() against a
//     hand-built PilotMessage / XPlaneContext. The fragile part — one
//     positive + one negative per recommended detector.
//   * Behaviour: drive atc_state_machine::process() at IDLE / towered /
//     GROUND with INITIAL_CALL_GROUND, toggling settings::bzf_strict_mode.
//     required is empty for this intent, so strict=false NEVER re-requests
//     (no regression); strict=true enforces all four recommended elements.

namespace {

using intent_parser::PilotIntent;
using intent_parser::PilotMessage;

void load_de_profile() {
    settings::set_atc_profile("DE");
    settings::set_pilot_callsign_raw("Hotel Bravo Whiskey Romeo Oscar");
    atc_templates::init();
    initial_call_conformance::init();
    flight_phase::reload();
    atc_state_machine::init();
}

// IDLE / towered / on GROUND frequency — the state where an INITIAL_CALL_
// GROUND clears the phase + frequency guards and reaches the conformance
// check.
xplane_context::XPlaneContext ground_ctx() {
    xplane_context::XPlaneContext ctx{};
    ctx.is_towered_airport = true;
    ctx.on_ground = true;
    ctx.frequency_type = xplane_context::FrequencyType::GROUND;
    ctx.nearest_airport_id = "EDNY";
    ctx.aircraft_icao = "DV20";
    return ctx;
}

PilotMessage first_call(const std::string &transcript, bool has_position) {
    PilotMessage m;
    m.intent = PilotIntent::INITIAL_CALL_GROUND;
    m.confidence = 0.85f;
    m.callsign = "Hotel Bravo Whiskey Romeo Oscar";
    m.raw_transcript = transcript;
    m.has_position = has_position;
    return m;
}

bool has(const std::vector<std::string> &v, const std::string &e) {
    return std::find(v.begin(), v.end(), e) != v.end();
}

} // namespace

// ── Pure detection ────────────────────────────────────────────────────

TEST_CASE("conformance: required is empty for INITIAL_CALL_GROUND",
          "[conformance]") {
    load_de_profile();
    auto ctx = ground_ctx();

    // Empty utterance — every recommended element is missing, but required
    // stays empty (only the request would be NfL-mandatory, and request is
    // not an element of the first contact).
    auto r = initial_call_conformance::evaluate(
        "INITIAL_CALL_GROUND", first_call("Boden", false), ctx);
    REQUIRE(r.missing_required.empty());
    REQUIRE(r.missing_recommended.size() == 4);
}

TEST_CASE("conformance: position via has_position and via vrp_name",
          "[conformance]") {
    load_de_profile();
    auto ctx = ground_ctx();

    auto with_flag = first_call("Boden", true);
    REQUIRE_FALSE(has(initial_call_conformance::evaluate("INITIAL_CALL_GROUND",
                                                         with_flag, ctx)
                          .missing_recommended,
                      "position"));

    auto with_vrp = first_call("Boden ueber Whiskey", false);
    with_vrp.vrp_name = "Whiskey";
    REQUIRE_FALSE(has(initial_call_conformance::evaluate("INITIAL_CALL_GROUND",
                                                         with_vrp, ctx)
                          .missing_recommended,
                      "position"));

    auto without = first_call("Boden", false);
    REQUIRE(has(initial_call_conformance::evaluate("INITIAL_CALL_GROUND",
                                                   without, ctx)
                    .missing_recommended,
                "position"));
}

TEST_CASE("conformance: intention keyword detection", "[conformance]") {
    load_de_profile();
    auto ctx = ground_ctx();

    auto present = first_call("Boden VFR nach Norden", false);
    REQUIRE_FALSE(has(initial_call_conformance::evaluate("INITIAL_CALL_GROUND",
                                                         present, ctx)
                          .missing_recommended,
                      "intention"));

    auto absent = first_call("Boden guten Tag", false);
    REQUIRE(has(initial_call_conformance::evaluate("INITIAL_CALL_GROUND", absent,
                                                   ctx)
                    .missing_recommended,
                "intention"));
}

TEST_CASE("conformance: atis_letter via 'Information' + NATO letter",
          "[conformance]") {
    load_de_profile();
    auto ctx = ground_ctx();

    auto present = first_call("Boden Information Bravo", false);
    REQUIRE_FALSE(has(initial_call_conformance::evaluate("INITIAL_CALL_GROUND",
                                                         present, ctx)
                          .missing_recommended,
                      "atis_letter"));

    // "Information" without a following letter does not count.
    auto absent = first_call("Boden erbitte Information", false);
    REQUIRE(has(initial_call_conformance::evaluate("INITIAL_CALL_GROUND", absent,
                                                   ctx)
                    .missing_recommended,
                "atis_letter"));
}

TEST_CASE("conformance: aircraft_type via lexicon and via live ICAO",
          "[conformance]") {
    load_de_profile();
    auto ctx = ground_ctx(); // aircraft_icao == "DV20"

    auto by_word = first_call("Boden Diamond am Vorfeld", false);
    REQUIRE_FALSE(has(initial_call_conformance::evaluate("INITIAL_CALL_GROUND",
                                                         by_word, ctx)
                          .missing_recommended,
                      "aircraft_type"));

    auto by_icao = first_call("Boden DV20 am Vorfeld", false);
    REQUIRE_FALSE(has(initial_call_conformance::evaluate("INITIAL_CALL_GROUND",
                                                         by_icao, ctx)
                          .missing_recommended,
                      "aircraft_type"));

    auto absent = first_call("Boden am Vorfeld", false);
    REQUIRE(has(initial_call_conformance::evaluate("INITIAL_CALL_GROUND", absent,
                                                   ctx)
                    .missing_recommended,
                "aircraft_type"));
}

// ── Behaviour through the state machine ───────────────────────────────

namespace {

// A first call carrying all four recommended elements.
PilotMessage complete_call() {
    auto m = first_call(
        "Friedrichshafen Boden, Diamond Hotel Bravo Whiskey Romeo Oscar, "
        "am Vorfeld, VFR nach Norden, Information Alpha",
        true);
    return m;
}

// Missing aircraft_type + intention; position + atis present.
PilotMessage partial_call() {
    auto m = first_call(
        "Friedrichshafen Boden, Hotel Bravo Whiskey Romeo Oscar, am Vorfeld, "
        "Information Alpha",
        true);
    return m;
}

} // namespace

TEST_CASE("conformance behaviour: strict=false never re-requests (no regression)",
          "[conformance][bzf_strict]") {
    load_de_profile();
    settings::set_bzf_strict_mode(false);
    auto ctx = ground_ctx();

    atc_state_machine::set_state(atc_state_machine::ATCState::IDLE);
    auto resp = atc_state_machine::process(partial_call(), ctx, 100.0);

    // Normal initial-contact reply, state advances — exactly as today.
    REQUIRE_FALSE(resp.text.empty());
    REQUIRE(atc_state_machine::get_state() ==
            atc_state_machine::state_from_name("GROUND_CONTACT"));
}

TEST_CASE("conformance behaviour: strict=true re-requests an incomplete call",
          "[conformance][bzf_strict]") {
    load_de_profile();
    settings::set_bzf_strict_mode(true);
    auto ctx = ground_ctx();

    atc_state_machine::set_state(atc_state_machine::ATCState::IDLE);
    auto corr = atc_state_machine::process(partial_call(), ctx, 100.0);

    // A targeted re-request was issued and the state did NOT advance.
    REQUIRE_FALSE(corr.text.empty());
    REQUIRE(corr.text.find("Luftfahrzeugmuster") != std::string::npos);
    REQUIRE(corr.text.find("Vorhaben") != std::string::npos);
    REQUIRE(atc_state_machine::get_state() == atc_state_machine::ATCState::IDLE);

    // The corrected follow-up call completes the contact -> normal reply.
    auto ok = atc_state_machine::process(complete_call(), ctx, 101.0);
    REQUIRE_FALSE(ok.text.empty());
    REQUIRE(ok.text.find("Luftfahrzeugmuster") == std::string::npos);
    REQUIRE(atc_state_machine::get_state() ==
            atc_state_machine::state_from_name("GROUND_CONTACT"));
}

TEST_CASE("conformance behaviour: complete call clears in both modes",
          "[conformance][bzf_strict]") {
    auto ctx = ground_ctx();

    for (bool strict : {false, true}) {
        load_de_profile();
        settings::set_bzf_strict_mode(strict);
        atc_state_machine::set_state(atc_state_machine::ATCState::IDLE);

        auto resp = atc_state_machine::process(complete_call(), ctx, 100.0);
        REQUIRE_FALSE(resp.text.empty());
        REQUIRE(resp.text.find("sagen Sie") == std::string::npos);
        REQUIRE(atc_state_machine::get_state() ==
                atc_state_machine::state_from_name("GROUND_CONTACT"));
    }
}

// ── H2 (PR 2): hint ⇔ conformance consistency ─────────────────────────
//
// The INITIAL_CALL_GROUND phraseology hint (flight_rules.json ::
// pilot_phraseology) is teaching authority: a student who reads it back
// verbatim must NEVER be re-requested by the strict-mode conformance
// check. This asserts set-equality between the hint's rendered elements
// and the recommended set: render the hint exactly as the UI does
// (atc_templates::fill over ground_ops::build_vars), run it through the
// real parser, and require evaluate() to find nothing missing. It breaks
// if the hint wording and the detectors drift apart — e.g. if the hint
// loses its position / intention token, or "erbitte Rollen" creeps back
// in place of an explicit Vorhaben.
TEST_CASE("H2: INITIAL_CALL_GROUND hint satisfies every recommended element",
          "[conformance][hint][h2]") {
    load_de_profile();
    atis_generator::init(); // deterministic letter 'A' -> "Alpha"
    auto ctx = ground_ctx(); // aircraft_icao == "DV20"

    PilotMessage seed = first_call("", false);
    auto vars = ground_ops::build_vars(seed, ctx);
    std::string tmpl =
        flight_phase::get_pilot_phraseology("INITIAL_CALL_GROUND");
    REQUIRE_FALSE(tmpl.empty());
    std::string rendered = atc_templates::fill(tmpl, vars);

    // The corrected hint must NOT teach the taxi request — that belongs to
    // REQUEST_TAXI, not the first contact.
    REQUIRE(rendered.find("erbitte Rollen") == std::string::npos);

    // Read the rendered hint back through the real parser, then evaluate.
    auto parsed = intent_parser::parse(rendered, ctx);
    auto r =
        initial_call_conformance::evaluate("INITIAL_CALL_GROUND", parsed, ctx);
    INFO("rendered hint: " << rendered);
    REQUIRE(r.missing_recommended.empty());
    REQUIRE(r.missing_required.empty());
}
