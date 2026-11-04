/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>
#include <string>
#include <windows.h>

#include "utfconv.hh"

/* Keep portable copies independent of registered file handlers and of stale
 * taskbar shortcuts belonging to another installation of the same version. */
inline const wchar_t *GHOST_win32_app_id()
{
  static const std::wstring app_id = []() {
    std::wstring result;
    UTF16_ENCODE(BLENDER_WIN_APPID);
    result = BLENDER_WIN_APPID_16;
    UTF16_UN_ENCODE(BLENDER_WIN_APPID);

    std::wstring executable(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), DWORD(executable.size()));
    if (length == 0 || length >= executable.size()) {
      return result;
    }
    executable.resize(length);
    CharLowerBuffW(executable.data(), length);
    uint64_t hash = 14695981039346656037ULL;
    for (const wchar_t character : executable) {
      hash ^= uint16_t(character);
      hash *= 1099511628211ULL;
    }
    return result + L".portable." + std::to_wstring(hash);
  }();
  return app_id.c_str();
}
