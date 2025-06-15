#ifndef CATCHING_GAME_H
#define CATCHING_GAME_H

#include <Arduino.h>
#include <lvgl.h>
#include "sprite_stack.h"
#include "I2C_BM8563.h"           // RTC library

// --- Constants & Definitions ---
#define MAX_FALLING_OBJECTS 8
#define MAX_CAUGHT 8

// Structure for a falling object that uses a SpriteStack.
struct FallingObject {
    SpriteStack* sprite;  // Falling object represented by its own SpriteStack
    int x, y;             // Current position (top-left coordinate)
    float speed;          // Falling speed (pixels per update)
    int type;             // Object “type” used for catch order (e.g., 1 to sequenceLength)
    bool active;          // True if object is active/falling
};

// --- Function Declarations ---

// Creates and loads the catching game screen.
void createCatchingGameScreen(I2C_BM8563 rtc);

// Updates game logic (player movement, object falling, collision, etc.).
void updateCatchingGame();


static void cleanupCatchingGame();

// Extern declarations for globals defined in main.ino:
extern Point g_spritePosition;         // Global position for the player sprite
extern SpriteStack myStack;            // The player’s SpriteStack
//extern const lv_img_dsc_t *cat_images[]; // Sprite image array
extern const lv_img_dsc_t *ball_images[]; // Sprite image array
extern lv_obj_t * mainScreen;
extern bool inCatchingGame;
extern int random_int(int min, int max);  // Our random_int function
// Also, our gradient function is declared here.
//void set_gradient_background(lv_obj_t *parent = NULL, I2C_BM8563 rtc);

#endif // CATCHING_GAME_H
