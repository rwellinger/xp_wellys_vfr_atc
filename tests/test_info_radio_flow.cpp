/*
 * xp_wellys_devfr_atc - AI-powered ATC voice communication for X-Plane 12
 * Copyright (C) 2026 thWelly & Claude (Anthropic)
 *
 * Licensed under GPL-3.0-or-later. See LICENSE.
 */

// AFIS/Info and Radio facilities at uncontrolled fields. apt.dat encodes both
// under the Tower row code (1054); the parser reclassifies the name suffix to
// FrequencyType::INFO / RADIO (see test_frequency_classification.cpp). INFO is
// served by the advisory handle_info_flow() (no clearance, no readback, call
// "<Platz> Information"); RADIO routes through handle_unicom_flow() with the
// "<Platz> Radio" call. NfL 2024 §34 b) / §35.

#include "atc/atc_state_machine.hpp"
#include "atc/atc_templates.hpp"
#include "atc/flows/ground_operations.hpp"
#include "atc/intent_parser.hpp"
#include "core/xplane_context.hpp"
#include "persistence/settings.hpp"

#include <catch2/catch_amalgamated.hpp>

#include <string>

using atc_state_machine::ATCResponse;
using atc_state_machine::ATCState;
using intent_parser::PilotIntent;
using intent_parser::PilotMessage;
using FT = xplane_context::FrequencyType;

namespace {

struct DeProfileGuard {
    std::string saved_region;
    DeProfileGuard() : saved_region(settings::atc_profile()) {
        settings::set_atc_profile("DE");
    }
    ~DeProfileGuard() { settings::set_atc_profile(saved_region); }
};

// Airborne pilot inbound to an uncontrolled field, tuned to the named facility.
xplane_context::XPlaneContext info_ctx(FT freq) {
    xplane_context::XPlaneContext ctx;
    ctx.on_ground = false;
    ctx.is_towered_airport = false; // no real Tower (1054 was named Info/Radio)
    ctx.frequency_type = freq;
    ctx.nearest_airport_id = "EDTY";
    ctx.nearest_airport_name = "Schwaebisch Hall";
    ctx.active_runway = "10";
    return ctx;
}

bool has(const std::string &haystack, const std::string &needle) {
    return haystack.find(needle) != std::string::npos;
}

PilotMessage inbound_msg() {
    PilotMessage msg;
    msg.intent = PilotIntent::INITIAL_CALL_INBOUND;
    return msg;
}

} // namespace

TEST_CASE("info-flow: INFO frequency routes to handle_info_flow, advisory call",
          "[info][de][flow]") {
    DeProfileGuard g;
    atc_state_machine::init();
    atc_templates::reload();

    auto ctx = info_ctx(FT::INFO);
    ATCResponse resp;
    REQUIRE(ground_ops::handle_info_flow(inbound_msg(), ctx, resp));

    // Advisory: addresses "<Platz> Information", never a clearance/readback.
    REQUIRE(has(resp.text, "Schwaebisch Hall Information"));
    REQUIRE(resp.next_state == ATCState::IDLE);
    REQUIRE_FALSE(resp.requires_readback);
}

TEST_CASE("info-flow: handle_info_flow ignores non-INFO frequencies",
          "[info][de][flow]") {
    DeProfileGuard g;
    atc_state_machine::init();
    atc_templates::reload();

    ATCResponse resp;
    REQUIRE_FALSE(
        ground_ops::handle_info_flow(inbound_msg(), info_ctx(FT::RADIO), resp));
    REQUIRE_FALSE(
        ground_ops::handle_info_flow(inbound_msg(), info_ctx(FT::UNICOM), resp));
}

TEST_CASE("info-flow: RADIO frequency routes through unicom flow with Radio call",
          "[info][de][flow]") {
    DeProfileGuard g;
    atc_state_machine::init();
    atc_templates::reload();

    auto ctx = info_ctx(FT::RADIO);
    ATCResponse resp;
    REQUIRE(ground_ops::handle_unicom_flow(inbound_msg(), ctx, resp));

    REQUIRE(has(resp.text, "Schwaebisch Hall Radio"));
    REQUIRE(resp.next_state == ATCState::IDLE);
    REQUIRE_FALSE(resp.requires_readback);
}

TEST_CASE("info-flow: UNICOM call is unchanged (still 'Verkehr <Platz>')",
          "[info][de][flow]") {
    DeProfileGuard g;
    atc_state_machine::init();
    atc_templates::reload();

    auto ctx = info_ctx(FT::UNICOM);
    PilotMessage msg;
    msg.intent = PilotIntent::SELF_ANNOUNCE;
    ATCResponse resp;
    REQUIRE(ground_ops::handle_unicom_flow(msg, ctx, resp));

    // Generic uncontrolled/IDLE block: byte-identical "Verkehr <Platz>" call.
    REQUIRE(has(resp.text, "Verkehr Schwaebisch Hall"));
    REQUIRE_FALSE(has(resp.text, "Radio"));
    REQUIRE_FALSE(has(resp.text, "Information"));
}
