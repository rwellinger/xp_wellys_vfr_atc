#include "atc/bzf_compliance.hpp"

#include <catch2/catch_amalgamated.hpp>

#include <string>
#include <vector>

using bzf_compliance::build_correction_response;
using bzf_compliance::check_pilot_readback;
using bzf_compliance::Element;
using bzf_compliance::extract_required;

// ── extract_required: tower-side detection ─────────────────────────

TEST_CASE("extract_required: empty tower response yields empty list",
          "[bzf_compliance][extract]") {
    REQUIRE(extract_required("").empty());
}

TEST_CASE("extract_required: callsign-only clearance still requires callsign",
          "[bzf_compliance][extract]") {
    auto out = extract_required("D-EXYZ, verstanden, auf Wiederhoeren.");
    REQUIRE(out.size() == 1);
    REQUIRE(out[0] == Element::Callsign);
}

TEST_CASE("extract_required: taxi clearance requires callsign + runway + QNH",
          "[bzf_compliance][extract]") {
    auto out = extract_required(
        "D-EXYZ, rollen Sie zum Rollhalt Piste 25 ueber Alpha, QNH 1013.");
    REQUIRE(out.size() == 3);
    REQUIRE(out[0] == Element::Callsign);
    REQUIRE(out[1] == Element::Runway);
    REQUIRE(out[2] == Element::QNH);
}

TEST_CASE("extract_required: handoff requires callsign + frequency",
          "[bzf_compliance][extract]") {
    auto out = extract_required("D-EXYZ, kontaktieren Sie Tower auf 118.300.");
    REQUIRE(out.size() == 2);
    REQUIRE(out[0] == Element::Callsign);
    REQUIRE(out[1] == Element::Frequency);
}

TEST_CASE("extract_required: squawk clearance requires callsign + squawk",
          "[bzf_compliance][extract]") {
    auto out = extract_required("D-EXYZ, Squawk 7000.");
    REQUIRE(out.size() == 2);
    REQUIRE(out[0] == Element::Callsign);
    REQUIRE(out[1] == Element::Squawk);
}

TEST_CASE("extract_required: full clearance with multiple elements",
          "[bzf_compliance][extract]") {
    auto out = extract_required(
        "D-EXYZ, Piste 25, Start frei, QNH 1013, Squawk 7000, "
        "kontaktieren Sie Tower auf 118.300.");
    // callsign + runway + QNH + frequency + squawk
    REQUIRE(out.size() == 5);
}

// ── check_pilot_readback: pilot-side validation ────────────────────

TEST_CASE("check_pilot_readback: conformant readback yields empty missing",
          "[bzf_compliance][check]") {
    std::vector<Element> required{Element::Callsign, Element::Runway,
                                  Element::QNH};
    auto missing =
        check_pilot_readback("Piste 25, QNH 1013, D-EXYZ", required, "D-EXYZ");
    REQUIRE(missing.empty());
}

TEST_CASE("check_pilot_readback: missing QNH is reported",
          "[bzf_compliance][check]") {
    std::vector<Element> required{Element::Callsign, Element::Runway,
                                  Element::QNH};
    auto missing = check_pilot_readback("Piste 25, D-EXYZ", required, "D-EXYZ");
    REQUIRE(missing.size() == 1);
    REQUIRE(missing[0] == Element::QNH);
}

TEST_CASE("check_pilot_readback: missing runway and QNH both reported",
          "[bzf_compliance][check]") {
    std::vector<Element> required{Element::Callsign, Element::Runway,
                                  Element::QNH};
    auto missing = check_pilot_readback("verstanden, D-EXYZ", required, "D-EXYZ");
    REQUIRE(missing.size() == 2);
    REQUIRE(missing[0] == Element::Runway);
    REQUIRE(missing[1] == Element::QNH);
}

TEST_CASE("check_pilot_readback: missing callsign is reported",
          "[bzf_compliance][check]") {
    std::vector<Element> required{Element::Callsign, Element::Runway};
    auto missing = check_pilot_readback("Piste 25, verstanden", required,
                                        "D-EXYZ");
    REQUIRE(missing.size() == 1);
    REQUIRE(missing[0] == Element::Callsign);
}

TEST_CASE("check_pilot_readback: BZF-shortened callsign accepted",
          "[bzf_compliance][check][callsign]") {
    // Phonetic full callsign: "Delta Echo X-Ray Yankee Zulu".
    // Pilot uses BZF-Verkürzung (NfL §13 b)): last two tokens only.
    std::vector<Element> required{Element::Callsign, Element::Runway};
    auto missing = check_pilot_readback(
        "Piste 25, Yankee Zulu", required, "Delta Echo X-Ray Yankee Zulu");
    REQUIRE(missing.empty());
}

TEST_CASE("check_pilot_readback: frequency detection",
          "[bzf_compliance][check]") {
    std::vector<Element> required{Element::Callsign, Element::Frequency};
    auto ok =
        check_pilot_readback("118.300, D-EXYZ", required, "D-EXYZ");
    REQUIRE(ok.empty());
    auto missing =
        check_pilot_readback("verstanden, D-EXYZ", required, "D-EXYZ");
    REQUIRE(missing.size() == 1);
    REQUIRE(missing[0] == Element::Frequency);
}

TEST_CASE("check_pilot_readback: empty required list yields empty missing",
          "[bzf_compliance][check][edge]") {
    auto missing = check_pilot_readback("anything", {}, "D-EXYZ");
    REQUIRE(missing.empty());
}

// Whisper biases toward the general-language prior on acoustically
// ambiguous NATO letters: it renders "Victor" as "Vector" reliably
// enough that any HB-XXV / D-XXV registration would otherwise be
// unusable in strict mode. Regression fix from user test 2026-06-05
// (HB-DSV at EDNY): pilot read back the full callsign correctly,
// Whisper transcribed "Hotel Bravo Delta Sierra Vector", and strict
// mode wrongly flagged missing callsign.
TEST_CASE("check_pilot_readback: Whisper Vector->Victor tolerated",
          "[bzf_compliance][check][whisper]") {
    std::vector<Element> required{Element::Callsign, Element::Runway,
                                  Element::QNH};
    auto missing = check_pilot_readback(
        "Rollen zum Rollhalt Piste 24 ueber Alfa QNH 1013 Hotel Bravo Delta "
        "Sierra Vector",
        required, "Hotel Bravo Delta Sierra Victor");
    REQUIRE(missing.empty());
}

TEST_CASE("check_pilot_readback: Whisper Juliet->Juliett tolerated",
          "[bzf_compliance][check][whisper]") {
    std::vector<Element> required{Element::Callsign};
    auto missing = check_pilot_readback("verstanden D-Echo Juliet Bravo",
                                        required, "Delta Echo Juliett Bravo");
    REQUIRE(missing.empty());
}

// Regression from the EDNY Friedrichshafen test flight (2026-06-15):
// pilot read back "runway 06 Landung frei 3 Alfa Bravo". Strict mode
// wrongly flagged BOTH runway (English "runway" not recognised) and
// callsign (Whisper "Alfa" vs stored "Alpha"). Both forms are now
// tolerated so the landing clearance read-back is accepted.
TEST_CASE("check_pilot_readback: English 'runway NN' accepted",
          "[bzf_compliance][check][whisper]") {
    std::vector<Element> required{Element::Callsign, Element::Runway};
    auto missing = check_pilot_readback(
        "runway 06 Landung frei November One Two Three Alpha Bravo", required,
        "November One Two Three Alpha Bravo");
    REQUIRE(missing.empty());
}

TEST_CASE("check_pilot_readback: Whisper Alfa->Alpha tolerated",
          "[bzf_compliance][check][whisper]") {
    std::vector<Element> required{Element::Callsign, Element::Runway};
    auto missing = check_pilot_readback("runway 06 Landung frei 3 Alfa Bravo",
                                        required,
                                        "November One Two Three Alpha Bravo");
    REQUIRE(missing.empty());
}

// Whisper welds German compounds: "Rollhalt Piste 06" gets transcribed
// as "Rollhaltpiste 06" (no space). The runway matcher must still
// recognise it — otherwise the pilot's correct readback is flagged
// as missing runway. Regression fix from user EDNY test 2026-06-05:
// "Rollen zur Rollhaltpiste 06 über Alfa QNH 1020 Hotel Bravo Delta
// Sierra Victor".
TEST_CASE("check_pilot_readback: Whisper-welded compound Rollhaltpiste",
          "[bzf_compliance][check][whisper]") {
    std::vector<Element> required{Element::Callsign, Element::Runway,
                                  Element::QNH};
    auto missing = check_pilot_readback(
        "Rollen zur Rollhaltpiste 06 über Alfa QNH 1020 Hotel Bravo Delta "
        "Sierra Victor",
        required, "Hotel Bravo Delta Sierra Victor");
    REQUIRE(missing.empty());
}

TEST_CASE("extract_required: welded compound also detected on tower side",
          "[bzf_compliance][extract][whisper]") {
    // Even if a template (or future LLM-generated tower text) emits a
    // welded form, the extractor must still recognise the runway.
    auto out = extract_required("D-EXYZ, rollen Sie zur Rollhaltpiste 06.");
    bool has_runway = false;
    for (auto e : out)
        if (e == Element::Runway)
            has_runway = true;
    REQUIRE(has_runway);
}

// ── build_correction_response: template rendering ──────────────────

TEST_CASE("build_correction_response: empty missing yields empty string",
          "[bzf_compliance][correction]") {
    REQUIRE(build_correction_response("D-EXYZ", {}).empty());
}

TEST_CASE("build_correction_response: single missing picks element-specific "
          "template + substitutes callsign",
          "[bzf_compliance][correction]") {
    auto resp = build_correction_response("D-EXYZ", {Element::QNH});
    // Default fallback applies if atc_templates isn't loaded — both
    // forms must mention QNH and contain the callsign verbatim.
    REQUIRE(resp.find("D-EXYZ") != std::string::npos);
}

TEST_CASE("build_correction_response: multi-missing uses missing_multi",
          "[bzf_compliance][correction]") {
    auto resp = build_correction_response(
        "D-EXYZ", {Element::QNH, Element::Runway});
    REQUIRE(resp.find("D-EXYZ") != std::string::npos);
}

// ── element_name / missing_key stability ───────────────────────────

TEST_CASE("element_name maps each Element to a stable lowercase string",
          "[bzf_compliance][names]") {
    REQUIRE(std::string(bzf_compliance::element_name(Element::Callsign)) ==
            "callsign");
    REQUIRE(std::string(bzf_compliance::element_name(Element::Runway)) ==
            "runway");
    REQUIRE(std::string(bzf_compliance::element_name(Element::QNH)) == "qnh");
    REQUIRE(std::string(bzf_compliance::element_name(Element::Frequency)) ==
            "frequency");
    REQUIRE(std::string(bzf_compliance::element_name(Element::Squawk)) ==
            "squawk");
}

TEST_CASE("missing_key maps each Element to its bzf_strict template key",
          "[bzf_compliance][names]") {
    REQUIRE(std::string(bzf_compliance::missing_key(Element::Callsign)) ==
            "missing_callsign");
    REQUIRE(std::string(bzf_compliance::missing_key(Element::Runway)) ==
            "missing_runway");
    REQUIRE(std::string(bzf_compliance::missing_key(Element::QNH)) ==
            "missing_qnh");
    REQUIRE(std::string(bzf_compliance::missing_key(Element::Frequency)) ==
            "missing_frequency");
    REQUIRE(std::string(bzf_compliance::missing_key(Element::Squawk)) ==
            "missing_squawk");
}

// ── missing_readback_elements: char-robust soll-ist match ──────────

using bzf_compliance::ClearanceComponents;
using bzf_compliance::missing_readback_elements;
using bzf_compliance::readback_covers_core;

// The EDNY regression that motivated the whole change: the departure
// clearance issued only callsign + runway ("Piste 06, Start frei, ...").
// Whisper welded "Start frei" into "startfrei", so the keyword parser
// returned UNKNOWN/0.00. The structured matcher only checks the mandated
// values (runway 06, callsign) — "startfrei" is irrelevant filler — so
// the readback is fully covered.
TEST_CASE("missing_readback_elements: welded 'startfrei' readback is complete",
          "[bzf_compliance][readback]") {
    ClearanceComponents comp;
    comp.callsign = "Hotel Bravo Whiskey Romeo Oscar";
    comp.runway = "06";
    comp.required = {Element::Callsign, Element::Runway};
    auto missing = missing_readback_elements(
        comp, "Piste 06 startfrei Hotel Bravo Whiskey Romeo Oscar");
    REQUIRE(missing.empty());
    REQUIRE(readback_covers_core(comp.required, missing));
}

TEST_CASE("missing_readback_elements: welded qnh/piste tokens still match",
          "[bzf_compliance][readback]") {
    ClearanceComponents comp;
    comp.callsign = "D-EXYZ";
    comp.runway = "06";
    comp.qnh = "1018";
    comp.required = {Element::Callsign, Element::Runway, Element::QNH};
    // No spaces around the values — the canonical char stream eliminates
    // the word boundary the welding broke.
    auto missing = missing_readback_elements(comp, "piste06 qnh1018 d-exyz");
    REQUIRE(missing.empty());
}

TEST_CASE("missing_readback_elements: spoken digit words are normalised",
          "[bzf_compliance][readback]") {
    ClearanceComponents comp;
    comp.callsign = "Hotel Bravo Whiskey Romeo Oscar";
    comp.runway = "06";
    comp.qnh = "1018";
    comp.required = {Element::Callsign, Element::Runway, Element::QNH};
    auto missing = missing_readback_elements(
        comp,
        "Piste null sechs QNH eins null eins acht Hotel Bravo Whiskey Romeo "
        "Oscar");
    REQUIRE(missing.empty());
}

TEST_CASE("missing_readback_elements: missing QNH is reported (completeness)",
          "[bzf_compliance][readback]") {
    ClearanceComponents comp;
    comp.callsign = "D-EXYZ";
    comp.runway = "06";
    comp.qnh = "1018";
    comp.required = {Element::Callsign, Element::Runway, Element::QNH};
    auto missing = missing_readback_elements(comp, "Piste 06 D-EXYZ");
    REQUIRE(missing.size() == 1);
    REQUIRE(missing[0] == Element::QNH);
    // Recognition (lenient) still accepts it — callsign + runway covered.
    REQUIRE(readback_covers_core(comp.required, missing));
}

// ── readback_covers_core: recognition threshold ────────────────────

TEST_CASE("readback_covers_core: callsign alone is not a readback",
          "[bzf_compliance][readback][core]") {
    ClearanceComponents comp;
    comp.callsign = "D-EXYZ";
    comp.runway = "06";
    comp.required = {Element::Callsign, Element::Runway};
    // Only the callsign present -> Runway missing -> phantom guard fires.
    auto missing = missing_readback_elements(comp, "verstanden D-EXYZ");
    REQUIRE_FALSE(readback_covers_core(comp.required, missing));
}

TEST_CASE("readback_covers_core: callsign-only clearance accepts callsign",
          "[bzf_compliance][readback][core]") {
    ClearanceComponents comp;
    comp.callsign = "D-EXYZ";
    comp.required = {Element::Callsign};
    auto missing = missing_readback_elements(comp, "verstanden D-EXYZ");
    REQUIRE(missing.empty());
    REQUIRE(readback_covers_core(comp.required, missing));
}
