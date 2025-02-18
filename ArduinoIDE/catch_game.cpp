#include "catch_game.h"
#include "lv_xiao_round_screen.h"
#include <LSM6DS3.h>
#include <Wire.h>
#include "touch.h"     // For validate_touch()
#include <stdlib.h>
#include <math.h>

// Define some game–specific constants.
#define MAX_FALLING_OBJECTS 4
#define MAX_CAUGHT 8

// IMU init
LSM6DS3 myIMU(I2C_MODE, 0x6A);    //I2C device address 0x6A
float aX, aY, aZ, gX, gY, gZ;

// Array to hold falling objects.
static FallingObject fallingObjects[MAX_FALLING_OBJECTS];
// Array to hold caught SpriteStacks.
static SpriteStack* caughtObjects[MAX_CAUGHT];
static int caughtCount = 0;

// Game sequence: player must catch objects in order 1 .. sequenceLength.
static int nextExpected = 1;
static const int sequenceLength = 5;

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
void createCatchingGameScreen() {
  // Create a new screen.
  gameScreen = lv_obj_create(NULL);
  lv_obj_remove_style_all(gameScreen);
  // Call the global gradient function—passing our new screen as parent.
  set_gradient_background(gameScreen);
  
  // Use the global player SpriteStack and position.
  myStack.create(gameScreen);
  g_spritePosition.x = 120;
  g_spritePosition.y = 210;
  myStack.setPosition(g_spritePosition.x, g_spritePosition.y);
  myStack.setZoom(150);
  
  // Initialize falling objects.
  for (int i = 0; i < MAX_FALLING_OBJECTS; i++) {
    fallingObjects[i].active = false;
    fallingObjects[i].sprite = NULL;
  }
  
  caughtCount = 0;
  nextExpected = 1;
  
  lv_scr_load(gameScreen);
  inCatchingGame = true;
  gameStartTime = millis();
  
  //Call .begin() to configure the IMUs
  if (myIMU.begin() != 0) {
    Serial.println("Device error");
  } else {
    Serial.println("aX,aY,aZ,gX,gY,gZ");
  }

  Serial.println("Catching game started!");
}

//------------------------------------------------------------
// update_caught_display()
//  Repositions caught objects (e.g., in a row at the top of the screen).
//------------------------------------------------------------
static void update_caught_display() {
  for (int i = 0; i < caughtCount; i++) {
    int posX = 40;
    int posY = 40 - i;
    caughtObjects[i]->setPosition(posX, posY);
  }
}

//------------------------------------------------------------
// updateCatchingGame()
//  Updates the catching game each frame (player movement, falling objects, collision).
//------------------------------------------------------------
void updateCatchingGame() {
  // 1. Update the player’s horizontal position using touch input.
  /*lv_coord_t touchX, touchY;
  if (validate_touch(&touchX, &touchY)) {
    if (touchY > 150) {  // Only respond to touches in the lower part of the screen.
      g_spritePosition.x = touchX;
      if (g_spritePosition.x < 20) g_spritePosition.x = 20;
      if (g_spritePosition.x > 220) g_spritePosition.x = 220;
      myStack.setPosition(g_spritePosition.x, g_spritePosition.y);
    }
  }
  */
  Serial.print(myIMU.readFloatAccelX(), 3);
  Serial.print(',');
  Serial.print(myIMU.readFloatAccelY(), 3);
  Serial.print(',');
  Serial.print(myIMU.readFloatAccelZ(), 3);
  Serial.print(',');
  Serial.print(myIMU.readFloatGyroX(), 3);
  Serial.print(',');
  Serial.print(myIMU.readFloatGyroY(), 3);
  Serial.print(',');
  Serial.print(myIMU.readFloatGyroZ(), 3);
  Serial.println();

  gY = myIMU.readFloatAccelX();
  // Map the angle from -30 to +30 degrees to a movement speed between -5 and 5 pixels/frame.
  // Adjust these values as needed.
  float moveSpeed = gY * 10; //map(gY, -1, 1, -10, 10); //gY * 5;//((gY*2*3.14 + 30.0f) / 60.0f) * 10.0f - 5.0f;
  Serial.println(moveSpeed);
  g_spritePosition.x += moveSpeed;

  if (g_spritePosition.x < 20) g_spritePosition.x = 20;
  if (g_spritePosition.x > 220) g_spritePosition.x = 220;
  myStack.setPosition(g_spritePosition.x, g_spritePosition.y);

  
  // 2. Spawn new falling objects periodically.
  unsigned long now = millis();
  if(now - lastFallingSpawnTime > (unsigned long)fallingSpawnInterval) {
    for (int i = 0; i < MAX_FALLING_OBJECTS; i++) {
      if (!fallingObjects[i].active) {
        fallingObjects[i].sprite = new SpriteStack(cat_images, 1, i, 3.0, 1.0, 100.0f);
        fallingObjects[i].sprite->create(lv_scr_act());
        fallingObjects[i].x = random_int(20, 220);
        fallingObjects[i].y = -20;
        fallingObjects[i].sprite->setPosition(fallingObjects[i].x, fallingObjects[i].y);
        fallingObjects[i].speed = random_int(2, 5);
        fallingObjects[i].type = random_int(1, sequenceLength);
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
      
      // If object falls off bottom, delete it.
      if (fallingObjects[i].y > 240) {
        fallingObjects[i].active = false;
        if(fallingObjects[i].sprite != NULL) {
         fallingObjects[i].sprite->destroy();
         delete fallingObjects[i].sprite;
         fallingObjects[i].sprite = NULL;
        }
        continue;
      }
      
      // Approximate collision using bounding boxes.
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
        if (fallingObjects[i].type == nextExpected) {
          Serial.print("Caught correct object of type: ");
          Serial.println(fallingObjects[i].type);
          nextExpected++;
          if (nextExpected > sequenceLength) {
            Serial.println("Game complete!");
            inCatchingGame = false;
            // (Optionally add win animations here.)
          }
          if (caughtCount < MAX_CAUGHT) {
            caughtObjects[caughtCount++] = fallingObjects[i].sprite;
            update_caught_display();
          } else {
            fallingObjects[i].sprite->destroy();
            delete fallingObjects[i].sprite;
          }
        } else {
          Serial.print("Caught wrong object of type: ");
          Serial.println(fallingObjects[i].type);
          fallingObjects[i].sprite->destroy();
          delete fallingObjects[i].sprite;
        }
        fallingObjects[i].active = false;
        fallingObjects[i].sprite = NULL;
      }
    }
  }
  if(millis() - gameStartTime > 20000){
    Serial.print("out of time");
    inCatchingGame = false;
  }
}
