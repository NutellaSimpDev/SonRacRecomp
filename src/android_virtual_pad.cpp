// SonRac - on-screen gamepad bridge (Android)
//
// The touch overlay (VirtualPadView.kt) sends its whole state here on every
// change. It is exposed to the game as an SDL virtual Xbox 360 gamepad, so the
// runtime's SDL input driver picks it up like a physical controller (it handles
// SDL_EVENT_GAMEPAD_ADDED) and no game-side code is needed.

#if defined(__ANDROID__)

#include <SDL3/SDL.h>
#include <android/log.h>
#include <jni.h>

#include <mutex>

namespace {

std::mutex g_pad_mutex;
SDL_Joystick* g_pad = nullptr;

// Same order as VirtualPadView.Button (bit index = SDL_GamepadButton).
constexpr int kButtonCount = SDL_GAMEPAD_BUTTON_COUNT;

SDL_Joystick* EnsurePad() {
  if (g_pad) return g_pad;
  if (!SDL_WasInit(SDL_INIT_GAMEPAD)) return nullptr;  // runtime not up yet

  SDL_VirtualJoystickDesc desc;
  SDL_INIT_INTERFACE(&desc);
  desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
  // Xbox 360 controller ids, so SDL and the runtime treat it as one.
  desc.vendor_id = 0x045E;
  desc.product_id = 0x028E;
  desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
  desc.nbuttons = kButtonCount;
  desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
  desc.button_mask = (1u << kButtonCount) - 1;
  desc.name = "SonRac Touch Pad";

  SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
  if (!id) {
    __android_log_print(ANDROID_LOG_ERROR, "SonRac", "virtual pad: attach failed: %s",
                        SDL_GetError());
    return nullptr;
  }
  g_pad = SDL_OpenJoystick(id);
  __android_log_print(ANDROID_LOG_INFO, "SonRac", "virtual pad attached (id %u)",
                      static_cast<unsigned>(id));
  return g_pad;
}

Sint16 ToStickAxis(float v) {
  if (v > 1.f) v = 1.f;
  if (v < -1.f) v = -1.f;
  return static_cast<Sint16>(v * 32767.f);
}

Sint16 ToTriggerAxis(float v) {
  if (v > 1.f) v = 1.f;
  if (v < 0.f) v = 0.f;
  return static_cast<Sint16>(v * 32767.f);
}

}  // namespace

extern "C" JNIEXPORT void JNICALL Java_com_sonrac_game_GameActivity_nativeSetPadState(
    JNIEnv*, jclass, jint buttons, jfloat lx, jfloat ly, jfloat rx, jfloat ry, jfloat lt,
    jfloat rt) {
  std::lock_guard<std::mutex> lock(g_pad_mutex);
  SDL_Joystick* pad = EnsurePad();
  if (!pad) return;
  SDL_LockJoysticks();
  for (int i = 0; i < kButtonCount; ++i) {
    SDL_SetJoystickVirtualButton(pad, i, (buttons >> i) & 1);
  }
  SDL_SetJoystickVirtualAxis(pad, SDL_GAMEPAD_AXIS_LEFTX, ToStickAxis(lx));
  SDL_SetJoystickVirtualAxis(pad, SDL_GAMEPAD_AXIS_LEFTY, ToStickAxis(ly));
  SDL_SetJoystickVirtualAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX, ToStickAxis(rx));
  SDL_SetJoystickVirtualAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY, ToStickAxis(ry));
  SDL_SetJoystickVirtualAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, ToTriggerAxis(lt));
  SDL_SetJoystickVirtualAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, ToTriggerAxis(rt));
  SDL_UnlockJoysticks();
}

#endif  // __ANDROID__
