#pragma once

enum class InputEventType {
  Tap,
  LongPressStart,
  LongPressEnd,
  LongPressFail,
  Swipe,
  SwipeLeft,
  SwipeRight,
  SwipeUp,
  SwipeDown
};

struct InputEvent {
  InputEventType type;
  int16_t x, y;    // gesture origin
  int16_t dx, dy;  // total delta—for any extra logic you might need
};
