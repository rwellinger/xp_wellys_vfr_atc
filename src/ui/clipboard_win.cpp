/*
 * xp_wellys_devfr_atc - AI-powered ATC voice communication for X-Plane 12
 * Copyright (C) 2026 thWelly & Claude (Anthropic)
 *
 * Licensed under the GNU GPL-3.0-or-later. See LICENSE.
 *
 * Windows build gate (issue #18). The macOS NSPasteboard bridge lives
 * in clipboard.mm, which the Windows build excludes. This is a linkable
 * stub so the plugin builds cloud-only on Windows; the real Win32
 * clipboard (OpenClipboard / GetClipboardData) is issue #22. Until then
 * the [Paste]/[Copy] buttons are inert on Windows.
 */

#include "ui/clipboard.hpp"

namespace ui::clipboard {

std::string read_system_text() { return {}; }

void write_system_text(const std::string & /*text*/) {}

} // namespace ui::clipboard
