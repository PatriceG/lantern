

/*
 Name:		Lanterne.ino
 Created:	01/01/2017
 Author:	Patrice GODARD 
 Implements 5 light modes - fixed color with adujstable brightness - fixed brightness with adjustable color - fixed 'rainbow' with adjustable brightness - slowly rotating 'rainbow' with adjustable brightness - fire simulation
 Light modes are selected either by a 6 position rotary switch (USE_ENCODER_SWITCH UNDEFINED) or by the rotary encoder's push button switch (USE_ENCODER_SWITCH DEFINED)
*/
#define ENCODER_USE_INTERRUPTS
#include "Encoder.h"
#include "FastLED.h"
#include <EEPROM.h>
#include <PinChangeInterrupt.h>

FASTLED_USING_NAMESPACE

//define the following USE_ENCODER_SWICH to use the encoder's builtin push button switch
//otherwise we use a 6 pole rotary switch to select the lantern light mode
//#define USE_ENCODER_SWITCH 

#define ONBOARD_LED_PIN 11
//neopixel ring settings
#define LED_PIN 9

#ifdef USE_ENCODER_SWITCH
#define LED_COUNT 12 //small lantern
#else
#define LED_COUNT 16 //big lantern
#endif

//mode selection switches pins
#define MODE1_PIN 7
#define MODE2_PIN 6
#define MODE3_PIN 5
#define MODE4_PIN 4
#define MODE5_PIN A1
#define ENCODER_SWITCH_PIN A5

#define DEFAULT_BRIGHTNESS 20
#define FRAMES_PER_SECOND  120 //refresh rate

#define BRIGHTNESS_STEP 2 //multiplier for brigness settings

//EEProm addresses used to remember settings
#define EEPROM_BRIGHTNESS_ADDR 0
#define EEPROM_COLOR_ADDR 2
#define EEPROM_MODE_ADDR 4
#define EEPROM_COLOR_HUE_ADDR 6
#define EEPROM_COLOR_SAT_ADDR 8
#define EEPROM_COLOR_VAL_ADDR 10

CRGB leds[LED_COUNT]; //led array

uint8_t mode = 1;
uint8_t oldMode = -1;
int16_t brightness=DEFAULT_BRIGHTNESS;

int16_t colorWheelPos = 0;
uint8_t lastHue = 100;
uint8_t lastSat = 255;
uint8_t lastVal = 255;
uint8_t hue = 0; //rotating base color used by rainbow effect

//encoder init, using INT0 and INT1 
#ifdef USE_ENCODER_SWITCH
Encoder encoder(2, 3);
#else
Encoder encoder(3, 2);
#endif

long pos  = 0; //encoder position
volatile uint8_t interrupted = false; //to force exit of fire simulation when changing light mode

/* Array of effect functions */
typedef void (*fnct)();
void effect_fire();
void effect_fade_inout_slow();
void effect_wind();
fnct effect[] = {effect_fire, effect_fade_inout_slow, effect_fire, effect_wind, effect_fire, effect_fire, effect_fire,effect_wind}; 


void setup() { 
	FastLED.addLeds<WS2811,LED_PIN,GRB>(leds, LED_COUNT).setCorrection(TypicalLEDStrip);
  
	//read brightness from EEPROM

  EEPROM.get(EEPROM_BRIGHTNESS_ADDR,brightness) ;
	//read color from EEPROM
  EEPROM.get(EEPROM_COLOR_ADDR,colorWheelPos);
 
  EEPROM.get(EEPROM_COLOR_HUE_ADDR,lastHue);
  EEPROM.get(EEPROM_COLOR_SAT_ADDR,lastSat);
  EEPROM.get(EEPROM_COLOR_VAL_ADDR,lastVal);
  //read mode from EEPROM
  EEPROM.get(EEPROM_MODE_ADDR,mode);
 
  FastLED.setBrightness(brightness);
	pinMode(LED_PIN, OUTPUT);	
	pinMode(MODE1_PIN, INPUT_PULLUP);
	pinMode(MODE2_PIN, INPUT_PULLUP);
	pinMode(MODE3_PIN, INPUT_PULLUP);
	pinMode(MODE4_PIN, INPUT_PULLUP);
	pinMode(MODE5_PIN, INPUT_PULLUP);
 	pinMode(ONBOARD_LED_PIN,OUTPUT);

#ifdef USE_ENCODER_SWITCH
  pinMode(ENCODER_SWITCH_PIN,INPUT_PULLUP);
  attachPCINT(digitalPinToPCINT(ENCODER_SWITCH_PIN), modeChanged, FALLING);
#else
  attachPCINT(digitalPinToPCINT(MODE5_PIN), modeChanged, RISING);
#endif  

  CRGB col = CHSV(lastHue,lastSat,lastVal);
  fill_solid(leds,LED_COUNT,col);
}

/*
 * Called by an interrupt when encoder button is pushed
 */
void modeChanged(){
  interrupted = true;
}

void loop() {
  //lamp logic
  mode = getMode(mode);
  handleModes(mode,oldMode);
  if(mode != oldMode){   
    oldMode = mode;
  }
    
  //encoder mgmt
  long newPos = encoder.read();
  analogWrite(ONBOARD_LED_PIN,newPos);
  if (newPos != pos) {
    pos = newPos;
    switch(mode){
      case 1:
      case 3:
      case 4:
      //case 5:
        brightness = pos*BRIGHTNESS_STEP;
        if(brightness < 0){
          brightness = 0;
          encoder.write(brightness);
        }
        if(brightness > 255){
          brightness = 255;
          encoder.write((int)(brightness/BRIGHTNESS_STEP));
        }
        FastLED.setBrightness(brightness);
        EEPROM.put(EEPROM_BRIGHTNESS_ADDR, brightness);
        break;

      case 2:      
        colorWheelPos = pos;
        if(colorWheelPos < 0){
          colorWheelPos = 0;
          encoder.write(colorWheelPos);
        }
        if(colorWheelPos > 255){
          colorWheelPos = 255;
          encoder.write(colorWheelPos);
        }        
        //0 is white then gradient to red-orange and following the colorwheel with fully saturated colors
        if(colorWheelPos < 20){ 
          lastHue = 0;
          lastSat = colorWheelPos * 12;
          lastVal = 255;          
        }else{
          lastHue = colorWheelPos;
          lastSat = 255;
          lastVal = 255;
        }
        fill_solid(leds,LED_COUNT,CHSV(lastHue,lastSat,lastVal));
        EEPROM.put(EEPROM_COLOR_ADDR, colorWheelPos);
        EEPROM.put(EEPROM_COLOR_HUE_ADDR, lastHue);
        EEPROM.put(EEPROM_COLOR_SAT_ADDR, lastSat);
        EEPROM.put(EEPROM_COLOR_VAL_ADDR, lastVal);        
        break;
    }
  }
 
  //update leds
  // send the 'leds' array out to the actual LED strip
  FastLED.show();  
  // insert a delay to keep the framerate modest
  FastLED.delay(1000/FRAMES_PER_SECOND); 
}



/*
* Return current mode ID 
*/
#ifdef USE_ENCODER_SWITCH
int getMode(int m) {
  static unsigned long t0 = 0;
  static unsigned long t1 = 0;
  if(!digitalRead(ENCODER_SWITCH_PIN) && t0==0){
    t0 = millis();
  }else if(digitalRead(ENCODER_SWITCH_PIN)){
    t1 = millis();
    if(t0 != 0){
    unsigned long delta = t1 - t0;
    if(delta > 250 && delta < 750){
      t0 = 0;
      m++;
      if(m > 5){
        m = 1;
      }
        EEPROM.put(EEPROM_MODE_ADDR, m);
    }else if(delta > 750){
      t0 = 0;
      m--;
      if(m < 1){
        m = 5;
      }
      EEPROM.put(EEPROM_MODE_ADDR, m);
    }
  }
  }
  return m;
}
#else
int getMode(int m) {
	if (!digitalRead(MODE1_PIN)) m = 1;
	if (!digitalRead(MODE2_PIN)) m = 2;
	if (!digitalRead(MODE3_PIN)) m = 3;
	if (!digitalRead(MODE4_PIN)) m = 4;
	if (!digitalRead(MODE5_PIN)) m = 5;

	return m;
}
#endif

void handleModes(uint8_t mode,uint8_t oldMode) {
	switch(mode) {
		case 1: //static color and brightness is adjustable via encoder
			if(mode != oldMode){
        encoder.write(brightness);
        CRGB col = CHSV(lastHue,lastSat,lastVal);
        fill_solid(leds,LED_COUNT,col);
			}
			break;
		case 2: //static brightness and color is adjustable via encoder			
      if(mode != oldMode){
        encoder.write(colorWheelPos);
        CRGB col = CHSV(lastHue,lastSat,lastVal);
        fill_solid(leds,LED_COUNT,col);
      }      
			break;
		case 3: //rainbow and brightness is adjustable via encoder
      if(mode != oldMode){
        encoder.write(brightness);        
        fill_rainbow(leds,LED_COUNT,0,floor(255/LED_COUNT));
      }
			break;
		case 5: //candle simulation mode and brightness is adjustable via encoder	
      if(mode != oldMode){
        FastLED.setBrightness(255);	
        interrupted = false;
      }
      effect[random(7)]();  
      randomDelay(random(50, 200));
  		break;
		case 4: //cycling rainbow and brightness is adjustable via encoder			
      if(mode != oldMode){
        encoder.write(brightness);     
        hue = 0;   
      }
      fill_rainbow(leds,LED_COUNT,hue,floor(200/LED_COUNT));      
      hue++;
      delay(70);
			break;
	}
}

//========================================================================================================
//candle simulator borrowed from https://skyduino.wordpress.com/2011/11/10/arduino-bougie-virtuelle-a-led/

#define R1_PIN 0
#define R2_PIN 2
#define R3_PIN 4
#define J1_PIN 1
#define J2_PIN 3
#define J3_PIN 5

void lightUp(uint8_t pinOffset, uint8_t value){
  for(int i=0;i<4;i++){
    uint8_t idx = pinOffset+6*i;
    if(idx < LED_COUNT){    
      if(pinOffset % 2 == 0){
         leds[idx] = CRGB(80*value/255,35*value/255,0);
      }else{
        leds[idx] = CRGB(180*value/255,15*value/255,0);
        }
    }
    }
}

void effect_flickering()
{
  for(byte i = 1; i < random(5); i++)
  {
    lightUp(R1_PIN, random(120) + 135);
    lightUp(R2_PIN, random(120) + 135);
    lightUp(R3_PIN, random(120) + 135);
    lightUp(J1_PIN, random(120) + 135);
    lightUp(J2_PIN, random(120) + 135);
    lightUp(J3_PIN, random(120) + 135);
    FastLED.show();  
    if(interrupted) break;
    delay(i);
  }
}
 
 
void effect_moving()
{
  for(byte i = 0; i < random(5); i++)
  {
    lightUp(R1_PIN, random(120 + i));
    lightUp(R2_PIN, random(120 + i) + 65);
    lightUp(R3_PIN, random(120 + i) + 135);
    lightUp(J1_PIN, random(120));
    lightUp(J2_PIN, random(120) + 65);
    lightUp(J3_PIN, random(120) + 135);
    FastLED.show();  
    if(interrupted) break;
    delay(random(200));
  }
}
 
 
void effect_fade_in_slow()
{
  for(byte i = 0; i < 200; i++)
  {
    lightUp(R1_PIN, random(i) + 30);
    lightUp(R2_PIN, random(i) + 30);
    lightUp(R3_PIN, random(i) + 30);
    lightUp(J1_PIN, random(i) + 30);
    lightUp(J2_PIN, random(i) + 30);
    lightUp(J3_PIN, random(i) + 30);
    FastLED.show();  
    if(interrupted) break;
    delay(random(200));
  }
}
 
void effect_fade_inout_slow()
{
  for(byte i = 200; i > 0; i--)
  {
    lightUp(R1_PIN, random(i) + 30);
    lightUp(R2_PIN, random(i) + 30);
    lightUp(R3_PIN, random(i) + 30);
    lightUp(J1_PIN, random(i) + 30);
    lightUp(J2_PIN, random(i) + 30);
    lightUp(J3_PIN, random(i) + 30);
    FastLED.show();  
    if(interrupted) break;
    delay(random(100));
  }
  for(byte i = 0; i < 200; i++)
  {
    lightUp(R1_PIN, random(i) + 30);
    lightUp(R2_PIN, random(i) + 30);
    lightUp(R3_PIN, random(i) + 30);
    lightUp(J1_PIN, random(i) + 30);
    lightUp(J2_PIN, random(i) + 30);
    lightUp(J3_PIN, random(i) + 30);
    FastLED.show();  
    if(interrupted) break;
    delay(random(100));
  }
}
 
void effect_fade_inout_fast()
{
  for(byte i = 200; i > 0; i--)
  {
    lightUp(R1_PIN, random(i) + 30);
    lightUp(R2_PIN, random(i) + 30);
    lightUp(R3_PIN, random(i) + 30);
    lightUp(J1_PIN, random(i) + 30);
    lightUp(J2_PIN, random(i) + 30);
    lightUp(J3_PIN, random(i) + 30);
    FastLED.show();  
    if(interrupted) break;
    delay(random(25));
  }
  for(byte i = 0; i < 200; i++)
  {
    lightUp(R1_PIN, random(i) + 30);
    lightUp(R2_PIN, random(i) + 30);
    lightUp(R3_PIN, random(i) + 30);
    lightUp(J1_PIN, random(i) + 30);
    lightUp(J2_PIN, random(i) + 30);
    lightUp(J3_PIN, random(i) + 30);
    FastLED.show();  
    if(interrupted) break;
    delay(random(25));
  }
}
 
/*
 * Effect of wind boosting fire
 */
void effect_wind()
{
  for(byte i = 0; i < random(20); i++)
  {
    lightUp(R1_PIN, random(120 + i));
    lightUp(R2_PIN, random(120 + i) + 65);
    lightUp(R3_PIN, random(120 + i) + 135);
    lightUp(J1_PIN, random(120));
    lightUp(J2_PIN, random(120) + 65);
    lightUp(J3_PIN, random(120) + 135);
    FastLED.show();  
    if(interrupted) break;
    delay(random(100));
  }
}
 
/*
 * Fire Effect
 */
void effect_fire()
{
  lightUp(R1_PIN, random(120) + 135);
  lightUp(R2_PIN, random(120) + 135);
  lightUp(R3_PIN, random(120) + 135);
  lightUp(J1_PIN, random(120) + 135);
  lightUp(J2_PIN, random(120) + 135);
  lightUp(J3_PIN, random(120) + 135);
  FastLED.show();  
  delay(random(100));
}
 
/*
 * Take Fire Effect
 *
 * The fire begin by fading slowly, before moving  little and finally take really fire
 */
void effect_takefire()
{
  effect_fade_in_slow();
  effect_moving();
  effect_fire();
  effect_fire();
}
 
/*
 * Environement (EMF) affected random delay effect
 * 
 * stablity - level of random stability
 */
void randomDelay(byte stability)
{
  unsigned int seed = analogRead(A0);
  seed = seed * (stability + 1) / random(seed % stability, seed * (stability + 1));
  delay(seed);
}




