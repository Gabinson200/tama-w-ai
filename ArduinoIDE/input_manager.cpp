#include "input_manager.h"
//#include <cmath>  // for std::abs

InputManager::InputManager(int x_min, int x_max,
                           int y_min, int y_max,
                           int min_swipe_len)
  : _tapRec(200, 48, 80),
    _lpRec(1000, 24),
    _xMin(x_min), _xMax(x_max),
    _yMin(y_min), _yMax(y_max),
    _minSwipeLen(min_swipe_len)
{
  // if you want, restrict gesture target areas here:
  // _tapRec.set_target_area({...});  etc.
}

void InputManager::update() {
  // 1) clear last frame’s events
  _events.clear();

  // 2) get debounced touch info
  update_global_touch_info(&_touch);  // exact same call as before

  // 3) update LP & Swipe recognizers FIRST
  _lpRec.update(_touch);
  update_swipe_state(
    /*x_min=*/_xMin, /*x_max=*/_xMax,
    /*y_min=*/_yMin, /*y_max=*/_yMax,
    /*min_len=*/_minSwipeLen,
    &_swipeTracker,
    _touch
  );

  // 4) if the screen is still pressed, cancel any pending tap
  //    once a long-press has begun/failed or we’re in a drag
  if (_touch.is_pressed) {
    if (_lpRec.get_state() == GestureState::BEGAN ||
        _lpRec.get_state() == GestureState::FAILED ||
        _swipeTracker.state    == SWIPE_DRAGGING)
    {
      _tapRec.notify_current_press_is_claimed();
    }
  }

  // 5) update Tap recognizer
  _tapRec.update(_touch);

  // 6) if we were waiting for tap-confirmation, cancel it now
  //    if a swipe finished or a long-press just ended on release
  if (_tapRec.is_waiting_for_confirmation()) {
    bool cancel_tap = false;
    if (_swipeTracker.swipeDetected) {
      cancel_tap = true;
    }
    if (_lpRec.get_state() == GestureState::ENDED) {
      cancel_tap = true;
    }
    if (cancel_tap && !_touch.is_pressed) {
      _tapRec.cancel();
    }
  }

  // 7) grab current states to compare with prev
  GestureState tapState       = _tapRec.get_state();
  GestureState longPressState = _lpRec.get_state();
  swipe_state_t swipeState    = _swipeTracker.state;

  // 8) exactly the same “react” logic from your loop, now pushing events:

  // — Tap Ended
  if (tapState == GestureState::ENDED && prevTapState != GestureState::ENDED) {
    _events.push_back({
      InputEventType::Tap,
      _tapRec.get_tap_x(),
      _tapRec.get_tap_y(),
      0, 0
    });
    _tapRec.reset();
  }

  // — Long Press changes
  if (longPressState != prevLongPressState) {
    if (longPressState == GestureState::BEGAN) {
      _events.push_back({
        InputEventType::LongPressStart,
        _lpRec.recognized_at_x,
        _lpRec.recognized_at_y,
        0, 0
      });
    }
    else if (longPressState == GestureState::ENDED) {
      _events.push_back({
        InputEventType::LongPressEnd,
        _touch.x,    // release point
        _touch.y,
        0, 0
      });
      _lpRec.reset();
    }
    else if (longPressState == GestureState::FAILED) {
      _events.push_back({
        InputEventType::LongPressFail,
        0, 0, 0, 0
      });
      _lpRec.reset();
    }
  }

  // — Swipe Detected
  if (_swipeTracker.swipeDetected) {
    // directional event
    InputEventType dirEv;
    switch (_swipeTracker.swipeDir) {
      case SWIPE_DIR_LEFT:  dirEv = InputEventType::SwipeLeft;  break;
      case SWIPE_DIR_RIGHT: dirEv = InputEventType::SwipeRight; break;
      case SWIPE_DIR_UP:    dirEv = InputEventType::SwipeUp;    break;
      case SWIPE_DIR_DOWN:  dirEv = InputEventType::SwipeDown;  break;
      default:              dirEv = InputEventType::Swipe;      break;
    }
    _events.push_back({
      dirEv,
      _swipeTracker.startX,
      _swipeTracker.startY,
      int16_t(_swipeTracker.lastGoodX - _swipeTracker.startX),
      int16_t(_swipeTracker.lastGoodY - _swipeTracker.startY)
    });
    // clear so it won’t fire again until next swipe
    _swipeTracker.swipeDetected = false;
  }

  // 9) stash states for next frame
  prevTapState        = tapState;
  prevLongPressState  = longPressState;
  prevSwipeState      = swipeState;
}


void InputManager::resolveConflicts() {
  // if a long-press is active or just started, suppress tap & swipe
  if (_lpRec.get_state()==GestureState::BEGAN ||
      _lpRec.get_state()==GestureState::POSSIBLE) {
    _tapRec.notify_current_press_is_claimed();
    _swipeTracker.swipeDetected = false;
  }
  // (you can add further rules here)
}

void InputManager::emitTap() {
  InputEvent e{InputEventType::Tap,
               (int16_t)_tapRec.get_tap_x(),
               (int16_t)_tapRec.get_tap_y(),
               0, 0};
  _events.push_back(e);
}

void InputManager::emitLongPressStart() {
  InputEvent e{InputEventType::LongPressStart,
               (int16_t)_lpRec.recognized_at_x,
               (int16_t)_lpRec.recognized_at_y,
               0, 0};
  _events.push_back(e);
}

void InputManager::emitLongPressEnd() {
  InputEvent e{InputEventType::LongPressEnd,
               (int16_t)_lpRec.recognized_at_x,
               (int16_t)_lpRec.recognized_at_y,
               0, 0};
  _events.push_back(e);
}

void InputManager::emitSwipe() {
  int16_t dx = _swipeTracker.currentX - _swipeTracker.startX;
  int16_t dy = _swipeTracker.currentY - _swipeTracker.startY;

  // manual absolute to avoid Arduino's abs() macro
  int16_t adx = dx < 0 ? -dx : dx;
  int16_t ady = dy < 0 ? -dy : dy;

  InputEventType dir;
  if (adx > ady) {
    dir = (dx > 0)
        ? InputEventType::SwipeRight
        : InputEventType::SwipeLeft;
  } else {
    dir = (dy > 0)
        ? InputEventType::SwipeDown
        : InputEventType::SwipeUp;
  }

  _events.push_back(InputEvent{
    dir,
    _swipeTracker.startX,
    _swipeTracker.startY,
    dx,
    dy
  });
}

void InputManager::emitSwipeLeft() {
  _events.push_back({ InputEventType::SwipeLeft,
                      _swipeTracker.startX,
                      _swipeTracker.startY,
                      int16_t(_swipeTracker.currentX - _swipeTracker.startX),
                      int16_t(_swipeTracker.currentY - _swipeTracker.startY) });
}

void InputManager::emitSwipeRight() {
  _events.push_back({ InputEventType::SwipeRight,
                      _swipeTracker.startX,
                      _swipeTracker.startY,
                      int16_t(_swipeTracker.currentX - _swipeTracker.startX),
                      int16_t(_swipeTracker.currentY - _swipeTracker.startY) });
}

void InputManager::emitSwipeDown() {
  _events.push_back({ InputEventType::SwipeDown,
                      _swipeTracker.startX,
                      _swipeTracker.startY,
                      int16_t(_swipeTracker.currentX - _swipeTracker.startX),
                      int16_t(_swipeTracker.currentY - _swipeTracker.startY) });
}

void InputManager::emitSwipeUp() {
  _events.push_back({ InputEventType::SwipeUp,
                      _swipeTracker.startX,
                      _swipeTracker.startY,
                      int16_t(_swipeTracker.currentX - _swipeTracker.startX),
                      int16_t(_swipeTracker.currentY - _swipeTracker.startY) });
}

