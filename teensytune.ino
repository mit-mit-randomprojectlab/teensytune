/* 
Teensytune: A homebuilt MIDI controller/keyboard built around an old broken keyboard using the 
Teensy microcontroller

By mit-mit

This code runs on a Teensy 3.1 and connects to a 49 key piano keyboard using a switch matrix layout
and several input controls (including buttons, a potentiometer, LCD two-line character display, 
two-axis joystick and neopixels) and outputs MIDI messages across the Teensy USB.

For details on the hardware and contruction process, see:


Required libraries:

Adafruit Neopixels (used to run neopixels):
https://github.com/adafruit/Adafruit_NeoPixel

Adafruit MCP23008 (used for port expander, providing sufficient button inputs):
https://github.com/adafruit/Adafruit-MCP23008-library

LiquidCrystal_I2C (used to drive the two-line display):
https://www.dfrobot.com/wiki/index.php/I2C/TWI_LCD1602_Module_(SKU:_DFR0063)

*/

// TODO:

// Fix display text bugs out (related to interupts?)
// Fix loop record to deal with when note isn't initially pressed: seems to work first loop but not next

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <Adafruit_MCP23008.h>
#include <Adafruit_NeoPixel.h>

// Instrument list (General User)
char instr_list[] = {0,3,6,17,8,11,10,
  2,4,5,25,26,29,
  40,45,48,46,57,61,68,50,53,47,114,
  1,80,38,86,81,88,89,95,96,97,
  76,55,124,0,0,0,0,0,0,0,0,0,0,0,0};

const char* const instr_names[] = {"Piano C1","Honky C#1","Harpsichord D1","Organ D#1","Celeste E1","Vibraphone F1","Music Box F#1",
  "Elec Piano G1","Rhodes G#1","DX7 Piano A1",
  "Steel Guitar A#1","Jazz Guitar B1","Overdrive C2",
  "Violin C#2","Pizzicato D2","Strings D#2","Harp E2","Trombone F2","Brass F#2","Oboe G2","Synth String G#2","Synth Voice A2","Timpani A#2","Steel Drum B2",
  "Sine C3","Square C#3","Synth Bass D3","Fifth Saw D#3","Saw Decline E3","Fantasia F3","Warm Pad F#3","Sweep Pad G3","Ice Rain G#3","Soundtrack A3",
  "Bottle A#3","Orch Hit B3","Telephone C4",
  "Piano","Piano","Piano","Piano","Piano","Piano","Piano","Piano","Piano","Piano","Piano","Piano"};

// Initialise display
LiquidCrystal_I2C lcd(0x20,16,2);

// memory to store key pressed state
char keystate[49] = {0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0};

char keystate_prev[49] = {0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0};

// Keyboard controls and channel select
#define CHANSELECT_PIN 3 // MCP
char main_keyboard_channel = 1;
int main_keyboard_program = 0;
int main_keyboard_program_ind = 0;
int looper_program = 0;
int looper2_program = 0;

char chansel_state = 0;
char chansel_state_prev = 0;
char chansel_waitingforchoice = 0;

// pitch/modulation bend joystick
#define PITCHBEND_PIN 8 // analog
#define MODULATION_PIN 7 // analog
int pitchbend = 0;
int modulation = 0;
int pitchbend_prev = 0;
int modulation_prev = 0;
int pitchbend_center = 0;
int modulation_center = 0;

// Stuff for MPC Multiplexer
Adafruit_MCP23008 mcp;
uint8_t mcp_button_state[8] = {0,0,0,0,0,0,0,0};

///////////////////////////
// Setup Neopixel lights

#define LED_PIN 23

Adafruit_NeoPixel strip = Adafruit_NeoPixel(2, LED_PIN, NEO_GRB + NEO_KHZ800);

// setPixelHue - Quick function to balance RGB channels to create a particular colour
void setPixelHue(int pixel, int hue)
{
  while(hue < 0)
  {
    hue += 360;
  }
  
  float h = hue % 360;
    
  float sectorPos = h / 60;
  int sectorNumber = (int)floor(sectorPos);
  float fractionalSector = sectorPos - sectorNumber;
        
  float q = 1 - fractionalSector;
  float t = fractionalSector;
        
  switch(sectorNumber)
  {
    case 0:
      strip.setPixelColor(pixel, 255, 255 * t, 0);
      break;
    case 1:
      strip.setPixelColor(pixel, 255 * q, 255, 0);
      break;
    case 2:
      strip.setPixelColor(pixel, 0, 255, 255 * t);
      break;
    case 3:
      strip.setPixelColor(pixel, 0, 255 * q, 255);
      break;
    case 4:
      strip.setPixelColor(pixel, 255 * t, 0, 255);
      break;
    case 5:
      strip.setPixelColor(pixel, 255, 0, 255 * q);
      break;
  }
}

/////////////////////////////////////////////////////
// Stuff for interupt driven drum machine and looper

#define DRUMONOFF_PIN 1 // MCP
#define TEMPO_PIN 6 // analog
#define LOOPER_PIN 2 // MCP
#define LOOPER2_PIN 4 // MCP

// Drum machine
char drumonoff_state = 0;
char drumonoff_state_prev = 0;

#define TIE 0x2
#define TEN 0x1

char drum_state = 0;
char drum_instr[] = {0,36,42,41,48,34,81,56,27,49};
char drum_pattern[] = {1,0,0,0,
  2,0,0,0,
  1,0,1,1,
  2,0,0,0};
char drum_ind = 0;

int tempo = 0;
int tempo_prev = 0;

uint32_t tempo_cycles = 50000000;
char new_tempo = 0;

// Looper A
char looperonoff_state = 0;
char looperonoff_state_prev = 0;
elapsedMillis looperonoff_holdtime;

char looper_playback_state = 0;
char looper_record_state = 0;
char looper_recording = 0;
char looper_has_tune = 0;
elapsedMillis looper_record_time;
char looper_root_note = 0;
char looper_transpose = 0;
char looper_transpose_use = 0;

#define LOOPBUFFERSIZE 128
char looper_buffer_keys[LOOPBUFFERSIZE];
float looper_buffer_time[LOOPBUFFERSIZE];
char looper_n = 0;
char looper_channel = 2;

char looper_ind = 0;
char looper_keystate[49] = {0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0};

// Looper B
char looper2onoff_state = 0;
char looper2onoff_state_prev = 0;
elapsedMillis looper2onoff_holdtime;

char looper2_playback_state = 0;
char looper2_record_state = 0;
char looper2_recording = 0;
char looper2_has_tune = 0;
elapsedMillis looper2_record_time;
char looper2_root_note = 0;
char looper2_transpose = 0;
char looper2_transpose_use = 0;

char looper2_buffer_keys[LOOPBUFFERSIZE];
float looper2_buffer_time[LOOPBUFFERSIZE];
char looper2_n = 0;
char looper2_channel = 3;

char looper2_ind = 0;
char looper2_keystate[49] = {0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0,0,0,0,0,0,
  0};

void pit0_isr(void){

  // toggle pin13 led on teensy
  digitalWrite( 13, !digitalRead(13) );

  // check for looper initialisation
  if (drum_ind == 0) {
    if ( (looper_has_tune == 1) && (looper_playback_state == 1) ) {
      looper_transpose_use = looper_transpose;
      start_looper(); // start looper playback
      strip.setPixelColor(1, 0, 255, 0);
      lcd.setCursor(14, 0);
      lcd.print("A");
    }
    else if ( (looper_has_tune == 0) && (looper_record_state == 1) ) {
      if (looper_recording == 1) { // just finished recording loop
        looper_recording = 0;
        if (looper_n > 0) {
          usbMIDI.sendProgramChange(main_keyboard_program,looper_channel); // update looper channel program
          looper_root_note = looper_buffer_keys[0] % 12; // store root note based on first note
          looper_transpose = 0;
          looper_transpose_use = 0;
          looper_has_tune = 1;
          looper_playback_state = 1;
          start_looper(); // start looper playback
          strip.setPixelColor(1, 0, 255, 0);
          lcd.setCursor(14, 0);
          lcd.print("A");
        }
        else {
          strip.setPixelColor(1, 0, 0, 255);
        }
      }
      else {
        start_looper_record(); // start recording loop
        strip.setPixelColor(1, 255, 0, 0);
      }
    }
    else {
      strip.setPixelColor(1, 0, 0, 255);
    }
    
    if ( (looper2_has_tune == 1) && (looper2_playback_state == 1) ) {
      start_looper2(); // start looper playback
      strip.setPixelColor(0, 0, 255, 0);
      lcd.setCursor(15, 0);
      lcd.print("B");
    }
    else if ( (looper2_has_tune == 0) && (looper2_record_state == 1) ) {
      if (looper2_recording == 1) { // just finished recording loop
        looper2_recording = 0;
        if (looper2_n > 0) {
          usbMIDI.sendProgramChange(main_keyboard_program,looper2_channel); // update looper channel program
          looper2_has_tune = 1;
          looper2_playback_state = 1;
          start_looper2(); // start looper playback
          strip.setPixelColor(0, 0, 255, 0);
          lcd.setCursor(15, 0);
          lcd.print("B");
        }
        else {
          strip.setPixelColor(0, 0, 0, 255);
        }
      }
      else {
        start_looper2_record(); // start recording loop
        strip.setPixelColor(0, 255, 0, 0);
      }
    }
    else {
      strip.setPixelColor(0, 0, 0, 255);
    }
  }

  // Play drums
  if (drum_instr[drum_pattern[drum_ind]] > 0) {
    usbMIDI.sendNoteOn(drum_instr[drum_pattern[drum_ind]], 99, 10);
    usbMIDI.sendNoteOff(drum_instr[drum_pattern[drum_ind]], 0, 10);
  }
  drum_ind++;
  if (drum_ind == 16) {
    drum_ind = 0;
  }
  
  // Check for tempo update
  if (new_tempo > 0) {
    SIM_SCGC6 |= SIM_SCGC6_PIT;
    PIT_MCR = 0x00;
    NVIC_ENABLE_IRQ(IRQ_PIT_CH0);
    PIT_LDVAL0 = tempo_cycles;
    PIT_TCTRL0 = TIE;
    PIT_TCTRL0 |= TEN;
    PIT_TFLG0 |= 1;
    new_tempo = 0;
  }
  PIT_TFLG0 = 1; 
}

void start_drums() {
  SIM_SCGC6 |= SIM_SCGC6_PIT;
  PIT_MCR = 0x00;
  NVIC_ENABLE_IRQ(IRQ_PIT_CH0);
  //PIT_LDVAL0 = 0x2faf080;
  PIT_LDVAL0 = tempo_cycles;
  PIT_TCTRL0 = TIE;
  PIT_TCTRL0 |= TEN;
  PIT_TFLG0 |= 1;
}

void stop_drums() {
  NVIC_DISABLE_IRQ(IRQ_PIT_CH0);
  stop_looper();
}

void start_looper() {
  stop_looper();
  if (looper_n == 0) {
    return;
  }
  SIM_SCGC6 |= SIM_SCGC6_PIT;
  PIT_MCR = 0x00;
  NVIC_ENABLE_IRQ(IRQ_PIT_CH1);
  PIT_LDVAL1 = (16.0*looper_buffer_time[looper_ind])*tempo_cycles;
  //PIT_LDVAL1 = tempo_cycles;
  PIT_TCTRL1 = TIE;
  PIT_TCTRL1 |= TEN;
  PIT_TFLG1 |= 1;
}

void stop_looper() {
  NVIC_DISABLE_IRQ(IRQ_PIT_CH1);
  looper_ind = 0;
  for (int i = 0; i < 49; i++) { // stop active keys
    if (looper_keystate[i] > 0) {
      usbMIDI.sendNoteOff(i+36, 0, looper_channel);
      looper_keystate[i] = 0;
    }
  }
}

void pit1_isr(void){
  if (looper_buffer_keys[looper_ind] < 49) {
    looper_keystate[looper_buffer_keys[looper_ind]] = 1;
    usbMIDI.sendNoteOn(looper_buffer_keys[looper_ind]+36+looper_transpose_use, 99, looper_channel);  // 60 = C4
  }
  else {
    looper_keystate[looper_buffer_keys[looper_ind]-49] = 0;
    usbMIDI.sendNoteOff(looper_buffer_keys[looper_ind]-49+36+looper_transpose_use, 0, looper_channel);  // 60 = C4
  }

  looper_ind++;
  if (looper_ind >= looper_n) {
    stop_looper();
    return;
  }

  //SIM_SCGC6 |= SIM_SCGC6_PIT;
  //PIT_MCR = 0x00;
  //NVIC_ENABLE_IRQ(IRQ_PIT_CH1);
  PIT_LDVAL1 = (16.0*(looper_buffer_time[looper_ind]-looper_buffer_time[looper_ind-1]))*(tempo_cycles);
  //PIT_LDVAL1 = tempo_cycles;
  PIT_TCTRL1 = TIE;
  PIT_TCTRL1 |= TEN;
  PIT_TFLG1 |= 1;
  PIT_TFLG1 = 1;
  
}

void start_looper_record() {
  looper_recording = 1;
  looper_n = 0;
  for (int i = 0; i < 49; i++) { // create events for keys initially held down
    if (keystate[i] > 0) {
      looper_buffer_keys[looper_n] = i;
      looper_buffer_time[looper_n] = 0.0;
      looper_n++;
    }
  }
  looper_record_time = 0;
}

void stop_looper_record() {
  looper_recording = 0;
}

void start_looper2() {
  stop_looper2();
  if (looper2_n == 0) {
    return;
  }
  SIM_SCGC6 |= SIM_SCGC6_PIT;
  PIT_MCR = 0x00;
  NVIC_ENABLE_IRQ(IRQ_PIT_CH2);
  PIT_LDVAL2 = (16.0*looper2_buffer_time[looper2_ind])*tempo_cycles;
  //PIT_LDVAL1 = tempo_cycles;
  PIT_TCTRL2 = TIE;
  PIT_TCTRL2 |= TEN;
  PIT_TFLG2 |= 1;
}

void stop_looper2() {
  NVIC_DISABLE_IRQ(IRQ_PIT_CH2);
  looper2_ind = 0;
  for (int i = 0; i < 49; i++) { // stop active keys
    if (looper2_keystate[i] > 0) {
      usbMIDI.sendNoteOff(i+36, 0, looper2_channel);
      looper2_keystate[i] = 0;
    }
  }
}

void pit2_isr(void){
  if (looper2_buffer_keys[looper2_ind] < 49) {
    looper2_keystate[looper2_buffer_keys[looper2_ind]] = 1;
    usbMIDI.sendNoteOn(looper2_buffer_keys[looper2_ind]+36, 99, looper2_channel);  // 60 = C4
  }
  else {
    looper2_keystate[looper2_buffer_keys[looper2_ind]-49] = 0;
    usbMIDI.sendNoteOff(looper2_buffer_keys[looper2_ind]-49+36, 0, looper2_channel);  // 60 = C4
  }

  looper2_ind++;
  if (looper2_ind >= looper2_n) {
    stop_looper2();
    return;
  }

  PIT_LDVAL2 = (16.0*(looper2_buffer_time[looper2_ind]-looper2_buffer_time[looper2_ind-1]))*(tempo_cycles);
  PIT_TCTRL2 = TIE;
  PIT_TCTRL2 |= TEN;
  PIT_TFLG2 |= 1;
  PIT_TFLG2 = 1;
}

void start_looper2_record() {
  looper2_recording = 1;
  looper2_n = 0;
  for (int i = 0; i < 49; i++) { // create events for keys initially held down
    if (keystate[i] > 0) {
      looper2_buffer_keys[looper2_n] = i;
      looper2_buffer_time[looper2_n] = 0.0;
      looper2_n++;
    }
  }
  looper2_record_time = 0;
}

void stop_looper2_record() {
  looper2_recording = 0;
}

/////////////////////////////
// Stuff for Drum Programmer 

#define DRUMPROGONOFF_PIN 6 // MCP
#define DRUMPROGLEFT_PIN 5 // MCP
#define DRUMPROGRIGHT_PIN 7 // MCP

char drumprogonoffstate = 0;
char drumprogonoffstate_prev = 0;
char drumprog_state = 0;

char drumprogleftstate = 0;
char drumprogleftstate_prev = 0;
char drumprogrightstate = 0;
char drumprogrightstate_prev = 0;

char drumprog_pos = 0;
elapsedMillis drumprog_flashtime;

///////////////////////////////
// Stuff for running keyboard

// New pins (board orientation behind keyboard)
char pins_rows[6] = {10,11,12,14,15,16};
char pins_cols[9] = {4,2,0,9,7,5,3,1,6};

void init_keys() {
  for (int i = 0; i < 6; i++) {
    pinMode(pins_rows[i], OUTPUT);
    digitalWrite(pins_rows[i], LOW);
  }
  for (int i = 0; i < 9; i++) {
    pinMode(pins_cols[i], OUTPUT);
    digitalWrite(pins_cols[i], HIGH);
  }
}

// read_keys - Implements switch matrix
void read_keys() {
  int k = 0;
  
  // Read bottom C
  digitalWrite(pins_cols[0], LOW);
  pinMode(pins_rows[5], INPUT_PULLUP);
  delayMicroseconds(10);
  if (digitalRead(pins_rows[5])) {
    keystate[k] = 0;
  }
  else if (keystate_prev[k] > 0) { // keep prev channel data
    keystate[k] = keystate_prev[k];
  }
  else {
    keystate[k] = main_keyboard_channel;
  }
  pinMode(pins_rows[5], OUTPUT);
  digitalWrite(pins_rows[5], LOW);
  k++;
  digitalWrite(pins_cols[0], HIGH);
  
  // Read remaining keys
  for (int i = 1; i < 9; i++) {
    digitalWrite(pins_cols[i], LOW);
    for (int j = 0; j < 6; j++) {
      pinMode(pins_rows[j], INPUT_PULLUP);
      delayMicroseconds(10);
      if (digitalRead(pins_rows[j])) {
        keystate[k] = 0;
      }
      else if (keystate_prev[k] > 0) { // keep prev channel data
        keystate[k] = keystate_prev[k];
      }
      else {
        keystate[k] = main_keyboard_channel;
      }
      pinMode(pins_rows[j], OUTPUT);
      digitalWrite(pins_rows[j], LOW);
      k++;
      delayMicroseconds(500);
    }
    digitalWrite(pins_cols[i], HIGH);
  }
}

//////////////////////////////
// Main routines start here

void setupsimpletune() {
  // put in basic tune for now
  looper_buffer_keys[0] = 0;
  looper_buffer_time[0] = 0;
  
  looper_buffer_keys[1] = 0+49;
  looper_buffer_time[1] = 0.1;
  
  looper_buffer_keys[2] = 2;
  looper_buffer_time[2] = 0.11;
  
  looper_buffer_keys[3] = 2+49;
  looper_buffer_time[3] = 0.3;
  
  looper_buffer_keys[4] = 4;
  looper_buffer_time[4] = 0.5;
  
  looper_buffer_keys[5] = 4+49;
  looper_buffer_time[5] = 0.55;
  
  looper_buffer_keys[6] = 5;
  looper_buffer_time[6] = 0.9;
  
  looper_buffer_keys[7] = 5+49;
  looper_buffer_time[7] = 0.95;
  
  looper_n = 8;
}

void setup() {

  //setupsimpletune(); // create some simple data for the looper to test playback
  
  lcd.init(); // initialize the lcd 
 
  // Print a message to the LCD
  lcd.backlight();
  lcd.home();
  lcd.setCursor(2, 0);
  lcd.print("Teensytune!");
  delay(50);

  // Setup Neopixels and do rainbow
  strip.begin();
  strip.setBrightness(255);
  for (int i = 0; i < 360; i+=2) {
    setPixelHue(0, i);
    setPixelHue(1, i+90);
    strip.show();
    delay(20);
  }
  lcd.setCursor(0, 0);
  lcd.print("Prog:    Loop:  ");
  lcd.setCursor(0, 1);
  lcd.print(instr_names[0]);
  
  // setup pin 13 LED for when keys pressed
  pinMode(13, OUTPUT);

  // Initialise all pins ready for cycling through reading
  init_keys();

  // Initialise GPIO pins
  mcp.begin_special(1);
  
  // center pitchbend/modulation joystick
  pitchbend_center = analogRead(PITCHBEND_PIN);
  modulation_center = analogRead(MODULATION_PIN);
  pitchbend_prev = pitchbend_center;
  modulation_prev = 0;

  // Reset MIDI
  usbMIDI.sendPitchBend(8192, main_keyboard_channel);
  usbMIDI.sendControlChange(1, 0, main_keyboard_channel); 
  usbMIDI.sendProgramChange(main_keyboard_program,main_keyboard_channel);
  
}

void loop() {
  
  // read keys
  read_keys();

  // Check for keyboard input following channel select
  if (chansel_waitingforchoice == 1) {
    for (int i = 0; i < 49; i++) {
      if (keystate[i] > 0) { // grab first key that is on
        //main_keyboard_program = i;
        main_keyboard_program = instr_list[i];
        main_keyboard_program_ind = i;
        usbMIDI.sendProgramChange(main_keyboard_program,main_keyboard_channel);
        chansel_waitingforchoice = 0;
        /*lcd.setCursor(6, 0);
        if (main_keyboard_program < 10) {
          lcd.print(0);
          lcd.print(main_keyboard_program);
        }
        else {
          lcd.print(main_keyboard_program);
        }*/
        lcd.setCursor(0, 1);
        lcd.print("                ");
        lcd.setCursor(0, 1);
        lcd.print(instr_names[main_keyboard_program_ind]);
        break;
      }
    }
  }
  
  // Output midi messages
  int played_note = 0;
  for (int i = 0; i < 49; i++) {
    if (keystate[i] > 0) {
      played_note = 1;
      if (keystate_prev[i] == 0) { // note on
        if ( (drumprog_state == 1) && (i < 10) ) { // change current drum position
          drum_pattern[drumprog_pos] = i;
          usbMIDI.sendNoteOn(drum_instr[drum_pattern[drumprog_pos]], 99, 10);
          usbMIDI.sendNoteOff(drum_instr[drum_pattern[drumprog_pos]], 0, 10);
        }
        else if ( (looper_playback_state == 1) && (i < 12) ) { // root note change to loop
          looper_transpose = i - looper_root_note;
        }
        else {
          usbMIDI.sendNoteOn(i+36, 99, main_keyboard_channel);  // 60 = C4
        }
        if (looper_recording == 1) { // add event to loop recording queue
          looper_buffer_keys[looper_n] = i;
          looper_buffer_time[looper_n] = ((float)looper_record_time/1000.0)/(16.0*(float)tempo_cycles/50000000.0);
          looper_n++;
        }
        if (looper2_recording == 1) { // add event to loop recording queue
          looper2_buffer_keys[looper2_n] = i;
          looper2_buffer_time[looper2_n] = ((float)looper2_record_time/1000.0)/(16.0*(float)tempo_cycles/50000000.0);
          looper2_n++;
        }
      }
    }
    else if (keystate_prev[i] > 0) { // note off
      usbMIDI.sendNoteOff(i+36, 0, keystate_prev[i]);  // 60 = C4
      if (looper_recording == 1) { // add event to loop recording queue
        looper_buffer_keys[looper_n] = i+49;
        looper_buffer_time[looper_n] = ((float)looper_record_time/1000.0)/(16.0*(float)tempo_cycles/50000000.0);
        looper_n++;
      }
      if (looper2_recording == 1) { // add event to loop recording queue
        looper2_buffer_keys[looper2_n] = i+49;
        looper2_buffer_time[looper2_n] = ((float)looper2_record_time/1000.0)/(16.0*(float)tempo_cycles/50000000.0);
        looper2_n++;
      }
    }
  }

  // Update previous states
  for (int i = 0; i < 49; i++) {
    keystate_prev[i] = keystate[i];
  }

  // Run lights
  if (drum_state == 0) {
    if (chansel_waitingforchoice == 1) {
      setPixelHue(0, 300);
      setPixelHue(1, 300);
    }
    else {
      setPixelHue(0, 60);
      setPixelHue(1, 60);
    }
  }
  else {
    if ( (looper_playback_state == 0) && (looper_has_tune == 0) && (looper_record_state == 0) ) {
      if (chansel_waitingforchoice == 1) {
        setPixelHue(1, 300);
      }
      else {
        setPixelHue(1, 60);
      }
    }
    if ( (looper2_playback_state == 0) && (looper2_has_tune == 0) && (looper2_record_state == 0) ) {
      if (chansel_waitingforchoice == 1) {
        setPixelHue(0, 300);
      }
      else {
        setPixelHue(0, 60);
      }
    }
  }

  // Read MCP buttons
  uint8_t pinvals = mcp.readGPIO();
  mcp_button_state[0] = pinvals & 0x1;
  mcp_button_state[1] = pinvals & 0x2;
  mcp_button_state[2] = pinvals & 0x4;
  mcp_button_state[3] = pinvals & 0x8;
  mcp_button_state[4] = pinvals & 0x10;
  mcp_button_state[5] = pinvals & 0x20;
  mcp_button_state[6] = pinvals & 0x40;
  mcp_button_state[7] = pinvals & 0x80;

  // Check button control states
  if (mcp_button_state[CHANSELECT_PIN]) {
    chansel_state = 0;
  }
  else {
    chansel_state = 1;
  }
  if (mcp_button_state[DRUMONOFF_PIN]) {
    drumonoff_state = 0;
  }
  else {
    drumonoff_state = 1;
  }
  if (mcp_button_state[LOOPER_PIN]) {
    looperonoff_state = 0;
    looperonoff_holdtime = 0;
  }
  else {
    looperonoff_state = 1;
    if (looperonoff_state_prev == 0) {
      looperonoff_holdtime = 0;
    }
  }
  if (mcp_button_state[LOOPER2_PIN]) {
    looper2onoff_state = 0;
    looper2onoff_holdtime = 0;
  }
  else {
    looper2onoff_state = 1;
    if (looper2onoff_state_prev == 0) {
      looper2onoff_holdtime = 0;
    }
  }
  
  if (mcp_button_state[DRUMPROGONOFF_PIN]) {
    drumprogonoffstate = 0;
  }
  else {
    drumprogonoffstate = 1;
  }
  if (mcp_button_state[DRUMPROGLEFT_PIN]) {
    drumprogleftstate = 0;
  }
  else {
    drumprogleftstate = 1;
  }
  if (mcp_button_state[DRUMPROGRIGHT_PIN]) {
    drumprogrightstate = 0;
  }
  else {
    drumprogrightstate = 1;
  }
  
  // Check for switch channel
  if ( (chansel_state == 1) && (chansel_state_prev == 0) ) {
    if (chansel_waitingforchoice == 0) {
      chansel_waitingforchoice = 1;
      //strip.setPixelColor(0, 0, 0, 255);
    }
    else {
      chansel_waitingforchoice = 0;
      //strip.setPixelColor(0, 255, 255, 255);
    }
  }

  // Check for drum prog
  if ( (drumprogonoffstate == 1) && (drumprogonoffstate_prev == 0) ) {
    if (drumprog_state == 0) {
      drumprog_state = 1;
      lcd.setCursor(0, 0);
      lcd.print("     Drums:     ");
      lcd.setCursor(0, 1);
      for (int i = 0; i < 16; i++) {
        lcd.print((int)drum_pattern[i]);
      }
    }
    else {
      drumprog_state = 0;
      lcd.setCursor(0, 0);
      lcd.print("Prog:    Loop:  ");
      /*lcd.setCursor(6, 0);
      if (main_keyboard_program < 10) {
        lcd.print(0);
        lcd.print(main_keyboard_program);
      }
      else {
        lcd.print(main_keyboard_program);
      }*/
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(instr_names[main_keyboard_program_ind]);
      if (looper_has_tune == 1) {
          lcd.setCursor(14, 0);
          lcd.print("A");
      }
      if (looper2_has_tune == 1) {
          lcd.setCursor(15, 0);
          lcd.print("B");
      }
      lcd.setCursor(0, 1);
      lcd.print("                ");
    }
  }

  if (drumprog_state == 1) { // drum machine on
    lcd.setCursor(drumprog_pos, 1);
    if (drumprog_flashtime < 500) {
      lcd.print((int)drum_pattern[drumprog_pos]);
    }
    else {
      lcd.print(" ");
    }
    if ( (drumprogleftstate == 1) && (drumprogleftstate_prev == 0) ) { // move ind left
      lcd.setCursor(drumprog_pos, 1);
      lcd.print((int)drum_pattern[drumprog_pos]);
      if (drumprog_pos == 0) {
        drumprog_pos = 15;
      }
      else {
        drumprog_pos--;
      }
    }
    if ( (drumprogrightstate == 1) && (drumprogrightstate_prev == 0) ) { // move ind right
      lcd.setCursor(drumprog_pos, 1);
      lcd.print((int)drum_pattern[drumprog_pos]);
      if (drumprog_pos == 15) {
        drumprog_pos = 0;
      }
      else {
        drumprog_pos++;
      }
    }
  }
  
  // Check for drum machine

  // switch on/off
  if ( (drumonoff_state == 1) && (drumonoff_state_prev == 0) ) {
    if (drum_state == 0) {
      drum_state = 1;
      drum_ind = 0;
      chansel_waitingforchoice = 0; // turn of channel select, if on
      strip.setPixelColor(0, 255, 255, 255);
      start_drums();
    }
    else {
      NVIC_DISABLE_IRQ(IRQ_PIT_CH0);
      drum_state = 0;
      digitalWrite(13, LOW);
    }
  }

  // Check for looper state change
  if (looperonoff_holdtime > 1000) { // holding button to wipe tune
    looper_has_tune = 0;
    looper_playback_state = 0;
    lcd.setCursor(14, 0);
    lcd.print(" ");
  }
  if ( (looperonoff_state == 0) && (looperonoff_state_prev == 1) ) { // look for button release
    if (looper_has_tune == 1) { // has a loaded tune
      if (looper_playback_state == 0) {
        looper_playback_state = 1; // start play back on next beat
      }
      else {
        stop_looper(); // stop playback for now
        looper_playback_state = 0;
      }
    }
    else { // no tune loaded
      if (looper_record_state == 0) {
        looper_record_state = 1;
      }
      else {
        stop_looper_record();
        looper_record_state = 0;
      }
    }
  }

  // Check for looper state change (looper B)
  if (looper2onoff_holdtime > 1000) { // holding button to wipe tune
    looper2_has_tune = 0;
    looper2_playback_state = 0;
    lcd.setCursor(15, 0);
    lcd.print(" ");
  }
  if ( (looper2onoff_state == 0) && (looper2onoff_state_prev == 1) ) { // look for button release
    if (looper2_has_tune == 1) { // has a loaded tune
      if (looper2_playback_state == 0) {
        looper2_playback_state = 1; // start play back on next beat
      }
      else {
        stop_looper2(); // stop playback for now
        looper2_playback_state = 0;
      }
    }
    else { // no tune loaded
      if (looper2_record_state == 0) {
        looper2_record_state = 1;
      }
      else {
        stop_looper2_record();
        looper2_record_state = 0;
      }
    }
  }
  
  // Check for tempo change
  tempo = analogRead(TEMPO_PIN);
  if (tempo != tempo_prev) {
    float freq = 10*(tempo/1024.0) + 0.5;
    tempo_cycles = (1.0/freq)*50000000;
    new_tempo = 1;
  }

  // Look for pitch bend and modulation controls
  pitchbend = analogRead(PITCHBEND_PIN);
  modulation = analogRead(MODULATION_PIN);
  if (abs(pitchbend - pitchbend_prev) > 50) {
    double pitchbend_f = (pitchbend-pitchbend_center)/512.0;
    unsigned int pitchval = (pitchbend_f+1.f)*8192;
    if (pitchval > 16383) pitchval = 16383;
    usbMIDI.sendPitchBend(pitchval, main_keyboard_channel);
    pitchbend_prev = pitchbend;
  }
  if (modulation > 700) {
    if (modulation_prev == 0) {
      usbMIDI.sendControlChange(1, 127, main_keyboard_channel);
    }
    modulation_prev = 1;
  }
  else {
    if (modulation_prev == 1) {
      usbMIDI.sendControlChange(1, 0, main_keyboard_channel);
    }
    modulation_prev = 0;
  }
  
  // Update previous states
  chansel_state_prev = chansel_state;
  drumonoff_state_prev = drumonoff_state;
  tempo_prev = tempo;
  looperonoff_state_prev = looperonoff_state;
  looper2onoff_state_prev = looper2onoff_state;
  drumprogonoffstate_prev = drumprogonoffstate;
  drumprogleftstate_prev = drumprogleftstate;
  drumprogrightstate_prev = drumprogrightstate;

  if (drumprog_flashtime > 1000) {
    drumprog_flashtime = 0;
  }
  
  // Update neopixel
  strip.show();
  
}



