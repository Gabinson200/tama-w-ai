#pragma once
#include <vector>
#include "touch.h"                   // TouchInfo, update_global_touch_info() :contentReference[oaicite:0]{index=0}
                // BaseGestureRecognizer, TapGestureRecognizer, LongPressGestureRecognizer :contentReference[oaicite:1]{index=1}
                 // swipe_tracker_t, update_swipe_state() :contentReference[oaicite:2]{index=2}
#include "input_event.h"

class InputManager {
public:
  // configure swipe bounds & minimum length
  InputManager(int x_min, int x_max, int y_min, int y_max, int min_swipe_len);

  // call once per loop
  void update();

  // after update(), read out all generated events
  const std::vector<InputEvent>& events() const { return _events; }
  void clearEvents()            { _events.clear(); }

private:
  TouchInfo             _touch;       // raw + debounced
  TapGestureRecognizer  _tapRec;
  LongPressGestureRecognizer _lpRec;
  swipe_tracker_t       _swipeTracker;

  int _xMin, _xMax, _yMin, _yMax, _minSwipeLen;
  std::vector<InputEvent> _events;

  // previous frame’s states, for edge detection
  GestureState prevTapState        = GestureState::IDLE;
  GestureState prevLongPressState = GestureState::IDLE;
  swipe_state_t prevSwipeState    = SWIPE_IDLE;

  void resolveConflicts();
  void emitTap();
  void emitLongPressStart();
  void emitLongPressEnd();
  void emitSwipe();
  void emitSwipeLeft();
  void emitSwipeRight();
  void emitSwipeUp();
  void emitSwipeDown();

};
