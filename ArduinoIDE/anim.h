// -----------------------------------------
//         Header guards and prototypes
// -----------------------------------------
#ifndef ANIMATION_FUNCTIONS_H
#define ANIMATION_FUNCTIONS_H

#include "touch.h"
#include <Arduino.h>
//#include <lvgl.h>
#include "sprite_stack.h"
#define USE_ARDUINO_GFX_LIBRARY




/**
 * @brief Base class for animating a SpriteStack property.
 */
class SpriteStackAnimation {
  public:
    /**
     * @param duration Animation duration in milliseconds.
     * @param delay    Optional delay (ms) before the animation begins.
     */
    SpriteStackAnimation(uint32_t duration, uint32_t delay = 0);
    virtual ~SpriteStackAnimation();

    /// Call this to start the animation.
    virtual void start() = 0;
    /// Call this from your loop() to update the animation.
    virtual void update() = 0;
    /// Returns true if the animation is still running.
    bool isActive() const;

  protected:
    uint32_t _duration;   // in ms
    uint32_t _delay;      // in ms (optional delay before starting)
    uint32_t _startTime;  // when the animation (or delay) began
    bool _active;         // true while the animation is running
};

/**
 * @brief Animates the roll angle (rotation) of a SpriteStack.
 *
 * Note: This example assumes that you want to animate only the roll angle
 * (i.e. sprite.setRotation(0, 0, angle)). Adjust if you wish to animate pitch/yaw.
 */
class RotationAnimation : public SpriteStackAnimation {
  public:
    /**
     * @param sprite     The SpriteStack object to animate.
     * @param startAngle The starting roll angle (in degrees).
     * @param endAngle   The ending roll angle (in degrees).
     * @param duration   Duration (ms) of the animation.
     * @param delay      Optional delay (ms) before the animation begins.
     */
    RotationAnimation(SpriteStack &sprite, float startAngle, float endAngle,
                      uint32_t duration, uint32_t delay = 0);
    virtual void start();
    virtual void update();

  private:
    SpriteStack &_sprite;
    float _startAngle;
    float _endAngle;
    float _basePitch, _baseYaw, _baseRoll;
    Point pos;
};

/**
 * @brief Animates the zoom of a SpriteStack.
 *
 * The SpriteStack’s zoom is controlled via setZoom(), where 100 means 100%.
 */
class ZoomAnimation : public SpriteStackAnimation {
  public:
    /**
     * @param sprite    The SpriteStack object to animate.
     * @param startZoom The starting zoom percentage.
     * @param endZoom   The ending zoom percentage.
     * @param duration  Duration (ms) of the animation.
     * @param delay     Optional delay (ms) before the animation begins.
     */
    ZoomAnimation(SpriteStack &sprite, float startZoom, float endZoom,
                  uint32_t duration, uint32_t delay = 0);
    virtual void start();
    virtual void update();

  private:
    SpriteStack &_sprite;
    float _startZoom;
    float _endZoom;
};

/**
 * @brief Animates the position of a SpriteStack.
 */
class PositionAnimation : public SpriteStackAnimation {
  public:
    /**
     * @param sprite   The SpriteStack object to animate.
     * @param startPos The starting position.
     * @param endPos   The ending position.
     * @param duration Duration (ms) of the animation.
     * @param delay    Optional delay (ms) before the animation begins.
     */
    PositionAnimation(SpriteStack &sprite, Point startPos, Point endPos,
                      uint32_t duration, uint32_t delay = 0);
    virtual void start();
    virtual void update();

  private:
    SpriteStack &_sprite;
    Point _startPos;
    Point _endPos;
};

/*
 * @brief Animates the no-no turn of the sprite stack, in this anim the start and end angle determine the range of the head shake
 */
class NoNoAnimation : public SpriteStackAnimation {
  public:
    /**
     * @param sprite     The SpriteStack object to animate.
     * @param startAngle The starting roll angle (in degrees).
     * @param endAngle   The ending roll angle (in degrees).
     * @param duration   Duration (ms) of the animation.
     * @param delay      Optional delay (ms) before the animation begins.
     */
    NoNoAnimation(SpriteStack &sprite, float startAngle, float endAngle,
                      uint32_t duration, uint32_t delay = 0);
    virtual void start();
    virtual void update();

  private:
    SpriteStack &_sprite;
    float _startAngle;
    float _endAngle;
    float _basePitch, _baseYaw, _baseRoll;
};

/*
 * @brief Animates the nod which changes the pitch to down and then back up, between the start and end angle similar to the no-no anim
 */
class NodAnimation : public SpriteStackAnimation {
  public:
    /**
     * @param sprite     The SpriteStack object to animate.
     * @param startAngle The starting pitch angle (in degrees).
     * @param endAngle   The ending pitch angle (in degrees).
     * @param duration   Duration (ms) of the animation.
     * @param delay      Optional delay (ms) before the animation begins.
     */
    NodAnimation(SpriteStack &sprite, float startAngle, float endAngle,
                      uint32_t duration, uint32_t delay = 0);
    virtual void start();
    virtual void update();

  private:
    SpriteStack &_sprite;
    float _startAngle;
    float _endAngle;
    float _basePitch, _baseYaw, _baseRoll;
};

/*
 * @brief Animates the dance which kind of performs a breakdance type movement
 */
class DanceAnimation : public SpriteStackAnimation {
  public:
    /**
     * @param sprite     The SpriteStack object to animate.
     * @param startAngle The starting pitch angle (in degrees).
     * @param endAngle   The ending pitch angle (in degrees).
     * @param duration   Duration (ms) of the animation.
     * @param delay      Optional delay (ms) before the animation begins.
     */
    DanceAnimation(SpriteStack &sprite, float startAngle, float endAngle,
                      uint32_t duration, uint32_t delay = 0);
    virtual void start();
    virtual void update();

  private:
    SpriteStack &_sprite;
    float _startAngle;
    float _endAngle;
    float _basePitch, _baseYaw, _baseRoll;
};

/*
 * @brief Animates the selection animation which raises and lowers the enitre sprite stack while separating hte layers a bit and then putting htem down again while rotating the 
 * sprite stacka full rotation, 
 */
class SelectionAnimation : public SpriteStackAnimation {
  public:
    /**
     * @param sprite     The SpriteStack object to animate.
     * @param startAngle The starting pitch angle (in degrees).
     * @param endAngle   The ending pitch angle (in degrees).
     * @param duration   Duration (ms) of the animation.
     * @param delay      Optional delay (ms) before the animation begins.
     */
    SelectionAnimation(SpriteStack &sprite, float startAngle, float endAngle,
                      uint32_t duration, uint32_t delay = 0);
    virtual void start();
    virtual void update();

  private:
    SpriteStack &_sprite;
    float _startAngle;
    float _endAngle;
    float _basePitch, _baseYaw, _baseRoll;
    Point _pos;
};

/*
 * @brief Animates the deselection animation which just shakes the yaw axis back and forward to show that the item was not selected
 */
class DeselectionAnimation : public SpriteStackAnimation {
  public:
    /**
     * @param sprite     The SpriteStack object to animate.
     * @param startAngle The starting pitch angle (in degrees).
     * @param endAngle   The ending pitch angle (in degrees).
     * @param duration   Duration (ms) of the animation.
     * @param delay      Optional delay (ms) before the animation begins.
     */
    DeselectionAnimation(SpriteStack &sprite, float startAngle, float endAngle,
                      uint32_t duration, uint32_t delay = 0);
    virtual void start();
    virtual void update();

  private:
    SpriteStack &_sprite;
    float _startAngle;
    float _endAngle;
    float _basePitch, _baseYaw, _baseRoll;
};

#endif