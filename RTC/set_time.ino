#include "I2C_BM8563.h"

I2C_BM8563 rtc(I2C_BM8563_DEFAULT_ADDRESS, Wire);

void setup() {
  // Init Serial
  Serial.begin(115200);
  while(!Serial);
  delay(50);
  Serial.println("RTC time calibration started!");
  // Init I2C
  Wire.begin();

  // Init RTC
  rtc.begin();

  // Set RTC Date
  I2C_BM8563_DateTypeDef dateStruct;
  dateStruct.weekDay = 2;
  dateStruct.month = 2;
  dateStruct.date = 4;
  dateStruct.year = 2025;
  rtc.setDate(&dateStruct);

  // Set RTC Time
  I2C_BM8563_TimeTypeDef timeStruct;
  timeStruct.hours   = 10;
  timeStruct.minutes = 42;
  timeStruct.seconds = 0;
  rtc.setTime(&timeStruct);

  Serial.println("RTC time calibration complete!");
}

void loop() {

}
