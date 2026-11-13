#include "app.h"

#include <stdlib.h>

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

// The indicators only show on the first screen: scrolled, the list starts higher.
int App::listTop(int first) const {
  return tasks.indicatorCount > 0 && first == 0 ? kTopWithIndicators : kTopNoIndicators;
}
int App::listTop() const { return listTop(scroll); }

bool App::needsHeading(int i, int first) const {
  const int s = tasks.items[i].section;
  if (s < 0) return false;
  return i == first || tasks.items[i - 1].section != s;
}

int App::measuresHeight(int i) const {
  const int s = tasks.items[i].section;
  if (s < 0 || (i > 0 && tasks.items[i - 1].section == s)) return 0;
  const int n = tasks.measuresIn(s);
  return n > 0 ? n * kMeasureH + 6 : 0;
}

int App::rowsFrom(int first) const {
  int y = listTop(first);
  int n = 0;
  for (int i = first; i < tasks.count; ++i) {
    const int h = (needsHeading(i, first) ? kSectionH + measuresHeight(i) : 0) +
                  (tasks.items[i].placeholder ? 0 : kRowH);
    if (y + h > kListBottom) break;
    y += h;
    ++n;
  }
  return n > 0 ? n : 1;
}

void App::keepShelfVisible() {
  SpineBox boxes[Library::kMax + 1];
  ShelfLayout::place(lib, boxes, Library::kMax + 1);
  const int shelf = boxes[libSelected].shelf;
  if (shelf < shelfScroll) shelfScroll = shelf;
  if (shelf >= shelfScroll + ShelfLayout::kShelves) shelfScroll = shelf - ShelfLayout::kShelves + 1;
}

Effects App::onLibraryButton(Button b) {
  Effects fx;
  const int items = lib.count + 1;  // + the "Ekle" spine
  if (libSelected >= items) libSelected = items - 1;
  SpineBox boxes[Library::kMax + 1];
  ShelfLayout::place(lib, boxes, Library::kMax + 1);
  switch (b) {
    case Button::Left:
      libSelected = (libSelected + items - 1) % items;
      break;
    case Button::Right:
      libSelected = (libSelected + 1) % items;
      break;
    case Button::Up:
    case Button::Down: {
      // Nearest spine (by centre) on the shelf above / below.
      const int target = boxes[libSelected].shelf + (b == Button::Up ? -1 : 1);
      const int cx = boxes[libSelected].x + boxes[libSelected].w / 2;
      int best = -1, bestDist = 1 << 30;
      for (int i = 0; i < items; ++i) {
        if (boxes[i].shelf != target) continue;
        const int d = abs(boxes[i].x + boxes[i].w / 2 - cx);
        if (d < bestDist) bestDist = d, best = i;
      }
      if (best < 0) return fx;
      libSelected = best;
      break;
    }
    case Button::Confirm:
      libStatus[0] = '\0';
      if (libSelected == lib.count) {
        wifiReturn = Screen::Library;
        screen = Screen::Wifi;
        wifiStatus[0] = '\0';
        fx.startWifi = fx.redraw = fx.fullRefresh = true;
      } else {
        screen = Screen::Reader;
        fx.openBook = fx.redraw = fx.fullRefresh = true;
      }
      return fx;
    case Button::Back:
      screen = Screen::List;
      tasks.sortFinishedLast();  // finished tasks sink when the list is reopened
      selected = 0;
      scroll = 0;
      fx.redraw = fx.fullRefresh = true;
      return fx;
    default:
      return fx;
  }
  keepShelfVisible();
  fx.redraw = true;
  return fx;
}

void App::keepSelectionVisible() {
  if (selected < scroll) scroll = selected;
  while (scroll < selected && selected >= scroll + rowsFrom(scroll)) ++scroll;
  if (scroll < 0) scroll = 0;
}

Effects App::onButton(Button b, uint32_t nowMs) {
  lastInputMs = nowMs;
  Effects fx;

  if (b == Button::Power && screen != Screen::Focus) {
    fx.sleep = true;
    fx.stopWifi = screen == Screen::Wifi;
    fx.closeBook = screen == Screen::Reader;
    return fx;
  }

  switch (screen) {
    case Screen::List:
      if (b == Button::Back) {
        screen = Screen::Library;
        keepShelfVisible();
        fx.redraw = fx.fullRefresh = fx.deepClean = true;
        break;
      }
      if (b == Button::Left && syncConfigured) {
        // Sync straight over the saved home/phone Wi-Fi: no hotspot needed.
        fx.syncNow = fx.redraw = true;
        break;
      }
      if (b == Button::Left) {  // not set up yet: open the transfer page
        wifiReturn = Screen::List;
        screen = Screen::Wifi;
        wifiStatus[0] = '\0';
        fx.startWifi = fx.redraw = fx.fullRefresh = true;
        break;
      }
      if (tasks.count == 0) break;
      if (b == Button::Up) {
        selected = (selected + tasks.count - 1) % tasks.count;
        keepSelectionVisible();
        fx.redraw = true;
      } else if (b == Button::Down) {
        selected = (selected + 1) % tasks.count;
        keepSelectionVisible();
        fx.redraw = true;
      } else if (b == Button::Confirm && tasks.items[selected].placeholder) {
        // the measures block: nothing to mark
      } else if (b == Button::Confirm) {
        // Open -> started -> done -> cancelled -> open.
        Task& t = tasks.items[selected];
        if (t.cancelled) {
          t.cancelled = false;
        } else if (t.done) {
          t.done = false;
          t.cancelled = true;
        } else if (t.started) {
          t.started = false;
          t.done = true;
        } else {
          t.started = true;
        }
        fx.redraw = fx.save = true;
      } else if (b == Button::Right && !tasks.items[selected].placeholder) {
        screen = Screen::Focus;
        focusTask = selected;
        focusStartMs = nowMs;
        focusElapsedBeforePause = 0;
        focusPaused = false;
        shownMinutes = focusMinutesLeft(nowMs);
        fx.redraw = fx.fullRefresh = true;
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

    case Screen::Library:
      return onLibraryButton(b);

    case Screen::Reader:
      if (b == Button::Right || b == Button::Down) {
        fx.pageDelta = 1;
        fx.redraw = true;
      } else if (b == Button::Left || b == Button::Up) {
        fx.pageDelta = -1;
        fx.redraw = true;
      } else if (b == Button::Back) {
        screen = Screen::Library;
        fx.closeBook = fx.redraw = fx.fullRefresh = true;
      } else if (b == Button::Confirm) {
        fx.nextFontSize = fx.redraw = fx.fullRefresh = true;
      }
      break;

    case Screen::Wifi:
      if (b == Button::Confirm && syncConfigured) {
        // Leave the hotspot and sync over the home Wi-Fi instead.
        screen = Screen::List;
        fx.stopWifi = fx.syncNow = true;
        break;
      }
      if (b == Button::Back) {
        screen = wifiReturn;
        if (selected >= tasks.count) selected = tasks.count > 0 ? tasks.count - 1 : 0;
        keepSelectionVisible();
        fx.stopWifi = fx.redraw = fx.fullRefresh = true;
      }
      break;

    case Screen::FocusDone:
      if (b == Button::Confirm) {
        if (focusTask >= 0 && focusTask < tasks.count) {
          tasks.items[focusTask].done = true;
          tasks.items[focusTask].cancelled = false;
          tasks.items[focusTask].started = false;
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
  const uint32_t idle =
      screen == Screen::Wifi ? kWifiIdleMs : screen == Screen::Reader ? kReaderIdleMs : kIdleSleepMs;
  if (nowMs - lastInputMs >= idle) {
    fx.sleep = true;
    fx.stopWifi = screen == Screen::Wifi;
    fx.closeBook = screen == Screen::Reader;
  }
  return fx;
}
