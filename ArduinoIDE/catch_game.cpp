#include "catch_game.h"
#include "lv_xiao_round_screen.h"
#include <LSM6DS3.h>
#include <Wire.h>
#include "touch.h"
#include <stdlib.h>
#include <math.h>
#include "background.h"

// --- Game-specific constants ---
#define MAX_FALLING_OBJECTS 8
#define MAX_CAUGHT 4
#define SEQUENCE_LENGTH 4 // Number of unique ingredients to catch

// --- IMU init ---
LSM6DS3 myIMU(I2C_MODE, 0x6A);
float aX, aY, aZ, gX, gY, gZ;

// --- OPTIMIZATION: Object Pooling for Falling Objects ---
struct FallingObjectData {
    SpriteStack* sprite;
    float x, y;
    float speed;
    int type;
    bool active;
};

static FallingObjectData fallingObjects[MAX_FALLING_OBJECTS];
// CORRECTED: The pool is now an array of POINTERS
static SpriteStack* fallingObjectSprites[MAX_FALLING_OBJECTS];

// --- Efficient "Caught Item" Stack Management ---
static SpriteStack* caughtStack = nullptr;
static bool caughtTypes[SEQUENCE_LENGTH + 1];
static int caughtStackCount = 0;

// Falling object spawn timing
static unsigned long lastFallingSpawnTime = 0;
static int fallingSpawnInterval = 1000;

// The LVGL screen for the catching game
static lv_obj_t *gameScreen = NULL;
unsigned long gameStartTime = 0;

//------------------------------------------------------------
// createCatchingGameScreen()
//------------------------------------------------------------
void createCatchingGameScreen(I2C_BM8563 rtc) {
    gameScreen = lv_obj_create(NULL);
    lv_obj_remove_style_all(gameScreen);
    set_gradient_background(gameScreen, rtc);

    myStack.destroy();
    myStack.create(gameScreen);
    g_spritePosition.x = 120;
    g_spritePosition.y = 150;
    myStack.setPosition(g_spritePosition.x, g_spritePosition.y);
    myStack.setRotation(0, 0, 0);
    myStack.setZoom(200);

    // --- CORRECTED: Initialize the falling objects pool ---
    for (int i = 0; i < MAX_FALLING_OBJECTS; i++) {
        // Use 'new' to create each object, calling the proper constructor
        fallingObjectSprites[i] = new SpriteStack(burger_images, 1, 0, 3.0, 1.0, 100.0f);
        fallingObjectSprites[i]->create(gameScreen);
        
        // Link the data struct to the newly created sprite object
        fallingObjects[i].sprite = fallingObjectSprites[i];
        fallingObjects[i].active = false;
        
        if (fallingObjects[i].sprite->getLVGLObject()) {
            lv_obj_add_flag(fallingObjects[i].sprite->getLVGLObject(), LV_OBJ_FLAG_HIDDEN);
        }
    }

    // --- Reset and efficiently create the caughtStack ---
    caughtStackCount = 0;
    for (int t = 0; t <= SEQUENCE_LENGTH; t++) {
        caughtTypes[t] = false;
    }

    if (caughtStack != nullptr) {
        caughtStack->destroy();
        delete caughtStack;
    }

    static const lv_img_dsc_t* initial_burger_layers[SEQUENCE_LENGTH + 2];
    initial_burger_layers[0] = burger_images[0];
    for(int i = 2; i <= SEQUENCE_LENGTH; ++i) {
        initial_burger_layers[i] = burger_images[0]; // Placeholder image
    }
    initial_burger_layers[SEQUENCE_LENGTH + 1] = burger_images[7];

    caughtStack = new SpriteStack(initial_burger_layers, SEQUENCE_LENGTH + 2, 0, 3.0, 4.0, 100.0f);
    caughtStack->create(gameScreen);
    for(int i = 1; i <= SEQUENCE_LENGTH; ++i) {
        caughtStack->setLayerVisibility(i, false);
    }
    caughtStack->setRotation(40, 0, 0);
    caughtStack->setPosition(120, 200);
    caughtStack->update(); 

    lv_scr_load(gameScreen);
    inCatchingGame = true;
    gameStartTime = millis();

    if (myIMU.begin() != 0) {
        Serial.println("IMU Device error");
    }
    Serial.println("Catching game started!");
}

//------------------------------------------------------------
// updateCatchingGame()
// NOTE: This function remains largely the same as the previous optimized version.
// All instances of `fallingObjects[i].sprite->` are already using pointers.
//------------------------------------------------------------
void updateCatchingGame() {
    unsigned long now = millis();
    static unsigned long lastUpdateTime = 0;
    float dtSec = lastUpdateTime ? (now - lastUpdateTime) / 1000.0f : 0;
    lastUpdateTime = now;

    // 1. Update player position
    gY = myIMU.readFloatAccelX();
    float moveSpeed = gY * 120.0f;
    g_spritePosition.x += moveSpeed * dtSec;

    if (g_spritePosition.x < 20) g_spritePosition.x = 20;
    if (g_spritePosition.x > 220) g_spritePosition.x = 220;
    myStack.setPosition(g_spritePosition.x, g_spritePosition.y);

    // 2. Spawn new falling objects
    if (now - lastFallingSpawnTime > (unsigned long)fallingSpawnInterval) {
        for (int i = 0; i < MAX_FALLING_OBJECTS; i++) {
            if (!fallingObjects[i].active) {
                int type_to_spawn = random_int(1, SEQUENCE_LENGTH);

                fallingObjects[i].x = random_int(20, 220);
                fallingObjects[i].y = -20;
                fallingObjects[i].speed = random_int(40, 80);
                fallingObjects[i].type = type_to_spawn;
                fallingObjects[i].active = true;

                SpriteStack* sprite = fallingObjects[i].sprite;
                sprite->setSpriteImage(burger_images[type_to_spawn]);
                sprite->setPosition(fallingObjects[i].x, fallingObjects[i].y);
                if (sprite->getLVGLObject()) {
                    lv_obj_clear_flag(sprite->getLVGLObject(), LV_OBJ_FLAG_HIDDEN);
                }
                
                lastFallingSpawnTime = now;
                fallingSpawnInterval = random_int(2000, 4000);
                break;
            }
        }
    }

    // 3. Update active objects and check collisions
    for (int i = 0; i < MAX_FALLING_OBJECTS; i++) {
        if (fallingObjects[i].active) {
            fallingObjects[i].y += fallingObjects[i].speed * dtSec;
            fallingObjects[i].sprite->setPosition(fallingObjects[i].x, fallingObjects[i].y);
            fallingObjects[i].sprite->update();

            if (fallingObjects[i].y > 240) {
                fallingObjects[i].active = false;
                if(fallingObjects[i].sprite->getLVGLObject()) {
                    lv_obj_add_flag(fallingObjects[i].sprite->getLVGLObject(), LV_OBJ_FLAG_HIDDEN);
                }
                continue;
            }

            int player_w, player_h;
            myStack.getDim(player_w, player_h);
            float player_zoom = myStack.getZoomPercent() / 100.0f;
            int player_half_w = (player_w * player_zoom) / 2;

            int cat_left = g_spritePosition.x - player_half_w;
            int cat_right = g_spritePosition.x + player_half_w;
            int cat_top = g_spritePosition.y - 20;
            int cat_bottom = g_spritePosition.y + 20;

            int obj_left = fallingObjects[i].x;
            int obj_right = fallingObjects[i].x + 20;
            int obj_top = fallingObjects[i].y;
            int obj_bottom = fallingObjects[i].y + 20;

            bool collision = !(cat_right < obj_left || cat_left > obj_right ||
                               cat_bottom < obj_top || cat_top > obj_bottom);

            if (collision) {
                int type = fallingObjects[i].type;
                Serial.print("Caught layer of type: ");
                Serial.println(type);
                if (!caughtTypes[type]) {
                    caughtTypes[type] = true;
                    caughtStack->setLayerImage(type, burger_images[type]);
                    caughtStack->setLayerVisibility(type, true);
                    caughtStackCount++;
                }
                
                fallingObjects[i].active = false;
                if(fallingObjects[i].sprite->getLVGLObject()) {
                    lv_obj_add_flag(fallingObjects[i].sprite->getLVGLObject(), LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
    }
    
    myStack.update();
    if (caughtStack) {
        caughtStack->update();
    }

    if (caughtStackCount >= SEQUENCE_LENGTH) {
        Serial.println("Game complete!");
        cleanupCatchingGame();
        return;
    }

    if (millis() - gameStartTime > 30000) {
        Serial.println("Out of time!");
        cleanupCatchingGame();
        return;
    }
}


// ------------------------------------------------------------
// cleanupCatchingGame()
// ------------------------------------------------------------
static void cleanupCatchingGame() {
    Serial.println("Cleaning up and returning to main screen…");

    // --- CORRECTED: Clean up the object pool ---
    for (int i = 0; i < MAX_FALLING_OBJECTS; i++) {
        if (fallingObjectSprites[i] != nullptr) {
            fallingObjectSprites[i]->destroy(); // Destroy LVGL objects
            delete fallingObjectSprites[i];     // Delete the C++ object
            fallingObjectSprites[i] = nullptr;
        }
        fallingObjects[i].active = false;
    }

    if (caughtStack) {
        caughtStack->destroy();
        delete caughtStack;
        caughtStack = nullptr;
    }
    caughtStackCount = 0;

    myStack.destroy();

    lv_scr_load(mainScreen);
    inCatchingGame = false;

    lv_obj_del(gameScreen);
    gameScreen = nullptr;

    myStack.create(mainScreen);
    myStack.setPosition(g_spritePosition.x, g_spritePosition.y);
    myStack.setZoom(200);
    myStack.update();
}