# irlock

Description
-----------
This is an Arduino program for locking and unlocking a door via infrared. I reccomend the [irplus Android app](https://play.google.com/store/apps/details?id=net.binarymode.android.irplus).

For devices equipped with an infrared **receiver**, irlock can transmit the unlock code in an infrared LED to be recorded by the irplus app. (Other apps with IR recording capabilities may also work.) For devices not equipped with an IR receiver, irlock can generate and print to Serial Monitor a file in the irplus-PRONTO HEX format.

Notices
-------

* I can only test this on the Arduino Uno R3. Other boards should work, but some won't. See [https://github.com/MTres19/irlock/wiki/Flash-irlock-to-Arduino](https://github.com/MTres19/irlock/wiki/Flash-irlock-to-Arduino) for a list of boards that probably won't work.

* Ken Shirriff's IR library must be installed. See [https://github.com/z3t0/Arduino-IRremote/releases](https://github.com/z3t0/Arduino-IRremote/releases) for downloads. Download `Arduino-IRremote-dev.zip`, then in the  Arduino IDE, go to Sketch > Include Library > Add .ZIP Library, and open the ZIP file. If this results in extra `#include` statements being added to the sketch, remove them, as the library is already included.

* You should be using the latest version of the Arduino IDE (v1.6.8 as of this writing).

* The IR LED **must** be connected to Arduino pin 3. No other pin is supported for sending with Ken Shirriff's library.

* You can view the (rather messy) schematic file, irlock.fzz, by downloading and opening it in [Fritzing](http://fritzing.org).

Releases
--------

Currently, there are no releases as I have not tested it.

Technical Details
-----------------

EEPROM is used for storing lock status and password. If you're the type to unlock or lock the door 100,000 times, then the first byte of EEPROM might wear out. Same with the password.

The password is stored as a String object, because it is a single variable (not a char array) and can easily be used with EEPROM.get().

Passwords are shortened to 2 characters and converted to a 16-bit integer in the loadVarsFromEEPROM() function for quick sending and less chance of mistakes.

Codes are sent in the Sony SIRC protocol, so to generate PRONTO HEX for an irplus file, a for loop and bitwise operations are used to go bit-by-bit through the password int and output the corrosponding PRONTO words.
