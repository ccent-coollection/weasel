#pragma once

// Acrylic (blurred) backdrop for the candidate window.
//
// Weasel draws through a DirectComposition swap chain on a
// WS_EX_NOREDIRECTIONBITMAP window, so the theme's back_color alpha already
// reaches the desktop. Handing the same colour to the DWM accent API puts the
// system acrylic blur *behind* that background, which is what macOS Squirrel
// calls "translucency".
//
// SetWindowCompositionAttribute is undocumented and absent on Windows 7, so it
// is resolved at run time; when it is missing the window simply keeps the
// plain translucent background.

#include <windows.h>

namespace weasel {
namespace acrylic {

inline void Apply(HWND hwnd, int back_color) {
  if (!hwnd)
    return;
  // Opaque themes keep their current appearance: there is nothing to blur.
  if (((static_cast<unsigned int>(back_color) >> 24) & 0xff) == 0xff)
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
}

}  // namespace acrylic
}  // namespace weasel
