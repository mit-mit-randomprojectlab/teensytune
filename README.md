# teensytune
A homebuilt MIDI controller/keyboard built around an old broken keyboard using the Teensy microcontroller

By mit-mit

This code runs on a Teensy 3.1 and connects to a 49 key piano keyboard using a switch matrix layout
and several input controls (including buttons, a potentiometer, LCD two-line character display, 
two-axis joystick and neopixels) and outputs MIDI messages across the Teensy USB.

For details on the hardware and contruction process, see:
http://randomprojectlab.blogspot.com.au/2017/12/teensytune-teensy-based-midi.html

Required libraries:

Adafruit Neopixels (used to run neopixels):
https://github.com/adafruit/Adafruit_NeoPixel

Adafruit MCP23008 (used for port expander, providing sufficient button inputs):
https://github.com/adafruit/Adafruit-MCP23008-library

LiquidCrystal_I2C (used to drive the two-line display):
https://www.dfrobot.com/wiki/index.php/I2C/TWI_LCD1602_Module_(SKU:_DFR0063)
