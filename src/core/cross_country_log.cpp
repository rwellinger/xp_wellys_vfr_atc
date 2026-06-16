/*
 * xp_wellys_devfr_atc - AI-powered ATC voice communication for X-Plane 12
 * Copyright (C) 2026 thWelly & Claude (Anthropic)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "core/cross_country_log.hpp"

#include <json.hpp>

#include <fstream>
#include <mutex>

namespace cross_country_log {

namespace {

std::mutex g_mtx;
std::string g_path = "cross_country_session.log"; // protected by g_mtx
std::string g_lm_backend;                         // protected by g_mtx
} // namespace

void set_lm_backend(const std::string &backend) {
  std::lock_guard<std::mutex> lk(g_mtx);
  g_lm_backend = backend;
}

void set_path(const std::string &path) {
  std::lock_guard<std::mutex> lk(g_mtx);
  g_path = path;
}

void write(const Entry &e) {
  using nlohmann::json;
  json j;
  j["transcript"] = e.transcript;
  j["quality"] = e.quality;
  j["intent"] = e.intent;
  j["confidence"] = e.confidence;
  j["path"] = e.path;

  j["lm_used"] = e.lm_used;
  {
    std::lock_guard<std::mutex> lk(g_mtx);
    if (e.lm_used) {
      j["lm_backend"] = g_lm_backend.empty() ? json(nullptr) : json(g_lm_backend);
      j["lm_ready"] = e.lm_ready;
    } else {
      j["lm_backend"] = nullptr;
      j["lm_ready"] = nullptr;
    }
  }

  j["outcome"] = e.outcome;
  j["state"] = e.state;
  j["flight_phase"] = e.flight_phase;

  j["expected_intent"] = e.expected_intent;
  j["vrp_name_set"] = e.vrp_name_set;
  j["vrp_name"] = e.vrp_name_set ? json(e.vrp_name) : json(nullptr);

  if (e.is_readback)
    j["readback_missing_elements"] = e.readback_missing_elements;
  else
    j["readback_missing_elements"] = nullptr;

  j["failure_locus"] =
      e.emit_failure_locus ? json(e.failure_locus) : json(nullptr);

  // Compact single line, UTF-8 preserved, invalid bytes replaced rather
  // than thrown — a German VRP name with a stray byte must never abort
  // the measurement run.
  std::string line =
      j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);

  std::lock_guard<std::mutex> lk(g_mtx);
  std::ofstream f(g_path, std::ios::app);
  if (!f)
    return; // best-effort: a failed open must not disturb the sim
  f << line << '\n';
}

} // namespace cross_country_log
