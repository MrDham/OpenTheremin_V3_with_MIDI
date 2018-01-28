## Open Theremin V3 with MIDI interface control software V2.0 for Arduino UNO 


Based on Arduino UNO Software for the Open.Theremin version 3.0  Copyright (C) 2010-2016 by Urs Gaudenz
https://github.com/GaudiLabs/OpenTheremin_V3

Urs also made a very clear presentation of previous version V1.0 of this MIDI feature on his website: http://www.gaudi.ch/OpenTheremin/index.php?option=com_content&view=article&id=200&Itemid=121, many thanks ! Since then, the software has been enhanced to V2.0 but there is still good applicable tips for the installation. 

### Don't click on the files!
Click on the "clone or download" Button to the right. Then unpack the archive.

### Open Source Theremin based on the Arduino Platform

Open.Theremin is an arduino shield to build the legendary music instrument invented by Leon Theremin back in 1920. The theremin is played with two antennas, one to control the pitch and one for volume. The electronic shield with two ports to connect those antennas comprises two heterodyne oscillators to measure the distance of the hand to the antenna when playing the instrument. The resulting signal is fed into the arduino. After linearization and filtering the arduino generates the instruments sound that is then played through a high quality digital analog audio converter on the board. The characteristics of the sound can be determined by a wave table on the arduino.

For more info on the open source project and on availability of ready made shield see:

http://www.gaudi.ch/OpenTheremin/

### Installation
1. Open up the Arduino IDE
2. Open the File "Open_Theremin_V3.ino"
3. In "Application.cpp", take care of selecting MIDI mode that correponds to your cituation (put "//" in front off inadequate line - MIDI through serial is selected by default here):

   Serial.begin(115200); // Baudrate for midi to serial. Use a serial to midi router http://projectgus.github.com/hairless-midiserial/
  
   //Serial.begin(31250); // Baudrate for real midi. Use din connection https://www.arduino.cc/en/Tutorial/Midi or HIDUINO https://github.com/ddiakopoulos/hiduino

   I tested "Hiduino" and "MIDI to serial" modes, both are OK.
   
4. Selecting the correct usb port on Tools -> Serial Port
5. Select the correct arduino board from Tools -> Board
6. Upload the code by clicking on the upload button.

### Added and removed compare to Open Theremin V3. 
Serial communication implemented for program monitoring purpose was removed (Particularly during calibration).
If you need to monitor calibration for antenna problem fixing, please use original master branch from 
https://github.com/GaudiLabs/OpenTheremin_V3. 

Serial port is used to send MIDI messages now. 

### How does it work ? 

The MIDI open theremin generates NOTE ON/OFF messages and  Continuous Controler changes (MIDI CC) depending on settings and hands positions next to antennas. 


MIDI CC: 

It is possible to affect independant MIDI CCs to the PITCH ANTENNA (ROD) and to the VOLUME ANTENNA (LOOP).  

NOTE ON/OFF: 

In MIDI standard NOTE ON/OFF messages have a NOTE NUMBER and a VELOCITY. 

Let's consider a Fade-in / Picth Variation / Fade-out sequence (I use right handed convention): 

1. Fade-In

   When left hand moves away from VOLUME ANTENNA (LOOP) and volume crosses a settable threshold (Volume trigger), a NOTE ON is generated. VELOCITY depends on how fast left hand is moving. Right hand's position next to PITCH ANTENNA (ROD) determines the starting NOTE NUMBER. 


2. Pitch variation

   When right hand moves next to PITCH ANTENNA (ROD), PITCH BEND messages are generated (if activated) to reach exact pitch as long as pitch bend range will do.  Beyond, a new NOTE ON followed by a NOTE OFF for the previous note are generated if legato mode is activated. Pitch bend range can be configured (1, 2, 7, 12 or 24 semitones) to align with synth's maximum capabilities. 

3. Fade-Out

   When left hand moves close to VOLUME ANTENNA (LOOP) and volume goes under Volume trigger threshold, a NOTE OFF is generated to mute the playing note. 

  
 SETTINGS:
 
 "Register" pot becomes "Selected Parameter" pot and have 8 positions. 
  "Timbre" pot becomes "Parameter's Value" and have a variable number of positions depending on selected parameter: 
 
 1. Register: 4 positions as in original Open Theremin V3
 2. Timbre: 8 positions as in original Open Theremin V3
 3. Channel: 16 positions (channel 1 to 16)
 4. Rod antenna mode: 4 positions 
     (Legato off/Pitch Bend off, Legato off/Pitch Bend on, Legato on/Pitch Bend off, Legato on/Pitch Bend on)
 5. Pitch bend range: 5 positions (1, 2, 7, 12, 24 Semitones). 
     For classical glissando and in order to have same note on audio and MIDI, use exactly same pitch bend range on your synth. 
     Maximum setting possible is recomended.
 6. Volume trigger / Velocity sensitivity (how fast moves the volume loop's hand): 128 positions (0 to 127)
 7. Rod antenna MIDI CC: 5 positions 
    (None, 8-Balance, 10-Pan, 16-Ribbon controler, 74-cutoff)
 8. Loop antenna MIDI CC: 8 positions 
    (1-Modulation, 7-Volume, 11-Expression, 71-Resonnance, 74-Cutoff, 91-Reverb, 93-Chorus, 95-Phaser)

Select a Parameter and move "Parameter's Value" to change corresponding setting. 

The picture at https://github.com/MrDham/OpenTheremin_V3_with_MIDI/blob/master/MIDI%20Open%20Theremin%20V3%20HMI.bmp gives an example of possible HMI: on "Value" pot, red lines have 4 positions, grey lines have 5 positions and yellow lines have 8 positions. On "Parameter" pot you see coloured lines indicating which colour to follow for the "Value" pot. 

The volume trigger can be configured so as we have some volume at note attack on percussive sounds. 
The volume trigger setting is also used to set sensitivity for velocity (how fast left hand is moving when note is triggered). 
Volume trigger = 127 (Maximum) won't generate any NOTE ON. It can be used to generate MIDI CC only.

Manipulation of "Rod antenna MIDI CC" and "Loop antenna MIDI CC" is not error proof. MIDI newbies should be advised to change their value in MUTE mode. 

 
Default configuration is: Register = Lowest Register, Timbre = 1st Waveform, Channel = MIDI Channel 1, Rod antenna mode = Legato on/Pitch Bend on, Pitch bend range = 2 Semitones, Volume trigger = 0, Rod antenna MIDI CC = None, Loop antenna MIDI CC = 7-Volume. 


MUTE BUTTON: 

Sends ALL NOTE OFF on selected channel and stay in mute until it is pushed again.  

AUDIO: 

Audio processing from antennas to output jack, including volume and pitch pots, LEDs and button functions, is exactly the same as in open theremin V3.  You can play the Audio and the MIDI side by side. 

CALIBRATION:

This device runs normal calibration of antennas after pushing button for 3 seconds as per initial project

### What can I do to get a theremin like glissando?

Activate picth bend and set pitch bend range of the theremin with a high value (12 semitones or 24 semitones).
Set pitch bend range of the synth with the same value. 


### If I do not trigger with the volume hand it also seems to trigger a new tone with the pitch antenna. Guess this is how MIDI works.

When legato mode is activated, if you trigger a note (with volume loop) and go in one direction (with pitch antenna) a new note will be triggered at the limit of pitch bend range. 

Legato mode is used as a workaround for a limitation of MIDI (max 24 semitones pitch bend). Maybe some synth can perform pitch bend on more that 2 octaves but none of mine does...


### Tweakable parameters (in application.cpp):
Changing this to your taste may require some test and trial. 

"#define VELOCITY_SENS  9" -> How easy it is to reach highest velocity (127). Something betwen 5 and 12.  

"#define PLAYER_ACCURACY  0.2"  -> Pitch accuracy of player. Tolerance on note center for changing notes when playing legato. From 0 (very accurate players) to 0.5 (may generate note toggling). 



### LICENSE
Original project written by Urs Gaudenz, GaudiLabs, 2016
GNU license. This Project inherits this 2016 GNU License. 

 Check LICENSE file for more information

All text above must be included in any redistribution

