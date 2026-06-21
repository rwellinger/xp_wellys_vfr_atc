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
    // Deterministic VFR intention defaults (global settings leak across the
    // suite's known order dependence). Pattern => hint reads "VFR Platzrunde".
    settings::set_vfr_flight_type("pattern");
    settings::set_vfr_destination("");
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

// ── VFR intention: Platzrunde vs. Ueberlandflug (NfL 1.4.7 a/b) ───────
//
// The departure hints carry the pilot's intention. The sim has no flight
// plan, so it comes from settings: pattern => "VFR Platzrunde";
// cross_country => "VFR nach <dest>" (or "VFR Ueberlandflug" when no dest).
// The {destination} var feeds "Kurs nach <dest>" with a "Plan" fallback.

TEST_CASE("vfr intention: pattern renders 'VFR Platzrunde', H2 holds",
          "[conformance][intention][h2]") {
    load_de_profile();
    atis_generator::init();
    settings::set_vfr_flight_type("pattern");
    auto ctx = ground_ctx();

    auto vars = ground_ops::build_vars(first_call("", false), ctx);
    REQUIRE(vars["intention"] == "VFR Platzrunde");

    // H2: the rendered hint, read back through the parser, leaves the
    // recommended set complete (the "platzrunde" intention keyword).
    std::string rendered = atc_templates::fill(
        flight_phase::get_pilot_phraseology("INITIAL_CALL_GROUND"), vars);
    REQUIRE(rendered.find("VFR Platzrunde") != std::string::npos);
    auto parsed = intent_parser::parse(rendered, ctx);
    auto r =
        initial_call_conformance::evaluate("INITIAL_CALL_GROUND", parsed, ctx);
    REQUIRE(r.missing_recommended.empty());
    REQUIRE(r.missing_required.empty());
}

TEST_CASE("vfr intention: cross-country with destination renders 'VFR nach X'",
          "[conformance][intention]") {
    load_de_profile();
    settings::set_vfr_flight_type("cross_country");
    settings::set_vfr_destination("EDDS");
    auto ctx = ground_ctx();

    auto vars = ground_ops::build_vars(first_call("", false), ctx);
    REQUIRE(vars["intention"] == "VFR nach EDDS");
    REQUIRE(vars["destination"] == "EDDS");

    // REQUEST_TAXI carries the intention (NfL 1.4.7 b home of the
    // destination); READY_FOR_DEPARTURE_VFR carries the course token.
    std::string taxi = atc_templates::fill(
        flight_phase::get_pilot_phraseology("REQUEST_TAXI"), vars);
    REQUIRE(taxi.find("VFR nach EDDS") != std::string::npos);
    std::string dep = atc_templates::fill(
        flight_phase::get_pilot_phraseology("READY_FOR_DEPARTURE_VFR"), vars);
    REQUIRE(dep.find("Kurs nach EDDS") != std::string::npos);

    // H2 still holds — "nach" is an intention keyword.
    std::string rendered = atc_templates::fill(
        flight_phase::get_pilot_phraseology("INITIAL_CALL_GROUND"), vars);
    auto parsed = intent_parser::parse(rendered, ctx);
    auto rr =
        initial_call_conformance::evaluate("INITIAL_CALL_GROUND", parsed, ctx);
    REQUIRE(rr.missing_recommended.empty());
}

TEST_CASE("vfr intention: cross-country without destination falls back",
          "[conformance][intention]") {
    load_de_profile();
    settings::set_vfr_flight_type("cross_country");
    settings::set_vfr_destination("");
    auto ctx = ground_ctx();

    auto vars = ground_ops::build_vars(first_call("", false), ctx);
    REQUIRE(vars["intention"] == "VFR Ueberlandflug");
    REQUIRE(vars["destination"] == "Plan");

    std::string dep = atc_templates::fill(
        flight_phase::get_pilot_phraseology("READY_FOR_DEPARTURE_VFR"), vars);
    REQUIRE(dep.find("Kurs nach Plan") != std::string::npos);
}

// ── Tower-only first contact: INITIAL_CALL_TOWER -> INITIAL_CALL_GROUND ──
//
// At a tower-only field the corrected GROUND hint renders {taxi_controller}
// = "Turm", so a student reading it verbatim is classified INITIAL_CALL_
// TOWER by the text-based parser. The state machine collapses that apron
// first contact (IDLE / on_ground / tower_only) to INITIAL_CALL_GROUND so
// the SAME conformance check + hint apply as at a field with Ground. The
// holding-point "abflugbereit" Tower call (state past IDLE) and a normal
// towered field (tower_only=false) are left untouched.

namespace {

// IDLE / tower-only field / on the TOWER frequency — the apron first-contact
// state where the collapse must fire.
xplane_context::XPlaneContext tower_only_ctx() {
    auto ctx = ground_ctx();
    ctx.tower_only = true;
    ctx.frequency_type = xplane_context::FrequencyType::TOWER;
    return ctx;
}

// A Tower-addressed apron first call (what the parser yields from the
// rendered tower-only hint), carrying all four recommended elements.
PilotMessage tower_complete_call() {
    PilotMessage m = first_call(
        "Friedrichshafen Turm, Diamond Hotel Bravo Whiskey Romeo Oscar, "
        "am Vorfeld, VFR nach Norden, Information Alpha",
        true);
    m.intent = PilotIntent::INITIAL_CALL_TOWER;
    return m;
}

// Tower-addressed, missing aircraft_type + intention.
PilotMessage tower_partial_call() {
    PilotMessage m = first_call(
        "Friedrichshafen Turm, Hotel Bravo Whiskey Romeo Oscar, am Vorfeld, "
        "Information Alpha",
        true);
    m.intent = PilotIntent::INITIAL_CALL_TOWER;
    return m;
}

} // namespace

TEST_CASE("tower-only: rendered hint collapses to GROUND and clears (strict)",
          "[conformance][tower_only][h2]") {
    load_de_profile();
    atis_generator::init();
    settings::set_bzf_strict_mode(true);
    auto ctx = tower_only_ctx();

    // Render the GROUND first-contact hint exactly as the UI does, on a
    // tower-only field: {taxi_controller} resolves to "Turm".
    PilotMessage seed = first_call("", false);
    auto vars = ground_ops::build_vars(seed, ctx);
    std::string tmpl =
        flight_phase::get_pilot_phraseology("INITIAL_CALL_GROUND");
    std::string rendered = atc_templates::fill(tmpl, vars);
    INFO("rendered tower-only hint: " << rendered);
    REQUIRE(rendered.find("Turm") != std::string::npos);

    // The premise: the text-based parser classifies this as a TOWER call.
    auto parsed = intent_parser::parse(rendered, ctx);
    REQUIRE(parsed.intent == PilotIntent::INITIAL_CALL_TOWER);

    // Through the state machine the collapse rewrites it to GROUND: the
    // self-dictated hint clears the conformance check (no re-request) and
    // the contact advances.
    atc_state_machine::set_state(atc_state_machine::ATCState::IDLE);
    auto resp = atc_state_machine::process(parsed, ctx, 100.0);
    REQUIRE_FALSE(resp.text.empty());
    REQUIRE(resp.text.find("Luftfahrzeugmuster") == std::string::npos);
    REQUIRE(resp.text.find("sagen Sie") == std::string::npos);
    REQUIRE(atc_state_machine::get_state() ==
            atc_state_machine::state_from_name("GROUND_CONTACT"));
}

TEST_CASE("tower-only: collapse wires the conformance guard (deficient call)",
          "[conformance][tower_only][bzf_strict]") {
    load_de_profile();
    settings::set_bzf_strict_mode(true);
    auto ctx = tower_only_ctx();

    atc_state_machine::set_state(atc_state_machine::ATCState::IDLE);
    auto corr = atc_state_machine::process(tower_partial_call(), ctx, 100.0);

    // Collapsed to GROUND -> the GROUND guard re-requests the missing
    // elements and holds IDLE. Proves the guard is actually reached.
    REQUIRE_FALSE(corr.text.empty());
    REQUIRE(corr.text.find("Luftfahrzeugmuster") != std::string::npos);
    REQUIRE(corr.text.find("Vorhaben") != std::string::npos);
    REQUIRE(atc_state_machine::get_state() == atc_state_machine::ATCState::IDLE);

    auto ok = atc_state_machine::process(tower_complete_call(), ctx, 101.0);
    REQUIRE_FALSE(ok.text.empty());
    REQUIRE(ok.text.find("Luftfahrzeugmuster") == std::string::npos);
    REQUIRE(atc_state_machine::get_state() ==
            atc_state_machine::state_from_name("GROUND_CONTACT"));
}

TEST_CASE("tower-only: holding-point Tower call is NOT collapsed (separation)",
          "[conformance][tower_only][bzf_strict]") {
    load_de_profile();
    settings::set_bzf_strict_mode(true);
    auto ctx = tower_only_ctx();

    // State has advanced past IDLE (taxi already cleared): the "abflugbereit"
    // Tower call at the holding point must stay INITIAL_CALL_TOWER, so the
    // GROUND conformance re-request must NOT fire even though the call lacks
    // the recommended apron elements.
    atc_state_machine::set_state(
        atc_state_machine::state_from_name("TAXI_CLEARED"));
    PilotMessage m = first_call(
        "Friedrichshafen Turm, Hotel Bravo Whiskey Romeo Oscar, "
        "Rollhalt Piste 24, abflugbereit",
        false);
    m.intent = PilotIntent::INITIAL_CALL_TOWER;

    auto resp = atc_state_machine::process(m, ctx, 100.0);
    REQUIRE(resp.text.find("Luftfahrzeugmuster") == std::string::npos);
    REQUIRE(resp.text.find("sagen Sie") == std::string::npos);
}

TEST_CASE("field with Ground: Tower first call stays TOWER, no check "
          "(regression)",
          "[conformance][bzf_strict]") {
    load_de_profile();
    settings::set_bzf_strict_mode(true);
    auto ctx = ground_ctx(); // tower_only == false
    ctx.frequency_type = xplane_context::FrequencyType::TOWER;

    // tower_only=false -> no collapse: a Tower initial call stays TOWER and
    // is never subjected to the GROUND conformance check (Situation 2 stays
    // untouched).
    atc_state_machine::set_state(atc_state_machine::ATCState::IDLE);
    PilotMessage m = first_call(
        "Friedrichshafen Turm, Hotel Bravo Whiskey Romeo Oscar, am Vorfeld",
        true);
    m.intent = PilotIntent::INITIAL_CALL_TOWER;

    auto resp = atc_state_machine::process(m, ctx, 100.0);
    REQUIRE(resp.text.find("Luftfahrzeugmuster") == std::string::npos);
    REQUIRE(resp.text.find("sagen Sie") == std::string::npos);
}
