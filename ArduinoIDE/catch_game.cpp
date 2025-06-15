#include "catch_game.h"
#include "lv_xiao_round_screen.h"
#include <LSM6DS3.h>
#include <Wire.h>
#include "touch.h"     // For validate_touch()
#include <stdlib.h>
#include <math.h>
#include "background.h"

// Define some game–specific constants.
#define MAX_FALLING_OBJECTS 4
#define MAX_CAUGHT 4

// IMU init
LSM6DS3 myIMU(I2C_MODE, 0x6A);    // I2C device address 0x6A
float aX, aY, aZ, gX, gY, gZ;

// Array to hold falling objects.
static FallingObject fallingObjects[MAX_FALLING_OBJECTS];

// Instead of an array of caught SpriteStacks, we use a single SpriteStack to hold caught images.
// caughtImages will serve as the sprite set for caughtStack.
static SpriteStack* caughtStack = nullptr;
static const lv_img_dsc_t* caughtImages[MAX_CAUGHT];
static int caughtStackCount = 0;

// Game configuration: one of each type (types 0..sequenceLength-1) must be caught.
static const int sequenceLength = 4;
static bool caughtTypes[sequenceLength];  // Index 0..sequenceLength-1

// Falling object spawn timing.
static unsigned long lastFallingSpawnTime = 0;
static int fallingSpawnInterval = 1000;  // in ms

// The LVGL screen for the catching game.
static lv_obj_t *gameScreen = NULL;

unsigned long gameStartTime = 0;

//------------------------------------------------------------
// createCatchingGameScreen()
//  Creates a new screen for the catching game and initializes the game state.
//------------------------------------------------------------
void createCatchingGameScreen(I2C_BM8563 rtc) {
  // Create a new screen.
  gameScreen = lv_obj_create(NULL);
  lv_obj_remove_style_all(gameScreen);
  // Set a gradient background.
  set_gradient_background(gameScreen, rtc);
  
  // Use the global player SpriteStack and position.
  // First, destroy its old LVGL objects from mainScreen
  myStack.destroy();
  myStack.create(gameScreen);
  g_spritePosition.x = 120;
  g_spritePosition.y = 150;  // Draw the player sprite higher on the display.
  myStack.setPosition(g_spritePosition.x, g_spritePosition.y);
  myStack.setRotation(0, 0, 0);
  myStack.setZoom(150);
  
  // Initialize falling objects.
  for (int i = 0; i < MAX_FALLING_OBJECTS; i++) {
    fallingObjects[i].active = false;
    fallingObjects[i].sprite = NULL;
  }
  
  // Reset caught tracking.
  caughtStackCount = 0;
  for (int t = 0; t < sequenceLength; t++) {
      caughtTypes[t] = false;
  }
  // If a previous caughtStack exists, destroy it.
  if (caughtStack != nullptr) {
      caughtStack->destroy();
      delete caughtStack;
      caughtStack = nullptr;
  }
  
  lv_scr_load(gameScreen);
  inCatchingGame = true;
  gameStartTime = millis();
  
  // Configure the IMU.
  if (myIMU.begin() != 0) {
    Serial.println("Device error");
  } else {
    Serial.println("aX,aY,aZ,gX,gY,gZ");
  }

  Serial.println("Catching game started!");
}


//------------------------------------------------------------
// updateCatchingGame()
//  Updates the catching game each frame (player movement, falling objects, collision).
//------------------------------------------------------------
void updateCatchingGame() {

  gY = myIMU.readFloatAccelX();
  // Map the IMU value to a movement speed.
  float moveSpeed = gY * 10;
  //Serial.println(moveSpeed);
  g_spritePosition.x += moveSpeed;

  if (g_spritePosition.x < 20) g_spritePosition.x = 20;
  if (g_spritePosition.x > 220) g_spritePosition.x = 220;
  myStack.setPosition(g_spritePosition.x, g_spritePosition.y);

  // 2. Spawn new falling objects periodically.
  unsigned long now = millis();
  if(now - lastFallingSpawnTime > (unsigned long)fallingSpawnInterval) {
    for (int i = 0; i < MAX_FALLING_OBJECTS; i++) {
      if (!fallingObjects[i].active) {
        int frame_int = random_int(0, sequenceLength-1); // random type and layer image
        fallingObjects[i].sprite = new SpriteStack(ball_images, 1, frame_int, 3.0, 1.0, 100.0f);
        fallingObjects[i].sprite->create(gameScreen);
        fallingObjects[i].x = random_int(20, 220);
        fallingObjects[i].y = -20;
        fallingObjects[i].sprite->setPosition(fallingObjects[i].x, fallingObjects[i].y);
        fallingObjects[i].speed = random_int(2, 5);
        fallingObjects[i].type = frame_int;
        fallingObjects[i].active = true;
        break;  // Spawn one object per interval.
      }
    }
    fallingSpawnInterval = random_int(2000, 4000);
    lastFallingSpawnTime = now;
  }
  
  // 3. Update falling objects and check for collisions.
  for (int i = 0; i < MAX_FALLING_OBJECTS; i++) {
    if (fallingObjects[i].active) {
      fallingObjects[i].y += fallingObjects[i].speed;
      fallingObjects[i].sprite->setPosition(fallingObjects[i].x, fallingObjects[i].y);
      fallingObjects[i].sprite->update();
      // If the object falls off the bottom, delete it.
      if (fallingObjects[i].y > 240) {
        fallingObjects[i].active = false;
        if(fallingObjects[i].sprite != NULL) {
         fallingObjects[i].sprite->destroy();
         delete fallingObjects[i].sprite;
         fallingObjects[i].sprite = NULL;
        }
        continue;
      }
      
      // Check for collision using bounding boxes.
      int cat_left   = g_spritePosition.x - 20;
      int cat_right  = g_spritePosition.x + 20;
      int cat_top    = g_spritePosition.y - 20;
      int cat_bottom = g_spritePosition.y + 20;
      
      int obj_left   = fallingObjects[i].x;
      int obj_right  = fallingObjects[i].x + 20;
      int obj_top    = fallingObjects[i].y;
      int obj_bottom = fallingObjects[i].y + 20;
      
      bool collision = !(cat_right < obj_left || cat_left > obj_right ||
                         cat_bottom < obj_top || cat_top > obj_bottom);
      
      if (collision) {
        int type = fallingObjects[i].type;
        if (!caughtTypes[type]) {
          Serial.print("Caught new object of type: ");
          Serial.println(type);
          caughtTypes[type] = true;
          
          if (caughtStackCount < MAX_CAUGHT) {
            // Here we assume that ball_images[type-1] corresponds to the image for this type.
            caughtImages[type] = ball_images[type];
            caughtStackCount++;
            
            // Delete the falling object's sprite since we don't need it anymore.
            fallingObjects[i].sprite->destroy();
            delete fallingObjects[i].sprite;
            fallingObjects[i].sprite = NULL;
            
            // Re-create caughtStack so its count matches caughtStackCount. No need to delete when the first one is caught
            if (caughtStack != nullptr && caughtStackCount != 1) {
              caughtStack->destroy();
              delete caughtStack;
              caughtStack = nullptr;
            }
            caughtStack = new SpriteStack(caughtImages, 4, 0, 3.0, 4.0, 100.0f);
            caughtStack->create(lv_scr_act());
            caughtStack->setRotation(40, 0, 0);
            caughtStack->setPosition(120, 200);
            caughtStack->update();
          }
        } else {
          Serial.print("Already caught object of type: ");
          Serial.println(type);
          fallingObjects[i].sprite->destroy();
          delete fallingObjects[i].sprite;
          fallingObjects[i].sprite = NULL;
        }
        fallingObjects[i].active = false;
      }
    }
  }
  myStack.update();
  
  // Check if all required types have been caught.
  if (caughtStackCount >= sequenceLength) {
    Serial.println("Game complete!");
    cleanupCatchingGame(); 
    return;
  }
  
  // Game is 60 seconds long.
  if(millis() - gameStartTime > 60000){ // 5seconds for testing
    Serial.print("Out of time");
    cleanupCatchingGame(); 
    return;
  }
}

// ------------------------------------------------------------
// cleanupCatchingGame()
//   Safely destroys all game resources and returns to mainScreen.
// ------------------------------------------------------------
static void cleanupCatchingGame() {
    Serial.println("Cleaning up Catching Game and returning to main screen...");

    // 1) Destroy any leftover falling-object sprites.
    for (int i = 0; i < MAX_FALLING_OBJECTS; i++) {
        if (fallingObjects[i].sprite != NULL) {
            fallingObjects[i].sprite->destroy();
            delete fallingObjects[i].sprite;
            fallingObjects[i].sprite = NULL;
        }
        fallingObjects[i].active = false;
    }

    // 2) Destroy caughtStack if it exists.
    if (caughtStack != nullptr) {
        caughtStack->destroy();
        delete caughtStack;
        caughtStack = nullptr;
    }
    caughtStackCount = 0;

    // 3) Reset the array that tracks whether each type is caught.
    for (int t = 0; t < sequenceLength; t++) {
        caughtTypes[t] = false;
    }

    // 4) Delete or clean up our gameScreen. 
    //    Using lv_obj_del() ensures it is removed from LVGL's hierarchy.
    if (gameScreen) {
        lv_obj_clean(gameScreen); // remove all children
        lv_obj_del(gameScreen);
        gameScreen = NULL;
    }

    // 5) Load the original mainScreen from your .ino.
    //    (You declared 'lv_obj_t * mainScreen = NULL;' in the .ino)
    if (mainScreen != NULL) {
        //lv_scr_load_anim(mainScreen, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
        myStack.destroy();
        myStack.create(mainScreen); // Re-create the player on the main screen
        lv_scr_load(mainScreen);
    }

    // 6) Mark the game as no longer active.
    inCatchingGame = false;
}

