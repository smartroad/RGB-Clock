#include <EEPROM.h>
#include "FastLED.h"
#include <TimeLib.h>
#include <Wire.h>
#include <DS1307RTC.h>  // a basic DS1307 library that returns time as a time_t
#include <Bounce2.h>

/*
 * Constants used throughout program
 */
 
/*  
 *   Change these 2 depending on number of LEDs on the strip for each segment of a digit
 *   and the number of LEDs used in the colon seperator
 */
#define LEDSPERSEG 3 // number of leds per segment
#define LEDSPERCOL 2 // number of leds used for colon

/*
 * Change the following depending on which pins they are connected to
 */
#define DATA_PIN 3 // pin neopixels are connected to
#define hourPin 7 // Hour adjustment button
#define minutePin 5 // Minute adjustment button
#define setPin 6 // Set button

/*
 * Change these depending on the light level that works for you and your LDR/Resistor divder.
 * Holding the set button  down when applying power will put the clock into a mode where it displays
 * the ADC value for the current light level. In that mode the hour and minute buttons can be used
 * to change between the different light levels - HIGH - MED-HIGH - MED-LOW - LOW - which level you
 * are on is displayed in the first digit as either the top segment (HIGH), both upper vertical
 * segments (MED-HIGH), middle segment (MED-LOW) and both bottom vertical segments (LOW).
 */
#define HYSTERESIS 15 // variance before changing light level
#define HIGHLEVEL 850 // threshold to go full brightness
#define MEDIUMLEVEL 650 // threshold to change from low level or from high level
#define LOWLEVEL 150 // threashold to change down to lowest brightness

/*
 * most of the following do not need to be changed except the numbers[] array if your segment layout
 * is not the same as displayed below.
 */
#define LEDSPERDIG LEDSPERSEG * 7 // number of leds per digit

#define DIGIT1 0                    // start of digit one
#define DIGIT2 DIGIT1 + LEDSPERDIG  // start of digit two
#define COLON  DIGIT2 + LEDSPERDIG  // start of colon
#define DIGIT3 COLON + LEDSPERCOL   // start of digit 3
#define DIGIT4 DIGIT3 + LEDSPERDIG  // start of digit 4

#define NUM_LEDS DIGIT4 + LEDSPERDIG // total number of LEDs in string

/*
 * mode numbers for changing into different display/setting modes
 */
#define DISPLAY_CLOCK 0
#define SET_CLOCK 1
#define SET_DISPLAYTIME 2


uint8_t numbers[22] = {
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
  B00000100,  // 10 upper dash
  B00001010,  // 11 upper sides
  B00000001,  // 12 middle dash
  B01010000,  // 13 lower sides
  B00100000,  // 14 lower dash
// xGFEDCBA
  B01011110,  // 15 A
  B01001111,  // 16 P
  B01110001,  // 17 o
  B01010001,  // 18 n
  B01000111,  // 19 F
  B01100011,  // 20 t
  B01110000   // 21 u
};

uint8_t colours[8] = {0, 32, 64, 96, 128, 160, 192, 224}; // hues for colours: red, orange, yellow, green, cyan, blue, purple, magenta




// Define the array of leds
CRGB leds[NUM_LEDS];



byte mode = DISPLAY_CLOCK; // holds current mode
unsigned long modeChange;  // time mode was changed



byte hourColour = 0;
byte minuteColour = 0;
byte colonColour = 0;



Bounce hourButton = Bounce();
Bounce minuteButton = Bounce();
Bounce setButton = Bounce();



byte setMinute;
byte setHour;
byte onHour, offHour; // time display should turn on and off



byte dst = 0;
byte brightnessSetting = 0;
byte brightnessLevel = 0; 
byte brightnessTarget = 255;



byte displayOverride = 2;



void setup() { 
//  Serial.begin(9600);
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  setColon(1, CHSV(0,255,64));
  FastLED.show();

  delay(3000);
  
  setSyncProvider(RTC.get);   // the function to get the time from the RTC
  setSyncInterval(10);

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
  offHour = EEPROM.read(4);
  onHour = EEPROM.read(5);
  displayOverride = EEPROM.read(6);

  

  setButton.update();

  /*
   * Test mode for calibrating brightness levels. Display will show the raw output of the light sensor.
   * First digit shows current brightness levels - see characters 10 - 13 with 10 being brightest.
   */
   
  if (setButton.read() == 0) { // Test mode for calibrating brightness thresholds

    char lightMode = 3;
    
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
  
  /*
   * test mode for checking led operation - flashes through all primary colours for whole display
   */
   
  else if (hourButton.read() == 0) {
    byte count = 0;
    while(true) {
      fill_solid(leds, NUM_LEDS, CRGB::Red);
      FastLED.show();
      delay(5000);
      fill_solid(leds, NUM_LEDS, CRGB::Green);
      FastLED.show();
      delay(5000);
      fill_solid(leds, NUM_LEDS, CRGB::Blue);
      FastLED.show();
      delay(5000);
    }
  }
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
  static byte onoff = 0;

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
    static unsigned long hourPressedDown;
    static unsigned long minutePressedDown;
    static unsigned long dstSetTime;

    showDigit(0, setMinute % 10,        CHSV(0,255,255), CHSV(0,0,0), 0);
    showDigit(1, (setMinute / 10) % 10, CHSV(0,255,255), CHSV(0,0,0), 0);
    showDigit(2, setHour % 10,          CHSV(0,255,255), CHSV(0,0,0), 0);
    showDigit(3, (setHour /10) % 10,    CHSV(0,255,255), CHSV(0,0,0), 0);
    setColon(false, CHSV(0, 255, 255));

    if ((long)(millis() - dstSetTime) < 1000) {
      if (dst == true) { // display on for 1 second
        showDigit(0, 18, CHSV(128,255,255), CHSV(0,0,0), 0);
        showDigit(1, 17, CHSV(128,255,255), CHSV(0,0,0), 0);
      }
      else { // display of(f) for 1 second
        showDigit(0, 19, CHSV(128,255,255), CHSV(0,0,0), 0);
        showDigit(1, 17, CHSV(128,255,255), CHSV(0,0,0), 0);
      }
    }

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
      if ((long)(millis() - setPressed >= 2000) && setHeld == 0) { mode = SET_DISPLAYTIME; modeChange = millis(); setHeld = 1; setTime(setHour, setMinute, 0, 1, 1, 2000); RTC.set(now());}
    }
    else if (setValue == 1 && setValueLast == 0) {
      if ((long)(millis() - setPressed < 2000)) {
        dstSet = true;
        dstSetTime = millis();
        if (dstSet == true) {
          if (dst == true) {
            setHour--;
            dst = false;
          }
          else {
            setHour++;
            dst = true;
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
  else if (mode == SET_DISPLAYTIME) {
    static unsigned long hourPressedDown;
    static unsigned long minutePressedDown;

    if (onoff == 0) {
      showDigit(0, onHour % 10,        CHSV(160,255,255), CHSV(0,0,0), 0);
      showDigit(1, (onHour / 10) % 10, CHSV(160,255,255), CHSV(0,0,0), 0);
      showDigit(2, 18, CHSV(96,255,255), CHSV(0,0,0), 0);
      showDigit(3, 17, CHSV(96,255,255), CHSV(0,0,0), 0);
    }
    else {
      showDigit(0, offHour % 10,        CHSV(160,255,255), CHSV(0,0,0), 0);
      showDigit(1, (offHour / 10) % 10, CHSV(160,255,255), CHSV(0,0,0), 0);
      showDigit(2, 19, CHSV(0,255,255), CHSV(0,0,0), 0);
      showDigit(3, 17, CHSV(0,255,255), CHSV(0,0,0), 0);
    }
    setColon(false, CHSV(0, 255, 255));

    if (hourValue == 0 && hourValueLast == 1) { // button pressed
      if (onoff == 0) onHour++;
      else offHour++;
      hourPressedDown = millis();
    }
    else if (hourValue == 0 && hourValueLast == 0) { // button held
      if ((long)(millis() - hourPressedDown) > 250) {
        if (onoff == 0) onHour++;
        else offHour++;
        hourPressedDown = millis();
      }
    }

    if (minuteValue == 0 && minuteValueLast == 1) { // button pressed
      displayOverride++;
      if (displayOverride > 3) displayOverride = 0;
      minutePressedDown = millis();
      
    }
//    else if (minuteValue == 0 && minuteValue == 0) { // button held
//      
//    }
    if ((long)(millis() - minutePressedDown) < 1500) {
      showDigit(1, 20, CHSV(96,255,255), CHSV(0,0,0), 0);
      showDigit(2, 21, CHSV(96,255,255), CHSV(0,0,0), 0);
      showDigit(3, 15, CHSV(96,255,255), CHSV(0,0,0), 0);
      
      if      (displayOverride == 0) showDigit(0, 0, CHSV(192,255,255), CHSV(0,0,0), 0);
      else if (displayOverride == 1) showDigit(0, 1, CHSV(192,255,255), CHSV(0,0,0), 0);
      else if (displayOverride == 2) showDigit(0, 2, CHSV(192,255,255), CHSV(0,0,0), 0);
      else                           showDigit(0, 3, CHSV(192,255,255), CHSV(0,0,0), 0);
      
      FastLED.show();
    }

    if (setValue == 0 && setValueLast == 1) { // button pressed
      setPressed = millis();
    }
    else if (setValue == 0 && setValueLast == 0) { // button held
      if ((long)(millis() - setPressed >= 2000) && setHeld == 0) { mode = DISPLAY_CLOCK; setHeld = 1; EEPROM.write(4, offHour); EEPROM.write(5, onHour); EEPROM.write(6, displayOverride);}
    }
    else if (setValue == 1 && setValueLast == 0) {
      if ((long)(millis() - setPressed < 2000)) {
        onoff = 1 - onoff;
      }
    }
    else if (setValue == 1 && setValueLast == 1) {
      setHeld = 0;
    }
    if (onHour >= 24) onHour = 0;
    if (offHour >= 24) offHour = 0;
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
  byte OFFHour;
  byte displayOn;
  static byte displayOverrideOld;

  static byte color = 0; // test variable to change colour
  
  minuteUnit = minute() % 10;
  minuteTens = (minute() / 10) % 10;
  hourUnit = hour() % 10;
  hourTens = (hour() /10) % 10;

  if (offHour == 0) OFFHour = 24;
  else OFFHour = offHour;

  if (OFFHour > onHour) {
    displayOn = hour() >= onHour && hour() < OFFHour;
  }
  else if (onHour > OFFHour) {
    displayOn = !(hour() >= OFFHour && hour() < onHour);
  }
  else if (onHour == offHour) {
    displayOn = true;
  }

  if (!displayOn) {
    if (displayOverride == 1) {
      if (displayOverrideOld) displayOn = lightLevel() > LOWLEVEL - HYSTERESIS;
      else displayOn = lightLevel() > LOWLEVEL;
    }
    else if (displayOverride == 2) {
      if (displayOverrideOld) displayOn = lightLevel() > MEDIUMLEVEL - HYSTERESIS;
      else displayOn = lightLevel() > MEDIUMLEVEL;
    }
    else if (displayOverride == 3) {
      if (displayOverrideOld) displayOn = lightLevel() > HIGHLEVEL - HYSTERESIS;
      else displayOn = lightLevel() > HIGHLEVEL;
    }
    displayOverrideOld = displayOn;
  }
  

  if (displayOn) {
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
  }
  else {fill_solid(leds, NUM_LEDS, CRGB::Black);}

  if (second() % 2 == 0) setColon(false, CHSV(0, 255, 255));
  else {
    if (colonColour <= 7) setColon(true, CHSV(colours[colonColour], 255, 255));
    else if (colonColour == 8) setColon(true, CHSV(0, 0, 255));
  }
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












