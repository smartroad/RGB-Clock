#include <EEPROM.h>
#include "FastLED.h"
#include <TimeLib.h>
#include <Wire.h>
#include <DS1307RTC.h>  // a basic DS1307 library that returns time as a time_t
#include <Bounce2.h>

#define LEDSPERSEG 3 // number of leds per segment
#define LEDSPERDIG LEDSPERSEG * 7 // number of leds per digit
#define LEDSPERCOL 2 // number of leds used for colon

#define DATA_PIN 3 // pin neopixels are connected to

#define DIGIT1 0                    // start of digit one
#define DIGIT2 DIGIT1 + LEDSPERDIG  // start of digit two
#define COLON  DIGIT2 + LEDSPERDIG  // start of colon
#define DIGIT3 COLON + LEDSPERCOL   // start of digit 3
#define DIGIT4 DIGIT3 + LEDSPERDIG  // start of digit 4

#define NUM_LEDS DIGIT4 + LEDSPERDIG // total number of LEDs in string

#define HYSTERESIS 15
#define HIGHLEVEL 850
#define MEDIUMLEVEL 650
#define LOWLEVEL 150

// Define the array of leds
CRGB leds[NUM_LEDS];

byte count = 0; // temp testing variable

byte mode = 0; // 1 = display clock; 0 = setclock
#define DISPLAY_CLOCK 0
#define SET_CLOCK 1

byte hourColour = 0;
byte minuteColour = 0;
byte colonColour = 0;

byte numbers[15] = {
// xGFEDCBA
  B01111110,  // 0    Segment Layout
  B00011000,  // 1      CCC
  B01101101,  // 2     B   D
  B00111101,  // 3     B   D
  B00011011,  // 4      AAA
  B00110111,  // 5     G   E
  B01110111,  // 6     G   E
  B00011100,  // 7      FFF
  B01111111,  // 8
  B00111111,  // 9
  B00000100,  // upper dash
  B00001010,  // upper sides
  B00000001,  // middle dash
  B01010000,  // lower sides
  B00100000   // lower dash
};

byte colours[8] = {0, 32, 64, 96, 128, 160, 192, 224}; // red, orange, yellow, green, cyan, blue, purple, magenta

Bounce hourButton = Bounce();
Bounce minuteButton = Bounce();
Bounce setButton = Bounce();
#define hourPin 7
#define minutePin 5
#define setPin 6

byte setMinute;
byte setHour;

byte dst = 0;
byte brightnessSetting = 0;
byte brightnessLevel = 0; 
byte brightnessTarget = 255;

void setup() { 
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  setSyncProvider(RTC.get);   // the function to get the time from the RTC
  setSyncInterval(10);
//  Serial.begin(9600);

  for(int whiteLed = 0; whiteLed < NUM_LEDS; whiteLed = whiteLed + 1) {
    // Turn our current led on to white, then show the leds
    leds[whiteLed] = CRGB::White;
  
    // Show the leds (only one of which is set to white, from above)
    FastLED.show();
  
    // Wait a little bit
    delay(20);
  
    // Turn our current led back to black for the next loop around
    leds[whiteLed] = CRGB::Black;
  }

  leds[NUM_LEDS - 1] = CRGB::Black;
  FastLED.show();

  pinMode(hourPin, INPUT_PULLUP);
  pinMode(minutePin, INPUT_PULLUP);
  pinMode(setPin, INPUT_PULLUP);

  hourButton.attach(hourPin);
  hourButton.interval(5);
  minuteButton.attach(minutePin);
  minuteButton.interval(5);
  setButton.attach(setPin);
  setButton.interval(5);

  hourColour = EEPROM.read(0);
  minuteColour = EEPROM.read(1);
  colonColour = EEPROM.read(2);
  dst = EEPROM.read(3);

  setButton.update();

  if (setButton.read() == 0) {
    delay(1000);

    char lightMode = 0;
    
    while(true) {
      hourButton.update();
      minuteButton.update();

      if (hourButton.rose() == 1) {
        lightMode--;
        if (lightMode < 0) lightMode = 0;
      }
      else if (minuteButton.rose() == 1) {
        lightMode++;
        if (lightMode > 3) lightMode = 3;
      }

      if (lightMode == 3) {
        brightnessTarget=255;
        showDigit(3, 10, CHSV(0,255,255), CHSV(0,0,0), 0);
      }
      else if (lightMode == 2) {
        brightnessTarget=128;
        showDigit(3, 11, CHSV(0,255,255), CHSV(0,0,0), 0);
      }
      else if (lightMode == 1) {
        brightnessTarget=64;
        showDigit(3, 12, CHSV(0,255,255), CHSV(0,0,0), 0);
      }
      else if (lightMode == 0) {
        brightnessTarget=8;
        showDigit(3, 13, CHSV(0,255,255), CHSV(0,0,0), 0);
      }

      FastLED.setBrightness(brightnessTarget);

      int lightTemp = lightLevel();

      showDigit(0, lightTemp % 10,          CHSV(0,255,255), CHSV(0,0,0), 0);
      showDigit(1, (lightTemp / 10) % 10,   CHSV(0,255,255), CHSV(0,0,0), 0);
      showDigit(2, (lightTemp / 100) % 10,  CHSV(0,255,255), CHSV(0,0,0), 0);

      FastLED.show();
      FastLED.delay(50);
    }
  }
  else delay(3000);
}





void loop() {
  hourButton.update();
  minuteButton.update();
  setButton.update();

  byte hourValue = hourButton.read();
  byte minuteValue = minuteButton.read();
  byte setValue = setButton.read();
  static byte hourValueLast = 1;
  static byte minuteValueLast = 1;
  static byte setValueLast = 1;
  static unsigned long setPressed = 0;
  static byte setHeld = 0;
  static byte eepromUpdate = 0;
  static unsigned long eepromDelay = 0;
  static byte dstSet = 0;

  int lightTemp = lightLevel();

  if (lightTemp >= HIGHLEVEL) brightnessTarget=255;
  else if (lightTemp >= MEDIUMLEVEL && lightTemp < HIGHLEVEL - HYSTERESIS) brightnessTarget=128;
  else if (lightTemp >= LOWLEVEL && lightTemp < MEDIUMLEVEL - HYSTERESIS) brightnessTarget=64;
  else if (lightTemp < LOWLEVEL - HYSTERESIS) brightnessTarget=8;

  if (brightnessLevel < brightnessTarget) brightnessLevel++;
  else if (brightnessLevel > brightnessTarget) brightnessLevel--;

  FastLED.setBrightness(brightnessLevel);
  
  if (mode == DISPLAY_CLOCK) {
    showClock();

    if (hourValue == 0 && hourValueLast == 1) { // button pressed
      hourColour ++;
      if (hourColour > 10) hourColour = 0;
      eepromDelay = millis();
      eepromUpdate = true;
    }

    if (minuteValue == 0 && minuteValueLast == 1) { // button pressed
      minuteColour ++;
      if (minuteColour > 10) minuteColour = 0;
      eepromDelay = millis();
      eepromUpdate = true;
    }

    if (setValue == 0 && setValueLast == 1) { // button pressed
      setPressed = millis();
    }
    else if (setValue == 0 && setValueLast == 0) { // button held
      if ((long)(millis() - setPressed >= 2000) && setHeld == 0) { mode = SET_CLOCK; setHeld = 1; setMinute = minute(); setHour = hour();}
    }
    else if (setValue == 1 && setValueLast == 0) {
      if ((long)(millis() - setPressed < 2000)) {
        colonColour ++;
        if (colonColour > 8) colonColour = 0;
        eepromDelay = millis();
        eepromUpdate = true;
      }
    }
    else if (setValue == 1 && setValueLast == 1) {
      setHeld = 0;
      if (eepromUpdate == true && (long)(millis() - eepromDelay > 5000)) {
        EEPROM.write(0, hourColour);
        EEPROM.write(1, minuteColour);
        EEPROM.write(2, colonColour);
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        delay(20);
        eepromUpdate = false;
      }
    }
  }
  
  else if (mode == SET_CLOCK) {
    showDigit(0, setMinute % 10,        CHSV(0,255,255), CHSV(0,0,0), 0);
    showDigit(1, (setMinute / 10) % 10, CHSV(0,255,255), CHSV(0,0,0), 0);
    showDigit(2, setHour % 10,          CHSV(0,255,255), CHSV(0,0,0), 0);
    showDigit(3, (setHour /10) % 10,    CHSV(0,255,255), CHSV(0,0,0), 0);
    setColon(false, CHSV(0, 255, 255));

    static unsigned long hourPressedDown;
    static unsigned long minutePressedDown;

    if (hourValue == 0 && hourValueLast == 1) { // button pressed
      setHour++;
      hourPressedDown = millis();
    }
    else if (hourValue == 0 && hourValueLast == 0) { // button held
      if ((long)(millis() - hourPressedDown) > 500) {
        setHour++;
        hourPressedDown = millis();
      }
    }

    if (minuteValue == 0 && minuteValueLast == 1) { // button pressed
      setMinute++;
      minutePressedDown = millis();
    }
    else if (minuteValue == 0 && minuteValue == 0) { // button held
      if ((long)(millis() - minutePressedDown) > 250) {
        setMinute++;
        minutePressedDown = millis();
      }
    }

    if (setValue == 0 && setValueLast == 1) { // button pressed
      setPressed = millis();
    }
    else if (setValue == 0 && setValueLast == 0) { // button held
      if ((long)(millis() - setPressed >= 2000) && setHeld == 0) { mode = DISPLAY_CLOCK; setHeld = 1; setTime(setHour, setMinute, 0, 1, 1, 2000); RTC.set(now());}
    }
    else if (setValue == 1 && setValueLast == 0) {
      if ((long)(millis() - setPressed < 2000)) {
        dstSet = true;
        if (dstSet == true) {
          if (dst == true) {
            setHour--;
            dst = false;
            fill_solid(leds, NUM_LEDS, CRGB::Black);
            delay(20);
          }
          else {
            setHour++;
            dst = true;
            fill_solid(leds, NUM_LEDS, CRGB::White);
            delay(20);
          }
          dstSet = false;
          EEPROM.write(3, dst);
        }
      }
    }
    else if (setValue == 1 && setValueLast == 1) {
      setHeld = 0;
    }
    if (setHour >= 24) setHour = 0;
    if (setMinute > 59) setMinute = 0;
  }

  hourValueLast = hourValue;
  minuteValueLast = minuteValue;
  setValueLast = setValue;

  FastLED.show();
  FastLED.delay(50);
}





int lightLevel() {
  #define numReadings 10

  static int readings[numReadings];      // the readings from the analog input
  static int readIndex = 0;              // the index of the current reading
  static int total = 0;                         // the running total
  static int average = 0;                       // the average
  
  #define inputPin A0

  // subtract the last reading:
  total = total - readings[readIndex];
  // read from the sensor:
  readings[readIndex] = analogRead(inputPin);
//  Serial.println(readings[readIndex]);
  // add the reading to the total:
  total = total + readings[readIndex];
  // advance to the next position in the array:
  readIndex = readIndex + 1;

  // if we're at the end of the array...
  if (readIndex >= numReadings) {
    // ...wrap around to the beginning:
    readIndex = 0;
  }

  // calculate the average:
  average = total / numReadings;

  return average;
}







void showClock() {
  byte minuteUnit = 0; // variables to hold the individual digits for displaying time.
  byte minuteTens = 1;
  byte hourUnit = 2;
  byte hourTens = 4;

  static byte color = 0; // test variable to change colour
//  static unsigned long secondFlash = 0;
//  static byte lastSecond = 0;

  static int count = 0;

  minuteUnit = minute() % 10;
  minuteTens = (minute() / 10) % 10;
  hourUnit = hour() % 10;
  hourTens = (hour() /10) % 10;


  /*
   * (42,  192, 255) Warm White
   */

  if (minuteColour <= 7) {
    showDigit(0, minuteUnit, CHSV(colours[minuteColour], 255, 255), CHSV(color+128, 255, 0), 0);
    showDigit(1, minuteTens, CHSV(colours[minuteColour], 255, 255), CHSV(color+128, 255, 0), 0);
  }
  else if (minuteColour == 8) {
    showDigit(0, minuteUnit, CHSV(0, 255, 255), CHSV(0, 255, 0), 1);
    showDigit(1, minuteTens, CHSV(128, 255, 255), CHSV(0, 255, 0), 1);
  }
  else if (minuteColour == 9) {
    showDigit(0, minuteUnit, CHSV(0, 255, 255), CHSV(0, 255, 0), 2);
    showDigit(1, minuteTens, CHSV(0, 255, 255), CHSV(0, 255, 0), 2);
  }
  else if (minuteColour == 10) {
    showDigit(0, minuteUnit, CHSV(0, 0, 255), CHSV(0, 255, 0), 0);
    showDigit(1, minuteTens, CHSV(0, 0, 255), CHSV(0, 255, 0), 0);
  }
  
  if (hourColour <= 7) {
    showDigit(2, hourUnit, CHSV(colours[hourColour], 255, 255), CHSV(color+128, 255, 0), 0);
    showDigit(3, hourTens, CHSV(colours[hourColour], 255, 255), CHSV(color+128, 255, 0), 0);
  }
  else if (hourColour == 8) {
    showDigit(2, hourUnit, CHSV(0, 255, 255), CHSV(0, 255, 0), 1);
    showDigit(3, hourTens, CHSV(128, 255, 255), CHSV(0, 255, 0), 1);
  }
  else if (hourColour == 9) {
    showDigit(2, hourUnit, CHSV(0, 255, 255), CHSV(0, 255, 0), 2);
    showDigit(3, hourTens, CHSV(0, 255, 255), CHSV(0, 255, 0), 2);
  }
  else if (hourColour == 10) {
    showDigit(2, hourUnit, CHSV(0, 0, 255), CHSV(0, 255, 0), 0);
    showDigit(3, hourTens, CHSV(0, 0, 255), CHSV(0, 255, 0), 0);
  }


//  if (second() != lastSecond) secondFlash = millis();
//  if ((long)(millis() - secondFlash < 500)) setColon(false, CHSV(0, 255, 255));
//  else {
//    if (colonColour <= 7) setColon(true, CHSV(colours[colonColour], 255, 255));
//    else if (colonColour == 8) setColon(true, CHSV(0, 0, 255));
//  }
//  lastSecond = second();

  if (second() % 2 == 0) setColon(false, CHSV(0, 255, 255));
  else {
    if (colonColour <= 7) setColon(true, CHSV(colours[colonColour], 255, 255));
    else if (colonColour == 8) setColon(true, CHSV(0, 0, 255));
  }

  count++;
  if (count > 9999) count = 0;

}









void showDigit(byte digit, byte number, CHSV onColour, CHSV offColour, byte routine) {
  byte startLED = 0;
  if (digit == 0) startLED = DIGIT1;
  else if (digit == 1) startLED = DIGIT2;
  else if (digit == 2) startLED = DIGIT3;
  else if (digit == 3) startLED = DIGIT4;

  static byte colorStart = 0;
  static unsigned long animationDelay = 0;
  CHSV colortemp = onColour;
  colortemp.h = colortemp.h + colorStart;
 
  for (byte i = 0; i < 7; i++) {
    byte ledpos = i * LEDSPERSEG + startLED;
    if (bitRead(numbers[number], i) == 1) {
      for (byte c = 0; c < LEDSPERSEG; c++) {
        if (routine == 0) leds[ledpos + c] = onColour;
        else if (routine == 1) leds[ledpos + c] = colortemp;
        else if (routine == 2) leds[ledpos + c] = colortemp;
      }
    }
    else if (bitRead(numbers[number], i) == 0) {
      for (byte c = 0; c < LEDSPERSEG; c++) {
        leds[ledpos + c] = offColour;
      }
    }
    if (routine == 1) colortemp.h += 32;
  }

  if ((long)(millis() - animationDelay) >= 100) {
    colorStart++;
    animationDelay += 100;
  }
}

void setColon(bool state, CHSV onColour) {
  for (byte i = 0; i < LEDSPERCOL; i++) {
    if (state == true) leds[i + COLON] = onColour;
    else leds[i + COLON] = CHSV(0,0,0);
  }
}






/*
    ones_column = bytefromrtc % 10;
    tens_column = (bytefromrtc / 10) % 10;
    hundreds_column = (bytefromrtc / 100) % 10;
 */


/*
 * from: https://stackoverflow.com/questions/5590429/calculating-daylight-saving-time-from-only-date
bool IsDst(int day, int month, int dow)
  {
    if (month < 3 || month > 10)  return false; 
    if (month > 3 && month < 10)  return true; 

    byte previousSunday = day - dow;

    if (month == 3) return previousSunday >= 25;
    if (month == 10) return previousSunday < 25;

    return false; // this line never gonna happend
  }
 */











