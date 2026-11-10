#include "app.h"

uint32_t App::focusElapsed(uint32_t nowMs) const {
  if (focusPaused) return focusElapsedBeforePause;
  return focusElapsedBeforePause + (nowMs - focusStartMs);
}

int App::focusMinutesLeft(uint32_t nowMs) const {
  const uint32_t elapsed = focusElapsed(nowMs);
  if (elapsed >= kFocusMs) return 0;
  const uint32_t left = kFocusMs - elapsed;
  return static_cast<int>((left + 59999) / 60000);
}

void App::keepSelectionVisible() {
  if (selected < scroll) scroll = selected;
  if (selected >= scroll + kVisibleRows) scroll = selected - kVisibleRows + 1;
  if (scroll < 0) scroll = 0;
}

Effects App::onButton(Button b, uint32_t nowMs) {
  lastInputMs = nowMs;
  Effects fx;

  if (b == Button::Power && screen != Screen::Focus) {
    fx.sleep = true;
    return fx;
  }

  switch (screen) {
    case Screen::List:
      if (tasks.count == 0) {
        if (b == Button::Back) { fx.redraw = fx.fullRefresh = true; }
        break;
      }
      if (b == Button::Up) {
        selected = (selected + tasks.count - 1) % tasks.count;
        keepSelectionVisible();
        fx.redraw = true;
      } else if (b == Button::Down) {
        selected = (selected + 1) % tasks.count;
        keepSelectionVisible();
        fx.redraw = true;
      } else if (b == Button::Confirm) {
        tasks.items[selected].done = !tasks.items[selected].done;
        fx.redraw = fx.save = true;
      } else if (b == Button::Right) {
        screen = Screen::Focus;
        focusTask = selected;
        focusStartMs = nowMs;
        focusElapsedBeforePause = 0;
        focusPaused = false;
        shownMinutes = focusMinutesLeft(nowMs);
        fx.redraw = fx.fullRefresh = true;
      } else if (b == Button::Back) {
        fx.redraw = fx.fullRefresh = true;  // manual ghosting cleanup
      }
      break;

    case Screen::Focus:
      if (b == Button::Confirm) {
        if (focusPaused) {
          focusPaused = false;
          focusStartMs = nowMs;
        } else {
          focusElapsedBeforePause = focusElapsed(nowMs);
          focusPaused = true;
        }
        fx.redraw = true;
      } else if (b == Button::Back) {
        screen = Screen::List;
        fx.redraw = fx.fullRefresh = true;
      }
      break;

    case Screen::FocusDone:
      if (b == Button::Confirm) {
        if (focusTask >= 0 && focusTask < tasks.count) {
          tasks.items[focusTask].done = true;
          fx.save = true;
        }
        screen = Screen::List;
        fx.redraw = fx.fullRefresh = true;
      } else if (b == Button::Back) {
        screen = Screen::List;
        fx.redraw = fx.fullRefresh = true;
      }
      break;
  }
  return fx;
}

Effects App::onTick(uint32_t nowMs) {
  Effects fx;
  if (screen == Screen::Focus) {
    const int left = focusMinutesLeft(nowMs);
    if (left == 0) {
      screen = Screen::FocusDone;
      lastInputMs = nowMs;  // give the user time to see it before idle sleep
      fx.redraw = fx.fullRefresh = true;
    } else if (left != shownMinutes) {
      shownMinutes = left;
      fx.redraw = true;
    }
    return fx;
  }
  if (nowMs - lastInputMs >= kIdleSleepMs) fx.sleep = true;
  return fx;
}
