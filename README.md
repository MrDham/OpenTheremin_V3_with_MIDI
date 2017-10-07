## Open Theremin V3 with MIDI interface control software for Arduino UNO 

Based on Arduino UNO Software for the Open.Theremin version 3.0  Copyright (C) 2010-2016 by Urs Gaudenz
https://github.com/GaudiLabs/OpenTheremin_V3

### Don't click on the files!
Click on the "Download ZIP" Button to the right or [Click here](https://github.com/GaudiLabs/OpenTheremin_V3/archive/master.zip) 
Then unpack the archive.

### Open Source Theremin based on the Arduino Platform

Open.Theremin is an arduino shield to build the legendary music instrument invented by Leon Theremin back in 1920. The theremin is played with two antennas, one to control the pitch and one for volume. The electronic shield with two ports to connect those antennas comprises two heterodyne oscillators to measure the distance of the hand to the antenna when playing the instrument. The resulting signal is fed into the arduino. After linearization and filtering the arduino generates the instruments sound that is then played through a high quality digital analog audio converter on the board. The characteristics of the sound can be determined by a wave table on the arduino.

For more info on the open source project and on availability of ready made shield see:

http://www.gaudi.ch/OpenTheremin/

### Installation
1. Open up the Arduino IDE
2. Open the File "Open_Theremin_V3.ino"
3. Selecting the correct usb port on Tools -> Serial Port
4. Select the correct arduino board from Tools -> Board
5. Upload the code by clicking on the upload button.

### Added and removed compare to Open Theremin V3. 
Serial communication implemented for programm monitoring purpose was removed (Particularly during calibration).
If you need to monitor calibration for antenna problem fixing, please use original master branch from 
https://github.com/GaudiLabs/OpenTheremin_V3
Serial port is used to send midi messages now. 

### How does it works ? 
PITCH : 
It uses first note detected at volume rise to generate a NOTEON. 
Then it uses PITCHBEND to reach pitch as long as pitch bend range will do. 
Beyond it generates a new NOTEON  followed by a NOTEOFF for the previous note (legato). 
Pitch bend range can be configured (1, 7, 12 or 24 semitones)
One exception is that I desactivated pitch bend in 1 semitone mode because portamento does a better job then. 

VOLUME: 
It generates VOLUME continuous controler, starting NOTEON and ending NOTE OFF (when playing stacato). 
The trigger volume can be configured so as we have some volume at note attack on percussive sounds. 

CONFIGURATION: 
There is two calibration mode. 
If REGISTER POT turned counter clockwise at entering in calibration mode 
         -> Runs normal calibration of antennas
If REGISTER POT turned clockwise at entering in calibration mode 
         -> Records midi settings as per pot position BEFORE entering in calibration mode: 
  VOLUME POT : sets volume trigger level
  PITCH POT : sets pitch bend range (1, 7, 12 or 24 semitones)
  TIMBRE POT : sets Channel. In the absence of graduation, timbre variation may help 
               (Wave Form 1 low = CH1, WF 1 High = CH2, WF 2 Low = CH3, etc...)
               
MUTE BUTTON: 
Sends ALL NOTE OFF on selected channel and stay in mute until it's pushed again.  




### LICENSE
Original project written by Urs Gaudenz, GaudiLabs, 2016
GNU license, check LICENSE file for more information
All text above must be included in any redistribution

