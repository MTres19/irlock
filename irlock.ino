/*

Infrared unlock---Program lock codes via Serial, save lock code in EEPROM, calibrate phone remote apps via IR LED, and unlock door via IR sensor and servo

This program is designed for Arduino Uno (Revision 3).
This program was tested with LG's QuickRemote and binarymode's irplus apps on an LG G4. Galaxy S6s don't seem to work, as they lack an IR receiver.
Pin directions below:

LCD Screen
------------
1. Ground (VSS): Ground
2. Power 5V (VDD): 5V
3. Contrast (V0): Middle pin of potentiometer, or voltage divider like this:

              1K Ohm       150 Ohm
Vin (+5V) ---/\/\/\/-------/\/\/\/--- GND (0V)
                      |
                      |
              Vout (LCD Pin 3: V0)

4. Register Select (RS): Arduino Digital Pin 8
5. Read/Write (RW): Ground
6. Enable (E): Arduino Digital Pin 9
7. Data 0 (D0): Disconnected
8. Data 1 (D1): Disconnected
9. Data 2 (D2): Disconnected
10. Data 3 (D3): Disconnected
11. Data 4 (D4): Arduino Digital Pin 10
12. Data 5 (D5): Arduino Digital Pin 11
13. Data 6 (D6): Arduino Digital Pin 12
14. Data 7 (D7): Arduino Digital Pin 13
15. Backlight Power 5V (A): 5V through 220 Ohm Resistor
16. Backlight Ground (K): Ground

Servo
------
1. Ground (Brown): Common Ground
2. Power (Red): 5-6V
3. Signal (Orange): Arduino Digital Pin 5

Infrared LED
--------------
Long Pin: Arduino Digital Pin 3 through 220 Ohm Resistor
Short Pin: Ground

Infrared Sensor
-----------------
(When facing side with round sensor)
Right Pin: 5V
Center Pin: Ground
Left Pin: Arduino Digital Pin 4

Buttons
--------
Connect an inverting Schmitt Trigger to power and ground

Lock/Unlock Button
--------------------
Connect a 10K Ohm pullup resistor.
Connect a 10uF capacitor across the button.
Connect the side with the pullup resistor to an input pin on the inverting Schmitt Trigger.
Connect the corrosponding output pin of the inverting Schmitt Trigger to digital pin 2 on the Arduino.
Connect the other side of the button to ground.

Calibrate Smartphone Button
----------------------------
Connect a 10K Ohm pullup resistor.
Connect a 10uF capacitor across the button.
Connect the side with the pullup resistor to an input pin on the inverting Schmitt Trigger.
Connect the corrosponding output pin of the inverting Schmitt Trigger to digital pin 6 on the Arduino.
Connect the other side of the button to ground.

Program Arduino Button
-----------------------
Connect a 10K Ohm pullup resistor.
Connect a 10uF capacitor across the button.
Connect the side with the pullup resistor to an input pin on the inverting Schmitt Trigger.
Connect the corrosponding output pin of the inverting Schmitt Trigger to digital pin 7 on the Arduino.
Connect the other side of the button to ground.
*/

// Include libraries
#include <LiquidCrystal.h>
#include <Servo.h>
#include <IRremote.h>
#include <EEPROM.h>

// Constants
#define lockedPosition 180 // Position of the servo, in degrees, when the door is locked. Change as circumstances warrent.
#define unlockedPosition 90 // Position of the servo, in degrees, when the door is unlocked. Change as circumstances warrent.

// Make LiquidCrystal variable named lcd, identify pins being used
LiquidCrystal lcd(8, 9, 10, 11, 12, 13);

// Make Servo variable named servo
Servo servo;

// Make a IRsend variable named irLED
IRsend irLED;

// Make an IRrecv variable named detector connected to pin 4
IRrecv detector(4);

// Make a decode_results variable named irInput
decode_results irInput;

// Variables
bool isLocked; // Stores whether the door is locked or not

// Variables for converting password to integer datatype---note that using short makes the number 16 bits, maintaining compatability with Arduino Due
char passwordCharArray[3]; // First two characters will be used for the password, the third holds an auxilary character that could be an unneeded carriage return
short firstChar;
short secondChar;
short passwordShort;

// Variables for converting to PRONTO HEX
short mask;
short masked_passwordShort;
short theBit;

void convertPassword()
{
  // Convert individual characters to integers
  firstChar = passwordCharArray[0];
  secondChar = passwordCharArray[1];
  
  // Multiply the integers together to get a number based on the password. It doesn't actually matter if what is used as the password matches
  // what the user entered byte-wise, since the user doesn't have to enter it.
  passwordShort = firstChar * secondChar;
}

void loadVarsFromEEPROM()
{
  // Get lock status (stored in address 0)
  EEPROM.get(0, isLocked);
  // Get password (stored beginning in address 1)
  EEPROM.get(1, passwordShort);
}

void generateIrplusFile()
{
  Serial.println(F("An irplus file will be generated."));
  Serial.println(F("Copy and paste the output below into a text editor, save it as irlock.irplus, and import it on any Android device with the irplus app installed."));
  Serial.println(F("Generating. Please wait..."));
  // Print two empty lines
  Serial.println("");
  Serial.println("");

  // Print irplus header
  Serial.println(F("<irplus>"));
  // Placing a backslash before the quote mark makes it print as part of the message. (The backslash is not printed.)
  Serial.println("<device manufacturer=\"Imagination\" model=\"irlock beta\" columns=\"1\" format=\"PRONTO_HEX\">");

  // Print <button> tag (notice this is not println()!)
  Serial.print("<button label=\"lock/unlock\">");

  // Print the first four PRONTO words (these are basically metadata---first one = raw code, second one = carrier frequency---not kHz, based on PRONTO hardware timers or something,
  // third one = number of "burst pairs" in sequence 1---16 in this case, because 16 bit number being sent, fourth one = number of burst pairs in sequence 2---none in this case
  Serial.print(F("0000 006D 0010 0000 "));
  // Print the next two PRONTO words (these are the (Sony) initiation pulse and won't change)
  Serial.print(F("0060 0018 "));
  
  // Cycle through the bits of passwordShort, printing the "1" PRONTO code if it's a one and a "0" PRONTO code if it's a zero.
  // Note that despite official Sony spec being little endian, Ken Shirriff's library uses big endian.
  // However, it is not necessary to convert from the Arduino's little endian format to big endian for transmission, since bitwise operations act big-endian.
  for (int i = 15; i >= 0; i--)
  {
    // Create a mask to apply to passwordShort, basically moving the the binary "1" at the end of the int "i" positions to the left
    mask = 1 << i;
    // "Mask" passwordShort in an AND operation, that is, anywhere where mask has a zero, passwordShort will also have a zero, and anywhere where mask has a 1, passwordShort stays the same. Since mask only has one one,
    // only one position will be guarenteed to be the same as passwordShort. passwordShort will not change in value itself.
    masked_passwordShort = passwordShort & mask;
    // Move the bit that was extracted from passwordShort down to the end so that we can tell if it's a one or a zero
    theBit = masked_passwordShort >> i;
    // We have the bit! If it's a one, then theBit will equal 1, if it's a zero, theBit will equal 0!

    // If the bit is a 1, send a (Sony) PRONTO 1
    if (theBit == 1)
    {
      Serial.print(F("0030 0018"));
      // A space following the number is only needed when not on the last bit
      if (i > 0)
      {
        Serial.print(" ");
      }
    }
    // Otherwise, send a (Sony) PRONTO 0
    else
    {
      Serial.print(F("0018 0018"));
      // A space following the number is only needed when not on the last bit
      if (i > 0)
      {
        Serial.print(" ");
      }
    }
    // Interestingly, Sony specification calls for a longer time off at the end of a transmission. However, because this generator does not create a code to be sent when the button is held down, and human reaction time
    // is probably longer than the amount of time called for in the spec, it is simpler to just omit this.
  }
  
  // Print the closing <button> tag
  Serial.println(F("</button>"));
  
  // Print closing irplus and device tags
  Serial.println(F("</device>"));
  Serial.println(F("</irplus>"));

  // Print two empty lines
  Serial.println("");
  Serial.println("");
  Serial.println(F("Generating complete."));
}

void programPasswordAndStatus()
{
  // Tell the user to connect board to computer
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Press Prgm when");
  lcd.setCursor(0, 1);
  lcd.print("connected to PC.");

  // Wait for user to release the button
  while (digitalRead(7) == HIGH)
  {
  }
  
  // Wait for button press
  while (digitalRead(7) == LOW)
  {
  }
  // Open USB/serial connection
  Serial.begin(9600);
  // Wait for USB port to connect---needed for Arduinos with native USB only
  while (!Serial)
  {
  }
  
  // Tell the user to use Serial Monitor to complete the rest of the process
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("Follow cues in");
  lcd.setCursor(1, 1);
  lcd.print("Serial Monitor.");

  // Announce successful serial connection
  Serial.println(F("Serial connection successfully established."));

  // Get lock status
  Serial.println(F("Is door in a locked position? [y/n]"));
  // Wait for a response
  while (!Serial.available())
  {
  }
  // Wait for entire message to arrive
  delay(200);
  
  // ASCII character "y" is equivalent to the decimal number 121. Use peek() because if using read, it will be erased from the buffer and can't be read by the else if statement next
  if (Serial.peek() == 121)
  {
    isLocked = true;
    // Inform the user that it is being written
    Serial.print(F("Writing answer... "));
    // Write value to EEPROM
    EEPROM.put(0, isLocked);
    // Inform the user that writing is done
    Serial.println(F("Done!"));
    // Inform the user that the servo is being positioned
    Serial.print(F("Positioning servo... "));
    // Move servo to position set by lockedPosition variable
    servo.write(lockedPosition);
    // Inform the user that positioning is done
    Serial.println(F("Done!"));
  }
  // ASCII character "n" is equivalent to the decimal number 110.
  else if (Serial.peek() == 110)
  {
    isLocked = false;
    // Inform the user that it is being written
    Serial.print(F("Writing answer... "));
    // Write value to EEPROM
    EEPROM.put(0, isLocked);
    // Inform the user that the writing is done
    Serial.println(F("Done!"));
    // Inform the user that the servo is being positioned
    Serial.print(F("Positioning servo... "));
    // Move servo to position set by unlockedPosition variable
    servo.write(unlockedPosition);
    // Inform the user that positioning is done
    Serial.println(F("Done!"));
  }
  // If it wasn't one or the other, then the user entered an invalid character.
  else
  {
    Serial.println(F("Sorry. That character is invalid. Please use a lowercase n for no, or lowercase y for yes. Programming will end."));
    Serial.println(F("Closing serial connection..."));
    // Close serial connection
    Serial.end();
    // Exit function
    return;
  }
  
  // Clear the Serial buffer, since peek() doesn't empty it, but read() does
  while (Serial.available() > 0)
  {
    Serial.read();
  }
  
  // Get password via Serial
  Serial.println(F("Please enter a password. Make sure it is at least three characters."));
  // Wait for a response
  while (!Serial.available())
  {
  }
  // Wait for entire message to arrive
  delay(200);

  // Read password into passwordCharArry
  for (int i = 0; i < 3; i++)
  {
    passwordCharArray[i] = Serial.read();
  }
  // Make sure the password is longer than 2 characters, otherwise a carriage return character could sneak in (less possibilities = less secure)
  if (strlen(passwordCharArray) < 3)
  {
    Serial.println(F("Sorry. That password is not long enough. Programming will end."));
    Serial.println(F("Closing serial connection..."));
    // Close serial connection
    Serial.end();
    // Exit function
    return;
  }

  // Inform the user that the password is being converted
  Serial.print(F("Converting password format... "));
  // Generate an integer based on the password
  convertPassword();
  // Inform the user that convertion is done
  Serial.println(F("Done!"));

  // DEBUGGING: Print HEX format of password
  Serial.print(F("The password is: 0x"));
  Serial.println(passwordShort, HEX);
  
  // Inform the user that it is being written
  Serial.print(F("Writing answer... "));
  // Write value to EEPROM
  EEPROM.put(1, passwordShort);
  // Inform the user that the writing is done
  Serial.println(F("Done!"));
  
  // Ask if the user would like to generate a file for irplus app
  Serial.println(F("Would you like to have a file generated for use with the irplus app on phones without an IR receiver? [y/n]"));
  // Wait for a response
  while (!Serial.available())
  {
  }
  // ASCII character "y" is equivalent to the decimal number 121.
  if (Serial.read() == 121)
  {
    generateIrplusFile();
  }
  // Otherwise...
  else
  {
    Serial.println(F("An irplus file will not be generated."));
  }
  
  Serial.println(F("Programming successful!"));
  Serial.println(F("Closing serial connection..."));
  // Close serial connection
  Serial.end();
  
  // Show the home screen
  showHomeScreen();
}

void showHomeScreen()
{
  // Clear LCD
  lcd.clear();
  // Position the cursor at the top left (not really neccessary, since clear() does this already)
  lcd.setCursor(0, 0);
  // Print name and version number
  lcd.print("irlock beta");
  // Position the cursor at the bottom left
  lcd.setCursor(0, 1);
  // If the door is unlocked, print "unlocked", otherwise...
  if (isLocked == false)
  {
    lcd.print("Status: unlocked");
  }
  else
  {
    lcd.print("Status: locked");
  }
}

void calibrateSmartphone()
{
  // Clear LCD
  lcd.clear();
  // Position cursor at top left-ish
  lcd.setCursor(1, 0);
  // Print message
  lcd.print("Prepare phone");
  // Position cursor at bottom left
  lcd.setCursor(0, 1);
  // Print message
  lcd.print("for calibration");
  // Wait 1.5 seconds
  delay(1500);

  // Clear LCD
  lcd.clear();
  // Do a countdown
  lcd.setCursor(0, 0);
  lcd.print("Calibrating in:");
  lcd.setCursor(0, 1);
  lcd.print("3");
  delay(1000);
  lcd.setCursor(0, 1);
  lcd.print("2");
  delay(1000);
  lcd.setCursor(0, 1);
  lcd.print("1");
  delay(1000);
  // End countdown
  
  // Clear LCD
  lcd.clear();
  // Set cursor at top left
  lcd.setCursor(0, 0);
  // Print calibration message
  lcd.print("Calibrating...");
  
  // Send IR password
  irLED.sendSony(passwordShort, 16);
  
  // Re-enable the IR receiver after sending (https://github.com/z3t0/Arduino-IRremote/issues/143)
  detector.enableIRIn();
  
  // Clear LCD
  lcd.clear();
  // Set cursor and print success message
  lcd.setCursor(2, 0);
  lcd.print("Code sent.");
  // Show message for a second and a half
  delay(1500);

  // Show home screen
  showHomeScreen();
}

void lock()
{
  // Move servo to position set by lockedPosition variable
  servo.write(lockedPosition);
  // Set variable to locked
  isLocked = true;
  // Write status to EEPROM (stored in address 0)
  EEPROM.put(0, isLocked);
  // Refresh home screen to show new status
  showHomeScreen();
}

void unlock()
{
  // Move servo to position set by unlockedPosition variable
  servo.write(unlockedPosition);
  // Set variable to unlocked
  isLocked = false;
  // Write status to EEPROM (stored in address 0)
  EEPROM.put(0, isLocked);
  // Refresh home screen to show new status
  showHomeScreen();
}

void setup()
{
  // Initialize the servo on pin 5
  servo.attach(5);
  
  // Intialize 16x2 LCD
  lcd.begin(16, 2);
  // Hide cursor
  lcd.noCursor();
  
  // Load password and lock setting from EEPROM.
  loadVarsFromEEPROM();
  
  // DEBUGGING: Send out the password in hex when starting
  Serial.begin(9600);
  while (!Serial)
  {
  }
  Serial.print(F("The passcode is: 0x"));
  Serial.println(passwordShort, HEX);
  Serial.end();
  
  // Make sure the lock is in the proper position
  if (isLocked == true)
  {
    servo.write(lockedPosition);
  }
  else
  {
    servo.write(unlockedPosition);
  }
  
  // Start the IR receiver
  detector.enableIRIn();

  // Set the LED pin as an output
  pinMode(3, OUTPUT);
  
  // Show the home screen
  showHomeScreen();
}

void loop()
{
  // If something has been decoded, decode() produces something other than 0. The ampersand (&) before irInput means that decode() will have write access to that variable.
  if (detector.decode(&irInput) != 0)
  {
    // If the value from the IR decoding is the password...
    if (irInput.value == passwordShort)
    {
      if (isLocked == true)
      {
        unlock();
      }
      else
      {
        lock();
      }
    }

    // Resume IR reception
    detector.resume();
  }
  
  // Pin inputs will be HIGH  when a button is pressed because the inverting Schmitt Trigger changes a HIGH from the pullup resistor to LOW and a LOW to HIGH.
  
  // If the lock/unlock button was pressed...
  if (digitalRead(2) == HIGH)
  {
    // Wait for the user to release the button
    while (digitalRead(2) == HIGH)
    {
    }
    
    if (isLocked == true)
    {
      unlock();
    }
    else
    {
      lock();
    }
  }
  
  // If the calibrate smartphone button was pressed...
  if (digitalRead(6) == HIGH)
  {
    calibrateSmartphone();
  }

  // If the program Arduino button was pressed...
  if (digitalRead(7) == HIGH)
  {
    programPasswordAndStatus();
  }
}
