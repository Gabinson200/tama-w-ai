#include "anim.h"
#include <math.h>
#include <Arduino.h>
//–––––– Base class implementation ––––––

SpriteStackAnimation::SpriteStackAnimation(uint32_t duration, uint32_t delay)
: _duration(duration), _delay(delay), _startTime(0), _active(false) {}

SpriteStackAnimation::~SpriteStackAnimation() {}

bool SpriteStackAnimation::isActive() const {
    return _active;
}

//–––––– RotationAnimation Implementation ––––––

RotationAnimation::RotationAnimation(SpriteStack &sprite, float startAngle, float endAngle,
                                     uint32_t duration, uint32_t delay)
  : SpriteStackAnimation(duration, delay),
    _sprite(sprite),
    _startAngle(startAngle),
    _endAngle(endAngle)
{}

void RotationAnimation::start() {
    _startTime = millis();
    _active = true;
    // Initialize to start angle (assume pitch and yaw remain 0).
    _sprite.getRotation(_basePitch, _baseYaw, _baseRoll); 
    _sprite.setRotation(_basePitch, _baseYaw, _baseRoll + _startAngle);
}

void RotationAnimation::update() {
    if (!_active) return;
    uint32_t now = millis();
    if (now < _startTime + _delay) {
        // Still in delay period.
        return;
    }
    uint32_t elapsed = now - (_startTime + _delay);
    if (elapsed >= _duration) {
        // Animation complete.
        _sprite.setRotation(_basePitch, _baseYaw, _baseRoll + _endAngle);
        _active = false;
    } else {
        float t = (float)elapsed / _duration; // linear interpolation factor [0,1]
        float currentAngle = _baseRoll + _startAngle + t * (_endAngle - _startAngle);
        _sprite.setRotation(_basePitch, _baseYaw, currentAngle);
    }
}

//–––––– ZoomAnimation Implementation ––––––

ZoomAnimation::ZoomAnimation(SpriteStack &sprite, float startZoom, float endZoom,
                             uint32_t duration, uint32_t delay)
  : SpriteStackAnimation(duration, delay),
    _sprite(sprite),
    _startZoom(startZoom),
    _endZoom(endZoom)
{}

void ZoomAnimation::start() {
    _startTime = millis();
    _active = true;
    _sprite.setZoom(_startZoom);
}

void ZoomAnimation::update() {
    if (!_active) return;
    uint32_t now = millis();
    if (now < _startTime + _delay) return;
    uint32_t elapsed = now - (_startTime + _delay);
    if (elapsed >= _duration) {
        _sprite.setZoom(_endZoom);
        _active = false;
    } else {
        float t = (float)elapsed / _duration;
        float currentZoom = _startZoom + t * (_endZoom - _startZoom);
        _sprite.setZoom(currentZoom);
    }
}

//–––––– PositionAnimation Implementation ––––––

PositionAnimation::PositionAnimation(SpriteStack &sprite, Point startPos, Point endPos,
                                     uint32_t duration, uint32_t delay)
  : SpriteStackAnimation(duration, delay),
    _sprite(sprite),
    _startPos(startPos),
    _endPos(endPos)
{}

void PositionAnimation::start() {
    _startTime = millis();
    _active = true;
    _sprite.setPosition(_startPos.x, _startPos.y);
}

void PositionAnimation::update() {
    if (!_active) return;
    uint32_t now = millis();
    if (now < _startTime + _delay) return;
    uint32_t elapsed = now - (_startTime + _delay);
    if (elapsed >= _duration) {
        _sprite.setPosition(_endPos.x, _endPos.y);
        _active = false;
    } else {
        float t = (float)elapsed / _duration;
        int currentX = _startPos.x + (int)(t * (_endPos.x - _startPos.x));
        int currentY = _startPos.y + (int)(t * (_endPos.y - _startPos.y));
        _sprite.setPosition(currentX, currentY);
    }
}

//–––––– NoNoAnimation Implementation ––––––

NoNoAnimation::NoNoAnimation(SpriteStack &sprite, float startAngle, float endAngle,
                                     uint32_t duration, uint32_t delay)
  : SpriteStackAnimation(duration, delay),
    _sprite(sprite),
    _startAngle(startAngle),
    _endAngle(endAngle)
{}

void NoNoAnimation::start() {
    _startTime = millis();
    _active = true;
    // Initialize to start angle (assume pitch and yaw remain 0).
    _sprite.getRotation(_basePitch, _baseYaw, _baseRoll); 
    _sprite.setRotation(_basePitch, _baseYaw, _baseRoll);
}

void NoNoAnimation::update() {
    if (!_active) return;
    uint32_t now = millis();
    if (now < _startTime + _delay) {
        // Still in delay period.
        return;
    }
    uint32_t elapsed = now - (_startTime + _delay);
    if (elapsed >= _duration) {
        // Animation complete.
        _sprite.setRotation(_basePitch, _baseYaw, _baseRoll);
        _active = false;
    } else {
        float t = (float)elapsed / _duration; // linear interpolation factor [0,1]
        float currentAngle = _baseRoll + (_endAngle - _startAngle) * sin(t * 2 * PI);
        _sprite.setRotation(_basePitch, _baseYaw, currentAngle);
    }
}

//–––––– NodAnimation Implementation ––––––

NodAnimation::NodAnimation(SpriteStack &sprite, float startAngle, float endAngle,
                                     uint32_t duration, uint32_t delay)
  : SpriteStackAnimation(duration, delay),
    _sprite(sprite),
    _startAngle(startAngle),
    _endAngle(endAngle)
{}

void NodAnimation::start() {
    _startTime = millis();
    _active = true;
    // Initialize to start angle (assume pitch and yaw remain 0).
    _sprite.getRotation(_basePitch, _baseYaw, _baseRoll); 
    _sprite.setRotation(_basePitch, _baseYaw, _baseRoll);
}

void NodAnimation::update() {
    if (!_active) return;
    uint32_t now = millis();
    if (now < _startTime + _delay) {
        // Still in delay period.
        return;
    }
    uint32_t elapsed = now - (_startTime + _delay);
    if (elapsed >= _duration) {
        // Animation complete.
        _sprite.setRotation(_basePitch, _baseYaw, _baseRoll);
        _active = false;
    } else {
        float t = (float)elapsed / _duration; // linear interpolation factor [0,1]
        float currentAngle = _basePitch + (_endAngle - _startAngle) * sin(t * PI);
        _sprite.setRotation(currentAngle, _baseYaw, _baseRoll);
    }
}

//–––––– DanceAnimation Implementation ––––––

DanceAnimation::DanceAnimation(SpriteStack &sprite, float startAngle, float endAngle,
                                     uint32_t duration, uint32_t delay)
  : SpriteStackAnimation(duration, delay),
    _sprite(sprite),
    _startAngle(startAngle),
    _endAngle(endAngle)
{}

void DanceAnimation::start() {
    _startTime = millis();
    _active = true;
    // Initialize to start angle (assume pitch and yaw remain 0).
    _sprite.getRotation(_basePitch, _baseYaw, _baseRoll); 
    _sprite.setRotation(_basePitch, _baseYaw, _baseRoll);
}

void DanceAnimation::update() {
    if (!_active) return;
    uint32_t now = millis();
    if (now < _startTime + _delay) {
        // Still in delay period.
        return;
    }
    uint32_t elapsed = now - (_startTime + _delay);
    if (elapsed >= _duration) {
        // Animation complete.
        _sprite.setRotation(_basePitch, _baseYaw, _baseRoll);
        _active = false;
    } else {
        float t = (float)elapsed / _duration; // linear interpolation factor [0,1]
        float currentAngle = (_endAngle - _startAngle) * sin(t * 2 * PI);
        float currentRot = 180 * sin(t * PI); // do a full circle once or any other degree rotation pi * n times
        _sprite.setRotation(_basePitch, currentAngle, currentRot);
    }
}

//–––––– SelectionAnimation Implementation ––––––

SelectionAnimation::SelectionAnimation(SpriteStack &sprite, float startAngle, float endAngle,
                                     uint32_t duration, uint32_t delay)
  : SpriteStackAnimation(duration, delay),
    _sprite(sprite),
    _startAngle(startAngle),
    _endAngle(endAngle)
{}

void SelectionAnimation::start() {
    _startTime = millis();
    _active = true;
    // Initialize to start angle (assume pitch and yaw remain 0).
    _sprite.getRotation(_basePitch, _baseYaw, _baseRoll); 
    _sprite.setRotation(_basePitch, _baseYaw, _baseRoll);
    _pos = _sprite.getPosition();
}

void SelectionAnimation::update() {
    if (!_active) return;
    uint32_t now = millis();
    if (now < _startTime + _delay) {
        // Still in delay period.
        return;
    }
    uint32_t elapsed = now - (_startTime + _delay);
    if (elapsed >= _duration) {
        // Animation complete.
        _sprite.setRotation(_basePitch, _baseYaw, _baseRoll);
        _sprite.setPosition(_pos.x, _pos.y);       // Reset to original position.
        _sprite.setLayerOffset(1);                 // Reset to the base layer offset (adjust if needed).
        _active = false;
    } else {
        float t = (float)elapsed / _duration; // linear interpolation factor [0,1]
        float currentRot = (_endAngle - _startAngle) * sin(t * PI/2); // do a full circle once or any other degree rotation pi * n times
        _sprite.setRotation(_basePitch, _baseYaw, currentRot);
        _sprite.setPosition(_pos.x, _pos.y - 20*sin(t * PI)); // sine wave between 0 and 10 pixels for height
        _sprite.setLayerOffset(1 + 2*sin(t * PI)); // sine wave between 1 to 4 between layer offsets
    }
}

//–––––– DeselectionAnimation Implementation ––––––

DeselectionAnimation::DeselectionAnimation(SpriteStack &sprite, float startAngle, float endAngle,
                                     uint32_t duration, uint32_t delay)
  : SpriteStackAnimation(duration, delay),
    _sprite(sprite),
    _startAngle(startAngle),
    _endAngle(endAngle)
{}

void DeselectionAnimation::start() {
    _startTime = millis();
    _active = true;
    // Initialize to start angle (assume pitch and yaw remain 0).
    _sprite.getRotation(_basePitch, _baseYaw, _baseRoll); 
    _sprite.setRotation(_basePitch, _baseYaw, _baseRoll);
}

void DeselectionAnimation::update() {
    if (!_active) return;
    uint32_t now = millis();
    if (now < _startTime + _delay) {
        // Still in delay period.
        return;
    }
    uint32_t elapsed = now - (_startTime + _delay);
    if (elapsed >= _duration) {
        // Animation complete.
        _sprite.setRotation(_basePitch, _baseYaw, _baseRoll);
        _active = false;
    } else {
        float t = (float)elapsed / _duration; // linear interpolation factor [0,1]
        float currentAngle = (_endAngle - _startAngle) * sin(t * 2 * PI);
        _sprite.setRotation(_basePitch, currentAngle, _baseRoll);

    }
}

