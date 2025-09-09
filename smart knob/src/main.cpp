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
  STATE_CONFIG_TIMER,
  STATE_STUDY,
  STATE_BREAK,
  STATE_TIMER
};

State currentState = STATE_IDLE;

//neopixel definitions
#define PIN_NEO_PIXEL 5  // Arduino pin that connects to NeoPixel
#define NUM_PIXELS 24    // The number of LEDs (pixels) on NeoPixel
#define STUDY_PIXELS_PER_MINS 5 // number of mins which acts as one pixel in NeoPixel
#define BREAK_PIXELS_PER_MINS 1
#define CYCLE_PIXELS_PER_MINS 1
#define TIMER_PIXELS_PER_MINS 5
#define MAX_STUDY_TIME 120
#define MIN_STUDY_TIME 25
#define MAX_BREAK_TIME 15
#define MIN_BREAK_TIME 5
#define MAX_CYCLE_TIME 4
#define MIN_CYCLE_TIME 1
#define MAX_TIMER_TIME 120
#define MIN_TIMER_TIME 10
#define STUDY_MIN_COLOR 250, 100, 0
#define STUDY_ADDITIONAL_TIME 252, 143, 71
#define BREAK_MIN_COLOR 5, 211, 252
#define BREAK_ADDITIONAL_TIME 100, 250, 255
#define CYCLE_MIN_COLOR 200, 0, 255
#define CYCLE_ADDITIONAL_TIME 220, 100, 255
#define TIMER_MIN_COLOR 255, 0, 0
#define TIMER_ADDITIONAL_TIME 255, 38, 38


Adafruit_NeoPixel NeoPixel(NUM_PIXELS, PIN_NEO_PIXEL, NEO_GRB + NEO_KHZ800); //neopixel instance
#define LIGHT_DELAY 50

volatile int study_time = MIN_STUDY_TIME; //will be in minutes. Need to take input from user. 
volatile int break_time = MIN_BREAK_TIME;
volatile int cycle = MIN_CYCLE_TIME;
volatile int timer_time = MIN_TIMER_TIME;

bool paused = false;
bool pomodoro_mode=false;

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



volatile int CLKstate;
volatile int lastCLKstate;

// returns: 0 = no event, 1 = short press, 2 = long press

void config_study_state(bool reset=false);
void config_break_state(bool reset=false);
void config_cycle_state(bool reset=false);
void config_timer_state(bool reset=false);
void study_state(bool reset=false);
void break_state(bool reset=false);
void timer_state(bool reset=false);


int length(int num) {
    return (num == 0) ? 1 : floor(log10(abs(num))) + 1;
}

bool isButtonHeld(){
  return digitalRead(ENCODER_BUTTON)==LOW;
}

int checkButton(unsigned long longPressDuration = 2500) {
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


void updateEncoder() {
   // lockout flag
  CLKstate = digitalRead(ENCODER_CLK);

  if (CLKstate != lastCLKstate) {
    lastCLKstate = CLKstate;
    byte data = digitalRead(ENCODER_DT);

    bool clockwise = (data && CLKstate == LOW);
    bool counterClockwise = (!data && CLKstate == LOW);

    // --- Handle hold+rotate for state navigation ---
    if (isButtonHeld()) {
        // only allow *one* rotation per hold
        if (clockwise) {
          if (currentState == STATE_IDLE) currentState = STATE_CONFIG_STUDY;
          else if (currentState == STATE_CONFIG_STUDY) {config_study_state(true); currentState = STATE_CONFIG_BREAK;} 
          else if (currentState == STATE_CONFIG_BREAK) {config_break_state(true); currentState = STATE_CONFIG_CYCLE;}
          else if (currentState == STATE_CONFIG_TIMER) {config_timer_state(true); currentState = STATE_IDLE;}
          //else if (currentState == STATE_CONFIG_CYCLE) {config_cycle_state(true); currentState = STATE_STUDY;}
        }
        else if (counterClockwise) {
          if (currentState == STATE_CONFIG_CYCLE) {config_cycle_state(true); currentState = STATE_CONFIG_BREAK;}
          else if (currentState == STATE_CONFIG_BREAK) {config_break_state(true); currentState = STATE_CONFIG_STUDY;}
          else if (currentState == STATE_CONFIG_STUDY) {config_study_state(true); currentState = STATE_IDLE;}
          else if (currentState == STATE_IDLE) {currentState = STATE_CONFIG_TIMER;}
          //else if (currentState == STATE_CONFIG_TIMER) {config_timer_state(true);currentState = STATE_TIMER;}
        }
      return; // skip normal time changes
    } 
    
    // --- Normal encoder behavior when not held ---
    if (counterClockwise) {
      if (currentState == STATE_CONFIG_STUDY && study_time >= (MIN_STUDY_TIME+STUDY_PIXELS_PER_MINS)) {
        study_time -= STUDY_PIXELS_PER_MINS;
      }
      else if (currentState == STATE_CONFIG_BREAK && break_time >= (MIN_BREAK_TIME+BREAK_PIXELS_PER_MINS)) {
        break_time -= BREAK_PIXELS_PER_MINS;
      }
      else if (currentState == STATE_CONFIG_CYCLE && cycle >= (MIN_CYCLE_TIME+CYCLE_PIXELS_PER_MINS)) {
        cycle -= CYCLE_PIXELS_PER_MINS;
      }else if (currentState == STATE_CONFIG_TIMER && timer_time >= (MIN_TIMER_TIME+TIMER_PIXELS_PER_MINS)) {
        timer_time -= TIMER_PIXELS_PER_MINS;
      }
    }
    else if (clockwise) {
      if (currentState == STATE_CONFIG_STUDY && study_time <= (MAX_STUDY_TIME-STUDY_PIXELS_PER_MINS)) {
        study_time += STUDY_PIXELS_PER_MINS;
      }
      else if (currentState == STATE_CONFIG_BREAK && break_time <= (MAX_BREAK_TIME-BREAK_PIXELS_PER_MINS)) {
        break_time += BREAK_PIXELS_PER_MINS;
      }
      else if (currentState == STATE_CONFIG_CYCLE && cycle <= (MAX_CYCLE_TIME-CYCLE_PIXELS_PER_MINS)) {
        cycle += CYCLE_PIXELS_PER_MINS;
      }
      else if (currentState == STATE_CONFIG_TIMER && timer_time <= (MAX_TIMER_TIME-TIMER_PIXELS_PER_MINS)) {
        timer_time += TIMER_PIXELS_PER_MINS;
      }
    }
  }
}


void idle_state() {
  static unsigned long lastUpdate = 0;
  unsigned long nowMillis = millis();

  // oled.ssd1306_command(SSD1306_SETCONTRAST);
  // oled.ssd1306_command(0x7F);

  NeoPixel.clear();
  NeoPixel.show();

  if (nowMillis - lastUpdate >= 500) {   // update every 0.5s
    lastUpdate = nowMillis;

    DateTime now = rtc.now();

    char left[3], right[3];
    int xLeft = 1; 
    int xRight = 73;

    int displayHour = now.hour() % 12;
    if (displayHour == 0) displayHour = 12; // handle midnight / noon
    int minutes = now.minute();

    sprintf(left, "%02d", displayHour);
    sprintf(right, "%02d", minutes);

    // Adjust position if first digit is '1'
    if (left[0] == '1' && left[1] == '1'){
      xLeft +=40;
    }

    if (left[0] == '1' && left[1] != '1') {
        xLeft += 20;
    }

    if (left[0] != '1' && left[1] == '1') {
        xLeft += 20;
    }
    

    oled.clearDisplay();

    // Big clock digits
    oled.setTextSize(5);
    oled.setFont(&Org_01);
    oled.setTextColor(SSD1306_WHITE);

    oled.setCursor(xLeft, 36);
    oled.print(left);

    oled.setCursor(xRight, 36);
    oled.print(right);

    // Separator dots
    oled.fillRect(62, 21, 5, 5,  SSD1306_WHITE);
    oled.fillRect(62, 31, 5, 5, SSD1306_WHITE);
    oled.display();
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

        char left[3], right[3];
        int xLeft = 1; 
        int xRight = 73;

        int displayHour = study_time / 60;   // integer division gives full hours
        int minutes = study_time % 60; // remainder gives remaining minutes

        sprintf(left, "%d", displayHour);
        sprintf(right, "%02d", minutes);

        // Adjust position if first digit is '1'

        if (left[0] == '1') {
            xLeft += 50;
        }
        else if (left[0] != '1'){
            xLeft+=30;
        }

        oled.clearDisplay();

        // Big clock digits
        oled.setTextSize(5);
        oled.setFont(&Org_01);
        oled.setTextColor(SSD1306_WHITE);

        oled.setCursor(xLeft, 36);
        oled.print(left);

        oled.setCursor(xRight, 36);
        oled.print(right);

        // Separator dots
        oled.fillRect(62, 21, 5, 5,  SSD1306_WHITE);
        oled.fillRect(62, 31, 5, 5, SSD1306_WHITE);
        oled.display();
    }
  
  //check if one more pixel has been added : means time increased.
  else if ((floor(study_time / STUDY_PIXELS_PER_MINS)-1)-pixels_to_show==1){
      pixels_to_show++;
      NeoPixel.setPixelColor(pixels_to_show, NeoPixel.Color(STUDY_ADDITIONAL_TIME));
      NeoPixel.show();

      char left[3], right[3];
        int xLeft = 1; 
        int xRight = 73;

        int displayHour = study_time / 60;   // integer division gives full hours
        int minutes = study_time % 60; // remainder gives remaining minutes

        sprintf(left, "%d", displayHour);
        sprintf(right, "%02d", minutes);

        // Adjust position if first digit is '1'

        if (left[0] == '1') {
            xLeft += 50;
        }
        else if (left[0] != '1'){
            xLeft+=30;
        }

        oled.clearDisplay();

        // Big clock digits
        oled.setTextSize(5);
        oled.setFont(&Org_01);
        oled.setTextColor(SSD1306_WHITE);

        oled.setCursor(xLeft, 36);
        oled.print(left);

        oled.setCursor(xRight, 36);
        oled.print(right);

        // Separator dots
        oled.fillRect(62, 21, 5, 5,  SSD1306_WHITE);
        oled.fillRect(62, 31, 5, 5, SSD1306_WHITE);
        oled.display();
      
  }

  else if ((floor(study_time / STUDY_PIXELS_PER_MINS)-1)-pixels_to_show==-1){
      NeoPixel.setPixelColor(pixels_to_show, NeoPixel.Color(0,0,0));
      pixels_to_show--;
      NeoPixel.show();

      char left[3], right[3];
        int xLeft = 1; 
        int xRight = 73;

        int displayHour = study_time / 60;   // integer division gives full hours
        int minutes = study_time % 60; // remainder gives remaining minutes

        sprintf(left, "%d", displayHour);
        sprintf(right, "%02d", minutes);

        // Adjust position if first digit is '1'

        if (left[0] == '1') {
            xLeft += 50;
        }
        else if (left[0] != '1'){
            xLeft+=30;
        }

        oled.clearDisplay();

        // Big clock digits
        oled.setTextSize(5);
        oled.setFont(&Org_01);
        oled.setTextColor(SSD1306_WHITE);

        oled.setCursor(xLeft, 36);
        oled.print(left);

        oled.setCursor(xRight, 36);
        oled.print(right);

        // Separator dots
        oled.fillRect(62, 21, 5, 5,  SSD1306_WHITE);
        oled.fillRect(62, 31, 5, 5, SSD1306_WHITE);
        oled.display();
  }
  
  char left[3], right[3];
        int xLeft = 1; 
        int xRight = 73;

        int displayHour = study_time / 60;   // integer division gives full hours
        int minutes = study_time % 60; // remainder gives remaining minutes

        sprintf(left, "%d", displayHour);
        sprintf(right, "%02d", minutes);


        xLeft+=50;
        
      
        // Big clock digits
        oled.setTextSize(1);
        oled.setFont(&Org_01);
        oled.setTextColor(SSD1306_WHITE);

        oled.setCursor(xLeft, 50);
        oled.print("H");

        oled.setCursor(xRight, 50);
        oled.print("M");

        // Separator dots
        oled.display();

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

        char left[3], right[3];
        int xLeft = 1; 
        int xRight = 73;

        int displayHour = break_time / 60;   // integer division gives full hours
        int minutes = break_time % 60; // remainder gives remaining minutes

        sprintf(left, "%d", displayHour);
        sprintf(right, "%02d", minutes);

        // Adjust position if first digit is '1'

        if (left[0] == '1') {
            xLeft += 50;
        }
        else if (left[0] != '1'){
            xLeft+=30;
        }

        oled.clearDisplay();

        // Big clock digits
        oled.setTextSize(5);
        oled.setFont(&Org_01);
        oled.setTextColor(SSD1306_WHITE);

        oled.setCursor(xLeft, 36);
        oled.print(left);

        oled.setCursor(xRight, 36);
        oled.print(right);

        // Separator dots
        oled.fillRect(62, 21, 5, 5,  SSD1306_WHITE);
        oled.fillRect(62, 31, 5, 5, SSD1306_WHITE);
        oled.display();
        
    }
  
  //check if one more pixel has been added : means time increased.
  else if ((floor(break_time / BREAK_PIXELS_PER_MINS)-1)-pixels_to_show==1){
      pixels_to_show++;
      NeoPixel.setPixelColor(pixels_to_show, NeoPixel.Color(BREAK_ADDITIONAL_TIME));
      NeoPixel.show();

      char left[3], right[3];
        int xLeft = 1; 
        int xRight = 73;

        int displayHour = break_time / 60;   // integer division gives full hours
        int minutes = break_time % 60; // remainder gives remaining minutes

        sprintf(left, "%d", displayHour);
        sprintf(right, "%02d", minutes);

        // Adjust position if first digit is '1'

        if (left[0] == '1') {
            xLeft += 50;
        }
        else if (left[0] != '1'){
            xLeft+=30;
        }

        oled.clearDisplay();

        // Big clock digits
        oled.setTextSize(5);
        oled.setFont(&Org_01);
        oled.setTextColor(SSD1306_WHITE);

        oled.setCursor(xLeft, 36);
        oled.print(left);

        oled.setCursor(xRight, 36);
        oled.print(right);

        // Separator dots
        oled.fillRect(62, 21, 5, 5,  SSD1306_WHITE);
        oled.fillRect(62, 31, 5, 5, SSD1306_WHITE);
        oled.display();

      
  }

  else if ((floor(break_time / BREAK_PIXELS_PER_MINS)-1)-pixels_to_show==-1){
      NeoPixel.setPixelColor(pixels_to_show, NeoPixel.Color(0,0,0));
      pixels_to_show--;
      NeoPixel.show();

      char left[3], right[3];
        int xLeft = 1; 
        int xRight = 73;

        int displayHour = break_time / 60;   // integer division gives full hours
        int minutes = break_time % 60; // remainder gives remaining minutes

        sprintf(left, "%d", displayHour);
        sprintf(right, "%02d", minutes);

        // Adjust position if first digit is '1'

        if (left[0] == '1') {
            xLeft += 50;
        }
        else if (left[0] != '1'){
            xLeft+=30;
        }

        oled.clearDisplay();

        // Big clock digits
        oled.setTextSize(5);
        oled.setFont(&Org_01);
        oled.setTextColor(SSD1306_WHITE);

        oled.setCursor(xLeft, 36);
        oled.print(left);

        oled.setCursor(xRight, 36);
        oled.print(right);

        // Separator dots
        oled.fillRect(62, 21, 5, 5,  SSD1306_WHITE);
        oled.fillRect(62, 31, 5, 5, SSD1306_WHITE);
        oled.display();

      
  }
  
  char left[3], right[3];
        int xLeft = 1; 
        int xRight = 73;

        int displayHour = break_time / 60;   // integer division gives full hours
        int minutes = break_time % 60; // remainder gives remaining minutes

        sprintf(left, "%d", displayHour);
        sprintf(right, "%02d", minutes);


        xLeft+=50;
        
      
        // Big clock digits
        oled.setTextSize(1);
        oled.setFont(&Org_01);
        oled.setTextColor(SSD1306_WHITE);

        oled.setCursor(xLeft, 50);
        oled.print("H");

        oled.setCursor(xRight, 50);
        oled.print("M");

        // Separator dots
        oled.display();

}

void config_cycle_state(bool reset=false){
  static int pixels_to_show = -1;

  if (reset) {
    pixels_to_show = -1; 
    return;
  }

  if (pixels_to_show == -1) {
        NeoPixel.clear();
        pixels_to_show = floor(cycle * CYCLE_PIXELS_PER_MINS) - 1;
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

        // oled.clearDisplay();
        // if (cycle==1){
        //   oled.setCursor(64,36);
        // }
        // else{oled.setCursor(56,36);}
        // oled.setTextSize(5);
        // oled.setFont(&Org_01);
        // oled.print(cycle);
        
    }
  
  //check if one more pixel has been added : means time increased.
  else if ((floor(cycle / CYCLE_PIXELS_PER_MINS)-1)-pixels_to_show==1){
      pixels_to_show++;
      NeoPixel.setPixelColor(pixels_to_show, NeoPixel.Color(CYCLE_ADDITIONAL_TIME));
      NeoPixel.show();

      // oled.clearDisplay();
      //   if (cycle==1){
      //     oled.setCursor(64,36);
      //   }
      //   else{oled.setCursor(56,36);}
      //   oled.setTextSize(5);
      //   oled.setFont(&Org_01);
      //   oled.print(cycle);
        

      
  }

  else if ((floor(cycle / CYCLE_PIXELS_PER_MINS)-1)-pixels_to_show==-1){
      NeoPixel.setPixelColor(pixels_to_show, NeoPixel.Color(0,0,0));
      pixels_to_show--;
      NeoPixel.show();

      // oled.clearDisplay();
      //   if (cycle==1){
      //     oled.setCursor(64,36);
      //   }
      //   else{oled.setCursor(56,36);}
      //   oled.setTextSize(5);
      //   oled.setFont(&Org_01);
      //   oled.print(cycle);
        

      
  }
  oled.clearDisplay();
  oled.setFont(&Picopixel);
  oled.setCursor(3,35);
  oled.setTextSize(4);
  oled.print("SESSIONS");
  oled.display();

}

void config_timer_state(bool reset=false){
  static int pixels_to_show = -1;

  if (reset) {
    pixels_to_show = -1;
    return;   // reset on demand

  }

  if (pixels_to_show == -1) {
        NeoPixel.clear();
        pixels_to_show = floor(timer_time / TIMER_PIXELS_PER_MINS) - 1;
        Serial.println("pixels to show: ");
        Serial.println(pixels_to_show);
        for (int i = 0; i <= pixels_to_show; i++) {
            if (i<= floor(MIN_TIMER_TIME / TIMER_PIXELS_PER_MINS)-1){
              NeoPixel.setPixelColor(i, NeoPixel.Color(TIMER_MIN_COLOR));}
            else{
              NeoPixel.setPixelColor(i, NeoPixel.Color(TIMER_ADDITIONAL_TIME));}
              
            NeoPixel.show();
            delay(LIGHT_DELAY); 
        }


        char left[3], right[3];
        int xLeft = 1; 
        int xRight = 73;

        int displayHour = timer_time / 60;   // integer division gives full hours
        int minutes = timer_time % 60; // remainder gives remaining minutes

        sprintf(left, "%d", displayHour);
        sprintf(right, "%02d", minutes);

        // Adjust position if first digit is '1'

        if (left[0] == '1') {
            xLeft += 50;
        }
        else if (left[0] != '1'){
            xLeft+=30;
        }

        oled.clearDisplay();

        // Big clock digits
        oled.setTextSize(5);
        oled.setFont(&Org_01);
        oled.setTextColor(SSD1306_WHITE);

        oled.setCursor(xLeft, 36);
        oled.print(left);

        oled.setCursor(xRight, 36);
        oled.print(right);

        // Separator dots
        oled.fillRect(62, 21, 5, 5,  SSD1306_WHITE);
        oled.fillRect(62, 31, 5, 5, SSD1306_WHITE);
        oled.display();
    }
  
  //check if one more pixel has been added : means time increased.
  else if ((floor(timer_time / TIMER_PIXELS_PER_MINS)-1)-pixels_to_show==1){
      pixels_to_show++;
      NeoPixel.setPixelColor(pixels_to_show, NeoPixel.Color(TIMER_ADDITIONAL_TIME));
      NeoPixel.show();

      
        char left[3], right[3];
        int xLeft = 1; 
        int xRight = 73;

        int displayHour = timer_time / 60;   // integer division gives full hours
        int minutes = timer_time % 60; // remainder gives remaining minutes

        sprintf(left, "%d", displayHour);
        sprintf(right, "%02d", minutes);

        // Adjust position if first digit is '1'

        if (left[0] == '1') {
            xLeft += 50;
        }
        else if (left[0] != '1'){
            xLeft+=30;
        }

        oled.clearDisplay();

        // Big clock digits
        oled.setTextSize(5);
        oled.setFont(&Org_01);
        oled.setTextColor(SSD1306_WHITE);

        oled.setCursor(xLeft, 36);
        oled.print(left);

        oled.setCursor(xRight, 36);
        oled.print(right);

        // Separator dots
        oled.fillRect(62, 21, 5, 5,  SSD1306_WHITE);
        oled.fillRect(62, 31, 5, 5, SSD1306_WHITE);
        oled.display();
      
  }

  else if ((floor(timer_time / TIMER_PIXELS_PER_MINS)-1)-pixels_to_show==-1){
      NeoPixel.setPixelColor(pixels_to_show, NeoPixel.Color(0,0,0));
      pixels_to_show--;
      NeoPixel.show();

        
        char left[3], right[3];
        int xLeft = 1; 
        int xRight = 73;

        int displayHour = timer_time / 60;   // integer division gives full hours
        int minutes = timer_time % 60; // remainder gives remaining minutes

        sprintf(left, "%d", displayHour);
        sprintf(right, "%02d", minutes);

        // Adjust position if first digit is '1'

        if (left[0] == '1') {
            xLeft += 50;
        }
        else if (left[0] != '1'){
            xLeft+=30;
        }

        oled.clearDisplay();

        // Big clock digits
        oled.setTextSize(5);
        oled.setFont(&Org_01);
        oled.setTextColor(SSD1306_WHITE);

        oled.setCursor(xLeft, 36);
        oled.print(left);

        oled.setCursor(xRight, 36);
        oled.print(right);

        // Separator dots
        oled.fillRect(62, 21, 5, 5,  SSD1306_WHITE);
        oled.fillRect(62, 31, 5, 5, SSD1306_WHITE);
        oled.display();

      
  }

  char left[3], right[3];
        int xLeft = 1; 
        int xRight = 73;

        int displayHour = timer_time / 60;   // integer division gives full hours
        int minutes = timer_time % 60; // remainder gives remaining minutes

        sprintf(left, "%d", displayHour);
        sprintf(right, "%02d", minutes);


        xLeft+=50;
        
      
        // Big clock digits
        oled.setTextSize(1);
        oled.setFont(&Org_01);
        oled.setTextColor(SSD1306_WHITE);

        oled.setCursor(xLeft, 50);
        oled.print("H");

        oled.setCursor(xRight, 50);
        oled.print("M");

        // oled.setTextSize(2);
        // oled.setCursor(60,55);
        // oled.print("T");

        
        //oled.clearDisplay();
        // oled.setTextSize(1);
        // oled.setFont(&Picopixel);
        // oled.setCursor(27,55);
        // oled.print("TIMER");
        oled.display();
  
}

void study_state(bool reset=false) {
    static DateTime last;
    static int temp_study_time = -1;
    static int session = 1;
    static bool justStarted = false;

    // Serial.print("DEBUG: cycle before STUDY = ");
    // Serial.println(cycle);

    if (reset) {
      temp_study_time = -1;
      justStarted = false;
      if (pomodoro_mode==true){pomodoro_mode=false;}
      NeoPixel.clear();
      NeoPixel.show();
      return;
    }

    if (paused) {
      oled.clearDisplay();
      oled.setTextColor(WHITE);
      oled.setCursor(0,40);
      oled.setTextSize(5);
      oled.setFont(&Picopixel);
      oled.print("PAUSED!");
      oled.display();
      return;
    }

    if (temp_study_time == -1) {
        if (pomodoro_mode){
          temp_study_time=MIN_STUDY_TIME;
        } else {
          temp_study_time = study_time;
        }

        NeoPixel.clear();
        int pixels_to_show = floor(temp_study_time / STUDY_PIXELS_PER_MINS);
        for (int pixel = 0; pixel < pixels_to_show; pixel++) {
            if (pixel<= floor(MIN_STUDY_TIME / STUDY_PIXELS_PER_MINS)-1){
              NeoPixel.setPixelColor(pixel, NeoPixel.Color(STUDY_MIN_COLOR));
            } else {
              NeoPixel.setPixelColor(pixel, NeoPixel.Color(STUDY_ADDITIONAL_TIME));
            }
            NeoPixel.show();
            delay(LIGHT_DELAY);
        }  
        last = rtc.now();
        justStarted = true;   // block timer subtraction on first loop
    }

    // Always show session info

    // NeoPixel refresh
    NeoPixel.clear();
    int pixels_to_show = floor(temp_study_time / STUDY_PIXELS_PER_MINS);
    for (int pixel = 0; pixel < pixels_to_show; pixel++) {
        if (pixel<= floor(MIN_STUDY_TIME / STUDY_PIXELS_PER_MINS)-1){
          NeoPixel.setPixelColor(pixel, NeoPixel.Color(STUDY_MIN_COLOR));
        } else {
          NeoPixel.setPixelColor(pixel, NeoPixel.Color(STUDY_ADDITIONAL_TIME));
        }        
    }
    NeoPixel.show();

    // Timer logic
    DateTime now = rtc.now();
    long diffSeconds = now.unixtime() - last.unixtime();
    
    if (!justStarted && diffSeconds >= 5) {   
        last = now;
        temp_study_time -= STUDY_PIXELS_PER_MINS;
    }
    justStarted = false; // only skip once

    // End of session
    if (temp_study_time <= 0) {
        temp_study_time = -1;
        if(session==cycle){
          session=1;
        } else {
          session++;
        }
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
      if (pomodoro_mode==true){
        pomodoro_mode=false;
      }
      NeoPixel.clear();
      NeoPixel.show();
      return;}

    if (paused) {
      oled.clearDisplay();
      oled.setTextColor(WHITE);
      oled.setCursor(0,40);
      oled.setTextSize(5);
      oled.setFont(&Picopixel);
      oled.print("PAUSED!");
      oled.display();
      return;}
    
    if (temp_break_time == -1) {
        if(pomodoro_mode){
          if(session==MAX_CYCLE_TIME){
            temp_break_time=MAX_BREAK_TIME;}
          else{
            temp_break_time=MIN_BREAK_TIME;
          }
        }
        else{temp_break_time = break_time;}
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
          pomodoro_mode=false;
          session=1;
          


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


void timer_state(bool reset=false) {
    static DateTime last;
    static int temp_timer_time = -1;

    if (reset) {
      temp_timer_time = -1;
      NeoPixel.clear();
      NeoPixel.show();
      return;}

    if (paused) {
      oled.clearDisplay();
      oled.setTextColor(WHITE);
      oled.setCursor(0,40);
      oled.setTextSize(5);
      oled.setFont(&Picopixel);
      oled.print("PAUSED!");
      oled.display();
      return;}

    if (temp_timer_time == -1) {
        temp_timer_time = timer_time;
        NeoPixel.clear();
        int pixels_to_show = floor(temp_timer_time / TIMER_PIXELS_PER_MINS);
        for (int pixel = 0; pixel < pixels_to_show; pixel++) {
            if (pixel<= floor(MIN_TIMER_TIME / TIMER_PIXELS_PER_MINS)-1){
              NeoPixel.setPixelColor(pixel, NeoPixel.Color(TIMER_MIN_COLOR));}
            else{
              NeoPixel.setPixelColor(pixel, NeoPixel.Color(TIMER_ADDITIONAL_TIME));}

            NeoPixel.show();
            delay(LIGHT_DELAY);
        }  
        last = rtc.now();
    }

    
    NeoPixel.clear();
    int pixels_to_show = floor(temp_timer_time / TIMER_PIXELS_PER_MINS);
    for (int pixel = 0; pixel < pixels_to_show; pixel++) {
        if (pixel<= floor(MIN_TIMER_TIME / TIMER_PIXELS_PER_MINS)-1){
          NeoPixel.setPixelColor(pixel, NeoPixel.Color(TIMER_MIN_COLOR));}
        else{
          NeoPixel.setPixelColor(pixel, NeoPixel.Color(TIMER_ADDITIONAL_TIME));}        
    }
    NeoPixel.show();
    

    DateTime now = rtc.now();
    long diffSeconds = now.unixtime() - last.unixtime();
    
    if (diffSeconds >= 5) {   
        last = now;
        temp_timer_time -= TIMER_PIXELS_PER_MINS;
    }

    
    if (temp_timer_time <= 0) {
        temp_timer_time = -1;
        NeoPixel.clear();
        for (int i=0; i<NUM_PIXELS; i++){
            NeoPixel.setPixelColor(i, NeoPixel.Color(CYCLE_MIN_COLOR)); //completion state
            NeoPixel.show();
            delay(LIGHT_DELAY);
          }
        currentState = STATE_IDLE; 
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
  if (currentState == STATE_STUDY || currentState == STATE_BREAK || currentState == STATE_TIMER) {
      if (buttonEvent == 1) {   // short press
          paused = !paused;     // toggle pause
      }
  }

  switch (currentState) {
    case STATE_IDLE:
      idle_state();
      if(buttonEvent==2){
        pomodoro_mode=true;
        currentState=STATE_CONFIG_CYCLE;
      }
      break;

    case STATE_CONFIG_STUDY:
      config_study_state();
      break;

    case STATE_CONFIG_BREAK:
      config_break_state(); 
      break;

    case STATE_CONFIG_CYCLE:
      config_cycle_state();  
      if (buttonEvent == 2) {
        config_cycle_state(true);
        currentState = STATE_STUDY;
      }
      break;
    
    case STATE_CONFIG_TIMER:
      config_timer_state();
      if (buttonEvent == 2) {
        config_timer_state(true);
        currentState = STATE_TIMER;
      }
      break;

    case STATE_TIMER:
      timer_state();
      if (buttonEvent == 2) {
        timer_state(true);
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