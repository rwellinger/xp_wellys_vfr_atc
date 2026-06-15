/*
 * xp_wellys_atc - AI-powered ATC voice communication for X-Plane 12
 * Copyright (C) 2026 thWelly & Claude (Anthropic)
 *
 * Licensed under the GNU GPL-3.0-or-later. See LICENSE.
 */

#include "persistence/model_paths.hpp"

#include <XPLMPlugin.h>
#include <XPLMUtilities.h>

#include <sys/stat.h>

namespace model_paths {

namespace {

std::string g_plugin_root;
std::string g_models_dir;
std::string g_espeakng_data_dir;

// Pre-init guard: if a getter is called before init(), return a
// stable empty string by reference rather than crashing.
const std::string g_empty;

// Convert HFS-style colon paths to POSIX. X-Plane 12 normally returns
// POSIX paths now that we set XPLM_USE_NATIVE_PATHS in main, but this
// is a belt-and-braces match for what settings.cpp already does — same
// pattern, same caveats.
std::string normalise_path(std::string p) {
#if defined(__APPLE__)
  if (p.find(':') != std::string::npos && p.find('/') == std::string::npos) {
    auto colon = p.find(':');
    std::string posix = p.substr(colon + 1);
    for (char &c : posix)
      if (c == ':')
        c = '/';
    p = "/" + posix;
  }
#endif
  return p;
}

void mkdir_p(const std::string &dir) {
  // Best-effort recursive mkdir. We can't use std::filesystem::create_
  // directories here because libpiper's espeak-ng ExternalProject
  // builds against a different libstdc++ ABI; sticking to POSIX keeps
  // both worlds happy.
  std::string cur;
  for (size_t i = 0; i < dir.size(); ++i) {
    cur.push_back(dir[i]);
    if (dir[i] == '/' && cur.size() > 1) {
      mkdir(cur.c_str(), 0755);
    }
  }
  if (!cur.empty() && cur.back() != '/') {
    mkdir(cur.c_str(), 0755);
  }
}

} // namespace

void init() {
  if (!g_plugin_root.empty())
    return; // idempotent

  char raw[2048] = {};
  XPLMGetPluginInfo(XPLMGetMyID(), nullptr, raw, nullptr, nullptr);
  std::string xpl_path = normalise_path(raw);

  // .../plugins/xp_wellys_atc/mac_x64/xp_wellys_atc.xpl
  //   ^ plugin_root              ^ platform_dir   ^ filename
  // Strip the filename, then strip the platform dir.
  auto pos = xpl_path.rfind('/');
  if (pos != std::string::npos) {
    pos = xpl_path.rfind('/', pos - 1);
  }
  if (pos != std::string::npos) {
    g_plugin_root = xpl_path.substr(0, pos);
  } else {
    // Fallback for unusual installs — better than crashing.
    g_plugin_root = ".";
  }

  g_models_dir = g_plugin_root + "/Resources/models";
  g_espeakng_data_dir = g_plugin_root + "/Resources/espeak-ng-data";

  mkdir_p(g_models_dir);
  // Don't auto-create espeakng_data_dir — its absence indicates a
  // packaging error, not user state, and silently creating an empty
  // dir would mask that.

  XPLMDebugString("[xp_wellys_atc] Plugin root resolved\n");
}

const std::string &plugin_root() {
  return g_plugin_root.empty() ? g_empty : g_plugin_root;
}
const std::string &models_dir() {
  return g_models_dir.empty() ? g_empty : g_models_dir;
}
const std::string &espeakng_data_dir() {
  return g_espeakng_data_dir.empty() ? g_empty : g_espeakng_data_dir;
}

} // namespace model_paths
