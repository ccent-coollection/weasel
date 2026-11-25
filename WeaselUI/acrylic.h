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
//   * the accent is rectangular and ignores the window's rounded corners, so
//     the window is clipped to the bar's rounded rect instead, which makes the
//     blur follow the same shape as the painted background.
//
// SetWindowCompositionAttribute is undocumented, so it is resolved at run time.

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
}

// The accent is rectangular, so without this the blur keeps square corners
// behind the rounded bar and the two shapes visibly disagree.
inline void ClipRounded(HWND hwnd, int radius) {
  if (!hwnd)
    return;
  RECT rc{};
  if (!::GetClientRect(hwnd, &rc))
    return;
  const int width = rc.right - rc.left;
  const int height = rc.bottom - rc.top;
  if (width <= 0 || height <= 0)
    return;
  const int limit = (width < height ? width : height) / 2;
  int r = radius < limit ? radius : limit;
  if (r < 0)
    r = 0;
  HRGN region = r > 0 ? ::CreateRoundRectRgn(0, 0, width + 1, height + 1, r * 2,
                                             r * 2)
                      : ::CreateRectRgn(0, 0, width + 1, height + 1);
  if (!region)
    return;
  // SetWindowRgn takes ownership of the region on success, and leaves it to
  // the caller on failure.
  if (::SetWindowRgn(hwnd, region, TRUE) == 0)
    ::DeleteObject(region);
}

}  // namespace acrylic
}  // namespace weasel