#include "core/buttons.h"

namespace buttons {
namespace {

// `millis()` repasse à zéro au bout de 49 jours. Comparer des différences
// signées rend le calcul insensible à ce débordement.
bool elapsed(uint32_t now_ms, uint32_t since_ms, uint32_t delay_ms) {
  return static_cast<int32_t>(now_ms - since_ms) >= static_cast<int32_t>(delay_ms);
}

}  // namespace

bool Controller::settle(Channel& channel, bool level, uint32_t now_ms, bool& released) {
  released = false;

  if (level != channel.raw) {
    channel.raw = level;
    channel.changed_ms = now_ms;
    return false;
  }

  if (channel.raw == channel.stable) return false;
  if (!elapsed(now_ms, channel.changed_ms, kDebounceMs)) return false;

  channel.stable = channel.raw;
  if (channel.stable) {
    channel.pressed_ms = now_ms;
    channel.long_fired = false;
    return true;
  }

  released = true;
  return false;
}

void Controller::scheduleRefresh(uint32_t now_ms) {
  // Chaque nouvelle action repousse l'échéance : on ne redessine qu'une fois la
  // rafale terminée.
  refresh_pending_ = true;
  refresh_due_ms_ = now_ms + kCoalesceMs;
}

Action Controller::update(uint32_t now_ms, bool previous_down, bool next_down,
                          bool green_down) {
  bool released = false;

  if (settle(previous_, previous_down, now_ms, released)) {
    scheduleRefresh(now_ms);
    return Action::kPrevious;
  }

  if (settle(next_, next_down, now_ms, released)) {
    scheduleRefresh(now_ms);
    return Action::kNext;
  }

  const bool green_pressed = settle(green_, green_down, now_ms, released);
  (void)green_pressed;  // le bouton vert ne décide qu'au relâchement ou à 1 s

  // Appui long : l'action part sans attendre le relâchement, pour que le geste
  // soit accusé pendant qu'on tient encore le bouton.
  if (green_.stable && !green_.long_fired &&
      elapsed(now_ms, green_.pressed_ms, kLongPressMs)) {
    green_.long_fired = true;
    scheduleRefresh(now_ms);
    return Action::kForceRedraw;
  }

  // Relâchement court : lecture/pause. Après un appui long, le relâchement ne
  // doit rien produire — sinon un redessin forcé mettrait aussi la musique en
  // pause.
  if (released && !green_.long_fired) {
    scheduleRefresh(now_ms);
    return Action::kPlayPause;
  }

  return Action::kNone;
}

bool Controller::takeRefresh(uint32_t now_ms) {
  if (!refresh_pending_) return false;
  if (!elapsed(now_ms, refresh_due_ms_, 0)) return false;

  refresh_pending_ = false;
  return true;
}

}  // namespace buttons
