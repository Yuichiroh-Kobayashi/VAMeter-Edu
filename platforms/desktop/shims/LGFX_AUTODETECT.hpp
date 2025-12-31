#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#if __has_include(<lgfx/v1/platforms/sdl/Panel_SDL.hpp>)
  #include <lgfx/v1/platforms/sdl/Panel_SDL.hpp>
  using PanelSDL = lgfx::Panel_SDL;
#elif __has_include(<lgfx/v1/platforms/sdl/Panel_sdl.hpp>)
  #include <lgfx/v1/platforms/sdl/Panel_sdl.hpp>
  using PanelSDL = lgfx::Panel_sdl;
#else
  #error "LovyanGFX SDL panel header not found (Panel_SDL.hpp / Panel_sdl.hpp)"
#endif

#include <type_traits>
#include <utility>

namespace lgfx_sdl_detail {

// メソッドの有無を検出
template<typename T, typename = void> struct has_setPanelSize       : std::false_type {};
template<typename T> struct has_setPanelSize<T,       std::void_t<decltype(std::declval<T&>().setPanelSize(0,0))>>       : std::true_type {};

template<typename T, typename = void> struct has_setPanelResolution : std::false_type {};
template<typename T> struct has_setPanelResolution<T, std::void_t<decltype(std::declval<T&>().setPanelResolution(0,0))>> : std::true_type {};

template<typename T, typename = void> struct has_setWindowSize      : std::false_type {};
template<typename T> struct has_setWindowSize<T,      std::void_t<decltype(std::declval<T&>().setWindowSize(0,0))>>      : std::true_type {};

template<typename T, typename = void> struct has_setSize            : std::false_type {};
template<typename T> struct has_setSize<T,            std::void_t<decltype(std::declval<T&>().setSize(0,0))>>            : std::true_type {};

// あるやつを呼ぶ
template<typename T>
inline void apply_size(T& p, int w, int h) {
  if constexpr (has_setPanelSize<T>::value)        { p.setPanelSize(w, h); }
  else if constexpr (has_setPanelResolution<T>::value) { p.setPanelResolution(w, h); }
  else if constexpr (has_setWindowSize<T>::value)  { p.setWindowSize(w, h); }
  else if constexpr (has_setSize<T>::value)        { p.setSize(w, h); }
  else { /* no-op: デフォルトサイズ */ }
}

} // namespace

// SDL用 LGFX デバイス
class LGFX : public lgfx::LGFX_Device {
public:
  explicit LGFX(int w, int h) {
    lgfx_sdl_detail::apply_size(_panel, w, h);
    setPanel(&_panel);
  }
private:
  PanelSDL _panel;
};
