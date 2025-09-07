#include <Arduino.h>
#include <Adafruit_NeoPixel.h> //for neopixel
#include <Adafruit_GFX.h> //for oled module
#include <Adafruit_SSD1306.h> //for oled module
#include <Wire.h> //this is for I2C. SPI.h for SPI'
#include <RTClib.h>
#include <Encoder.h>
#include <Fonts/Picopixel.h>
#include <Fonts/Org_01.h>

//sate machine setup
enum State {
  STATE_IDLE,
  STATE_CONFIG_STUDY,
  STATE_CONFIG_BREAK,
  STATE_CONFIG_CYCLE,
  STATE_STUDY,
  STATE_BREAK
};

State currentState = STATE_IDLE;
State previousState = STATE_IDLE; 

//neopixel definitions
#define PIN_NEO_PIXEL 5  // Arduino pin that connects to NeoPixel
#define NUM_PIXELS 24    // The number of LEDs (pixels) on NeoPixel
#define STUDY_PIXELS_PER_MINS 5 // number of mins which acts as one pixel in NeoPixel
#define BREAK_PIXELS_PER_MINS 1
#define CYCLE_PIXELS_PER_MINS 1
#define MAX_STUDY_TIME 120
#define MIN_STUDY_TIME 25
#define MAX_BREAK_TIME 15
#define MIN_BREAK_TIME 5
#define MAX_CYCLE_TIME 5
#define MIN_CYCLE_TIME 1
#define STUDY_MIN_COLOR 250, 100, 0
#define STUDY_ADDITIONAL_TIME 252, 143, 71
#define BREAK_MIN_COLOR 5, 211, 252
#define BREAK_ADDITIONAL_TIME 100, 250, 255
#define CYCLE_MIN_COLOR 200, 0, 255
#define CYCLE_ADDITIONAL_TIME 220, 100, 255


Adafruit_NeoPixel NeoPixel(NUM_PIXELS, PIN_NEO_PIXEL, NEO_GRB + NEO_KHZ800); //neopixel instance
#define LIGHT_DELAY 50

volatile int study_time = MIN_STUDY_TIME; //will be in minutes. Need to take input from user. 
volatile int break_time = MIN_BREAK_TIME;
volatile int cycle = MIN_CYCLE_TIME;

//oled module definitions
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 oled(SCREEN_WIDTH,SCREEN_HEIGHT,&Wire,-1); //oled module instance

#define FRAME_DELAY (0)
#define FRAME_WIDTH (48)
#define FRAME_HEIGHT (48)

RTC_DS1307 rtc;

// encoder definitions
#define ENCODER_CLK 2 // for encoder CLK pin
#define ENCODER_DT  3 //for encoder DT pin
#define ENCODER_BUTTON 4 // for encoder SW pin

bool paused = false;

volatile int CLKstate;
volatile int lastCLKstate;

// returns: 0 = no event, 1 = short press, 2 = long press
int checkButton(unsigned long longPressDuration = 3000) {
  static bool lastButtonState = HIGH;     // last physical button state
  static unsigned long pressedTime = 0;   // time when button was pressed
  static bool longPressHandled = false;   // ensure only one long press event

  bool buttonState = digitalRead(ENCODER_BUTTON);

  // Button just pressed
  if (lastButtonState == HIGH && buttonState == LOW) {
    pressedTime = millis();
    longPressHandled = false;
  }

  // Button is being held down
  if (lastButtonState == LOW && buttonState == LOW) {
    //Serial.print('pressed: ');
    //Serial.print(pressedTime);
    if (!longPressHandled && millis() - pressedTime >= longPressDuration) {
      longPressHandled = true;
      lastButtonState = buttonState;
      Serial.println("long press = TRUE");
      return 2; // long press detected
    }
  }

  // Button just released
  if (lastButtonState == LOW && buttonState == HIGH) {
    if (!longPressHandled && millis() - pressedTime < longPressDuration) {
      lastButtonState = buttonState;
      return 1; // short press detected
    }
  }

  lastButtonState = buttonState;
  return 0; // nothing happened
}


void idle_state() {
  static unsigned long lastUpdate = 0;
  unsigned long nowMillis = millis();

  if (nowMillis - lastUpdate >= 500) {   // update every 0.5s
    lastUpdate = nowMillis;

    DateTime now = rtc.now();  // Read current time from RTC

    int displayHour = now.hour();
    String ampm = "AM";

    if (displayHour == 0) displayHour = 12;           // Midnight
    else if (displayHour >= 12) {                     // PM hours
      ampm = "PM";
      if (displayHour > 12) displayHour -= 12;
    }

    NeoPixel.clear();
    NeoPixel.show();

    oled.clearDisplay();

    // Big clock (hours and minutes)
    oled.setTextSize(5.5);
    oled.setTextColor(SSD1306_WHITE);
    oled.setFont(&Org_01);
    oled.setCursor(2, 20);
    if (displayHour < 10) oled.print("0");
    oled.print(displayHour);
    oled.setTextSize(2);
    oled.setCursor(63,15);
    oled.print(":");
    oled.setCursor(71,20);
    oled.setTextSize(5.5);
    if (now.minute() < 10) oled.print("0");
    oled.print(now.minute());
    

    // AM/PM next to clock
    // oled.setTextSize(1);
    // oled.setCursor(90, 16);  // Adjust X, Y for positioning
    // oled.print(ampm);

    // Date below
    // oled.setTextSize(1);
    // oled.setCursor(35, 35);
    // if (now.day() < 10) oled.print("0");
    // oled.print(now.day());
    // oled.print("-");
    // if (now.month() < 10) oled.print("0");
    // oled.print(now.month());
    // oled.print("-");
    // oled.print(now.year());

    oled.display();
  }
}


void updateEncoder() {

  CLKstate = digitalRead(ENCODER_CLK);

  if (CLKstate != lastCLKstate) {
    lastCLKstate = CLKstate;
    byte data = digitalRead(ENCODER_DT);
    if (!data && CLKstate == LOW) {
      if (currentState==STATE_CONFIG_STUDY && study_time>=(MIN_STUDY_TIME+STUDY_PIXELS_PER_MINS)){
        study_time = study_time - STUDY_PIXELS_PER_MINS; //counterclockwise
        Serial.println(-STUDY_PIXELS_PER_MINS);
        Serial.println("Study Time: ");
        Serial.println(study_time);}
      else if (currentState==STATE_CONFIG_BREAK && break_time>=(MIN_BREAK_TIME+BREAK_PIXELS_PER_MINS)){
        break_time = break_time - BREAK_PIXELS_PER_MINS; //counterclockwise
        Serial.println(-BREAK_PIXELS_PER_MINS);
        Serial.println("Break Time: ");
        Serial.println(break_time);}
      else if (currentState==STATE_CONFIG_CYCLE && cycle>=(MIN_CYCLE_TIME+CYCLE_PIXELS_PER_MINS)){
        cycle = cycle - CYCLE_PIXELS_PER_MINS; //counterclockwise
        Serial.println(-CYCLE_PIXELS_PER_MINS);
        Serial.println("Cycles: ");
        Serial.println(cycle);}
      }


     else if (data && CLKstate == LOW)  {
      if (currentState==STATE_CONFIG_STUDY && study_time<=(MAX_STUDY_TIME-STUDY_PIXELS_PER_MINS)){
        study_time = study_time + STUDY_PIXELS_PER_MINS; //clockwise
        Serial.println(STUDY_PIXELS_PER_MINS);
        Serial.println("Study_Time");
        Serial.println(study_time);}
      else if (currentState==STATE_CONFIG_BREAK && break_time<=(MAX_BREAK_TIME-BREAK_PIXELS_PER_MINS)){
        break_time = break_time + BREAK_PIXELS_PER_MINS; //clockwise
        Serial.println(BREAK_PIXELS_PER_MINS);
        Serial.println("Break_Time");
        Serial.println(break_time);}
      else if (currentState==STATE_CONFIG_CYCLE && cycle<=(MAX_CYCLE_TIME-CYCLE_PIXELS_PER_MINS)){
        cycle = cycle + 1; //clockwise
        Serial.println(CYCLE_PIXELS_PER_MINS);
        Serial.println("Cycles: ");
        Serial.println(cycle);}
      }
    }
    }

void config_study_state(bool reset=false){
  static int pixels_to_show = -1;

  if (reset) {
    pixels_to_show = -1;
    return;   // reset on demand
  }

  if (pixels_to_show == -1) {
        NeoPixel.clear();
        pixels_to_show = floor(study_time / STUDY_PIXELS_PER_MINS) - 1;
        Serial.println("pixels to show: ");
        Serial.println(pixels_to_show);
        for (int i = 0; i <= pixels_to_show; i++) {
            if (i<= floor(MIN_STUDY_TIME / STUDY_PIXELS_PER_MINS)-1){
              NeoPixel.setPixelColor(i, NeoPixel.Color(STUDY_MIN_COLOR));}
            else{
              NeoPixel.setPixelColor(i, NeoPixel.Color(STUDY_ADDITIONAL_TIME));}
              
            NeoPixel.show();
            delay(LIGHT_DELAY); 
        }
        oled.clearDisplay();
        oled.setCursor(0,0);
        oled.setTextSize(1);
        oled.setTextColor(WHITE);
        oled.print("Study Time: ");
        oled.print(study_time);
        oled.display();
    }
  
  //check if one more pixel has been added : means time increased.
  else if ((floor(study_time / STUDY_PIXELS_PER_MINS)-1)-pixels_to_show==1){
      pixels_to_show++;
      NeoPixel.setPixelColor(pixels_to_show, NeoPixel.Color(STUDY_ADDITIONAL_TIME));
      NeoPixel.show();

      oled.clearDisplay();
      oled.setCursor(0,0);
      oled.setTextSize(1);
      oled.setTextColor(WHITE);
      oled.print("Study Time: ");
      oled.print(study_time);
      oled.display();
  }

  else if ((floor(study_time / STUDY_PIXELS_PER_MINS)-1)-pixels_to_show==-1){
      NeoPixel.setPixelColor(pixels_to_show, NeoPixel.Color(0,0,0));
      pixels_to_show--;
      NeoPixel.show();

      oled.clearDisplay();
      oled.setCursor(0,0);
      oled.setTextSize(1);
      oled.setTextColor(WHITE);
      oled.print("Study Time: ");
      oled.print(study_time);
      oled.display();
  }

}

void config_break_state(bool reset=false){
  static int pixels_to_show = -1;

  if (reset) {
    pixels_to_show = -1;   // reset on demand
    return;
  }

  if (pixels_to_show == -1) {
        NeoPixel.clear();
        pixels_to_show = floor(break_time / BREAK_PIXELS_PER_MINS) - 1;
        Serial.println("pixels to show: ");
        Serial.println(pixels_to_show);
        for (int i = 0; i <= pixels_to_show; i++) {
            if (i<= floor(MIN_BREAK_TIME / BREAK_PIXELS_PER_MINS)-1){
              NeoPixel.setPixelColor(i, NeoPixel.Color(BREAK_MIN_COLOR));}
            else{
            NeoPixel.setPixelColor(i, NeoPixel.Color(BREAK_ADDITIONAL_TIME));}

            NeoPixel.show();
            delay(LIGHT_DELAY); 
        }
        oled.clearDisplay();
        oled.setCursor(0,0);
        oled.setTextSize(1);
        oled.setTextColor(WHITE);
        oled.print("Break Time: ");
        oled.print(break_time);
        oled.display();
    }
  
  //check if one more pixel has been added : means time increased.
  else if ((floor(break_time / BREAK_PIXELS_PER_MINS)-1)-pixels_to_show==1){
      pixels_to_show++;
      NeoPixel.setPixelColor(pixels_to_show, NeoPixel.Color(BREAK_ADDITIONAL_TIME));
      NeoPixel.show();

      oled.clearDisplay();
      oled.setCursor(0,0);
      oled.setTextSize(1);
      oled.setTextColor(WHITE);
      oled.print("Break Time: ");
      oled.print(break_time);
      oled.display();
  }

  else if ((floor(break_time / BREAK_PIXELS_PER_MINS)-1)-pixels_to_show==-1){
      NeoPixel.setPixelColor(pixels_to_show, NeoPixel.Color(0,0,0));
      pixels_to_show--;
      NeoPixel.show();

      oled.clearDisplay();
      oled.setCursor(0,0);
      oled.setTextSize(1);
      oled.setTextColor(WHITE);
      oled.print("Break Time: ");
      oled.print(break_time);
      oled.display();
  }

}

void config_cycle_state(bool reset=false){
  static int pixels_to_show = -1;

  if (reset) {
    pixels_to_show = -1; 
    return;
  }

  if (pixels_to_show == -1) {
        NeoPixel.clear();
        pixels_to_show = floor(cycle / CYCLE_PIXELS_PER_MINS) - 1;
        Serial.println("pixels to show: ");
        Serial.println(pixels_to_show);
        for (int i = 0; i <= pixels_to_show; i++) {
            if (i<= floor(MIN_CYCLE_TIME / CYCLE_PIXELS_PER_MINS)-1){
              NeoPixel.setPixelColor(i, NeoPixel.Color(CYCLE_MIN_COLOR));}
            else{
            NeoPixel.setPixelColor(i, NeoPixel.Color(CYCLE_ADDITIONAL_TIME));}

            NeoPixel.show();
            delay(LIGHT_DELAY); 
        }
        oled.clearDisplay();
        oled.setCursor(0,0);
        oled.setTextSize(1);
        oled.setTextColor(WHITE);
        oled.print("Cycles: ");
        oled.print(cycle);
        oled.display();
    }
  
  //check if one more pixel has been added : means time increased.
  else if ((floor(cycle / CYCLE_PIXELS_PER_MINS)-1)-pixels_to_show==1){
      pixels_to_show++;
      NeoPixel.setPixelColor(pixels_to_show, NeoPixel.Color(CYCLE_ADDITIONAL_TIME));
      NeoPixel.show();

      oled.clearDisplay();
      oled.setCursor(0,0);
      oled.setTextSize(1);
      oled.setTextColor(WHITE);
      oled.print("Cycles: ");
      oled.print(cycle);
      oled.display();
  }

  else if ((floor(cycle / CYCLE_PIXELS_PER_MINS)-1)-pixels_to_show==-1){
      NeoPixel.setPixelColor(pixels_to_show, NeoPixel.Color(0,0,0));
      pixels_to_show--;
      NeoPixel.show();

      oled.clearDisplay();
      oled.setCursor(0,0);
      oled.setTextSize(1);
      oled.setTextColor(WHITE);
      oled.print("Cycles: ");
      oled.print(cycle);
      oled.display();
  }

}

void study_state(bool reset=false) {
    static DateTime last;
    static int temp_study_time = -1;
    static int session = 1;

    if (reset) {
      temp_study_time = -1;
      NeoPixel.clear();
      NeoPixel.show();
      return;}

    if (paused) {
    oled.clearDisplay();
    oled.setTextColor(WHITE);
    oled.setCursor(50,20);
    oled.print("||");
    oled.display();
    return;}

    if (temp_study_time == -1) {
        temp_study_time = study_time;
        NeoPixel.clear();
        int pixels_to_show = floor(temp_study_time / STUDY_PIXELS_PER_MINS);
        for (int pixel = 0; pixel < pixels_to_show; pixel++) {
            if (pixel<= floor(MIN_STUDY_TIME / STUDY_PIXELS_PER_MINS)-1){
              NeoPixel.setPixelColor(pixel, NeoPixel.Color(STUDY_MIN_COLOR));}
            else{
              NeoPixel.setPixelColor(pixel, NeoPixel.Color(STUDY_ADDITIONAL_TIME));}

            NeoPixel.show();
            delay(LIGHT_DELAY);
        }  
        last = rtc.now();
    }

    oled.clearDisplay();
    oled.setCursor(10,10);
    oled.setTextColor(WHITE);
    oled.setTextSize(1);
    oled.print("Session");
    oled.print(session);
    oled.display();
    
    NeoPixel.clear();
    int pixels_to_show = floor(temp_study_time / STUDY_PIXELS_PER_MINS);
    for (int pixel = 0; pixel < pixels_to_show; pixel++) {
        if (pixel<= floor(MIN_STUDY_TIME / STUDY_PIXELS_PER_MINS)-1){
          NeoPixel.setPixelColor(pixel, NeoPixel.Color(STUDY_MIN_COLOR));}
        else{
          NeoPixel.setPixelColor(pixel, NeoPixel.Color(STUDY_ADDITIONAL_TIME));}        
    }
    NeoPixel.show();
    

    DateTime now = rtc.now();
    long diffSeconds = now.unixtime() - last.unixtime();
    
    if (diffSeconds >= 5) {   
        last = now;
        temp_study_time -= STUDY_PIXELS_PER_MINS;
    }

    
    if (temp_study_time <= 0) {
        temp_study_time = -1;
        if(session==cycle){
          session=1;
        }
        else{session++;}
        NeoPixel.clear();
        NeoPixel.show();       
        delay(100);
        currentState = STATE_BREAK; 
    }
}


void break_state(bool reset=false){
    static DateTime last;
    static int temp_break_time = -1;
    static int session = 1;

    if (reset) {
      temp_break_time = -1;
      session = 1;
      NeoPixel.clear();
      NeoPixel.show();
      return;}

    if (paused) return;
    
    if (temp_break_time == -1) {
        temp_break_time = break_time;
        NeoPixel.clear();
        int pixels_to_show = floor(temp_break_time / BREAK_PIXELS_PER_MINS);
        for (int pixel = 0; pixel < pixels_to_show; pixel++) {
            if (pixel<= floor(MIN_BREAK_TIME / BREAK_PIXELS_PER_MINS)-1){
              NeoPixel.setPixelColor(pixel, NeoPixel.Color(BREAK_MIN_COLOR));}
            else{
              NeoPixel.setPixelColor(pixel, NeoPixel.Color(BREAK_MIN_COLOR));}
              NeoPixel.show();
              delay(LIGHT_DELAY);
        }
        last = rtc.now();
        
    }

    oled.clearDisplay();
    oled.setCursor(10,10);
    oled.setTextColor(WHITE);
    oled.setTextSize(1);
    oled.print("Session");
    oled.print(session);
    oled.display();
    
    NeoPixel.clear();
    int pixels_to_show = floor(temp_break_time / BREAK_PIXELS_PER_MINS);
    for (int pixel = 0; pixel < pixels_to_show; pixel++) {
        if (pixel<= floor(MIN_BREAK_TIME / BREAK_PIXELS_PER_MINS)-1){
          NeoPixel.setPixelColor(pixel, NeoPixel.Color(BREAK_MIN_COLOR));}
        else{
          NeoPixel.setPixelColor(pixel, NeoPixel.Color(BREAK_MIN_COLOR));}  
    }
    NeoPixel.show();
    

    
    DateTime now = rtc.now();
    long diffSeconds = now.unixtime() - last.unixtime();
    if (diffSeconds >= 1) {   
        last = now;
        temp_break_time -= BREAK_PIXELS_PER_MINS;
    }

    if (temp_break_time <= 0) {
        temp_break_time = -1; 
        NeoPixel.clear();
        NeoPixel.show();

        

        if (session==cycle){
          
          session=1;
          oled.clearDisplay();
          oled.setCursor(0,0);
          oled.setTextSize(1);
          oled.setTextColor(WHITE);
          oled.print("Congradulations!");
          oled.display();


          for (int i=0; i<NUM_PIXELS; i++){
            NeoPixel.setPixelColor(i, NeoPixel.Color(CYCLE_MIN_COLOR)); //completion state
            NeoPixel.show();
            delay(LIGHT_DELAY);
          }

          currentState=STATE_IDLE;
        }
        else if(session<cycle){
          delay(100);
          session++;
          currentState = STATE_STUDY;
    }
  }
}

void setup() {
  Serial.begin(9600);
  lastCLKstate = digitalRead(ENCODER_CLK); 
  NeoPixel.begin();  
  Wire.begin();
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3c);
  rtc.begin();
  pinMode(ENCODER_CLK, INPUT_PULLUP); 
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), updateEncoder, CHANGE);
  //oled.setFont(&Org_01);
}

void loop() {

int buttonEvent = checkButton();
if (currentState == STATE_STUDY || currentState == STATE_BREAK) {
    if (buttonEvent == 1) {   // short press
        paused = !paused;     // toggle pause
    }
}

switch (currentState) {
  case STATE_IDLE:
    idle_state();
    if (buttonEvent == 1) {
      currentState = STATE_CONFIG_STUDY;
      oled.clearDisplay();
      oled.display();
    }
    else if (buttonEvent == 2) {
      study_time = MIN_STUDY_TIME;
      break_time = MIN_BREAK_TIME;
      cycle = MIN_CYCLE_TIME;
      currentState = STATE_STUDY;
    }
    break;

  case STATE_CONFIG_STUDY:
    config_study_state();
    if (buttonEvent == 1) {
      config_study_state(true);
      currentState = STATE_CONFIG_BREAK;
    }
    else if (buttonEvent == 2) {
      config_study_state(true);
      currentState = STATE_IDLE;
    }
    break;

  case STATE_CONFIG_BREAK:
    config_break_state();  
    if (buttonEvent == 1) {
      config_break_state(true);
      currentState = STATE_CONFIG_CYCLE;
    }
    else if (buttonEvent == 2) {
      config_break_state(true);
      currentState = STATE_IDLE;
    }
    break;

  case STATE_CONFIG_CYCLE:
    config_cycle_state();  
    if (buttonEvent == 1) {
      config_cycle_state(true);
      currentState = STATE_STUDY;
    }
    else if (buttonEvent == 2) {
      config_cycle_state(true);
      currentState = STATE_IDLE;
    }
    break;

  case STATE_STUDY:
    study_state();
    if (buttonEvent == 2) {
      study_state(true);
      currentState = STATE_IDLE;
    }
    break;

  case STATE_BREAK:
    break_state();
    if (buttonEvent == 2) {
      break_state(true);
      currentState = STATE_IDLE;
    }
    break;

  }
}