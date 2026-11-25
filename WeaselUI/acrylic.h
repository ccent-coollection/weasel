#pragma once

// Acrylic (blurred) backdrop for the candidate window.
//
// Weasel composites through a DirectComposition swap chain on a
// WS_EX_NOREDIRECTIONBITMAP window, so the theme's back_color alpha already
// reaches the desktop. Handing the same colour to the DWM accent API puts the
// system acrylic blur behind that background, which is what macOS Squirrel
// calls translucency.
//
// Two things have to line up for the result to look right:
//   * the accent covers the whole window, so the window must be exactly the
//     painted candidate bar; the layout's shadow margin is therefore flattened
//     by WeaselPanel::_CreateLayout when the backdrop is enabled;
//   * the accent itself is rectangular, so the window asks DWM for rounded
//     corners, which clips the blur to the shape of the bar.
//
// Both entry points are resolved at run time: SetWindowCompositionAttribute is
// undocumented, and dwmapi.dll is not part of weasel's link line.

#include <windows.h>

namespace weasel {
namespace acrylic {

// A translucent theme background is the switch: alpha 0xFF keeps the previous
// opaque appearance, anything else turns the backdrop on.
inline bool IsEnabled(int back_color) {
  return ((static_cast<unsigned int>(back_color) >> 24) & 0xff) != 0xff;
}

inline void Apply(HWND hwnd, int back_color) {
  if (!hwnd || !IsEnabled(back_color))
    return;

  enum { kAccentPolicyAttribute = 19, kEnableAcrylicBlurBehind = 4 };
  struct AccentPolicy {
    int state;
    int flags;
    int gradient_color;
    int animation_id;
  };
  struct WindowCompositionAttribData {
    int attrib;
    void* data;
    SIZE_T size;
  };
  using SetWindowCompositionAttributeFn =
      BOOL(WINAPI*)(HWND, WindowCompositionAttribData*);

  static const auto set_window_composition_attribute =
      reinterpret_cast<SetWindowCompositionAttributeFn>(::GetProcAddress(
          ::GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
  if (!set_window_composition_attribute)
    return;

  // UIStyle colours are stored as 0xAABBGGRR, the layout the accent API wants.
  AccentPolicy policy = {kEnableAcrylicBlurBehind, 2, back_color, 0};
  WindowCompositionAttribData data = {kAccentPolicyAttribute, &policy,
                                      sizeof(policy)};
  set_window_composition_attribute(hwnd, &data);

  // Clip the rectangular accent to a rounded window so the blur never shows
  // outside the candidate bar.
  enum { kWindowCornerPreference = 33, kRoundSmall = 3 };
  using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
  static const auto set_window_attribute =
      reinterpret_cast<DwmSetWindowAttributeFn>(::GetProcAddress(
          ::LoadLibraryW(L"dwmapi.dll"), "DwmSetWindowAttribute"));
  if (set_window_attribute) {
    int preference = kRoundSmall;
    set_window_attribute(hwnd, kWindowCornerPreference, &preference,
                         sizeof(preference));
  }
}

}  // namespace acrylic
}  // namespace weasel
