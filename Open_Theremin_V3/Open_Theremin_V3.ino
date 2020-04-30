/*
 *  Open Theremin V3 with MIDI interface control software for Arduino UNO 
 *  Based on Open Theremin V3 version 3.0  Copyright (C) 2010-2016 by Urs Gaudenz
 *   
 *  Also integrate changes from Open Theremin V3 Version 3.1  Copyright (C) 2010-2020 by Urs Gaudenz
 *
 *
 *  Open Theremin V3 with MIDI interface control software is free software: 
 *  you can redistribute it and/or modify it under the terms of 
 *  the GNU General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Open Theremin V3 with MIDI interface control software is distributed 
 *  in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
 *  without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  the Open Theremin V3 with MIDI interface control software.  
 *  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Also credited for their important contributions to Open Theremin V3: 
 *  David Harvey
 *  Michael Margolis
 *  "Theremingenieur" Thierry Frenkel
 */

/* Midi added by Vincent Dhamelincourt - September 2017. 
 * Serial com' removed from the original Open Theremin V3 's code for midi purpose. 
 */
 
/**
Building the code
=================
build.h contains #defines that control the compilation of the code


ENABLE_CV - if non-0, emit cv output on pin 6 (EXPERIMENTAL!)
           
Structure of the code
=====================
** Open_Theremin_UNO.ino **
This file. Creates and hooks up the application object to the arduino setup()
and loop() callbacks.

** application.h/application.cpp **
Main application object. Holds the state of the app (playing, calibrating), deals
with initialisation and the app main loop, reads pitch and volume changed flags
from the interrupt handlers and sets pitch and volume values which the timer
interrupt sends to the DAC.
Midi is also managed here

** OTPinDefs.h **
Pin definitions for the DAC.

** build.h **
Preprocessor definitions for build (see above).

** hw.h **
Definitions for hardware button and LED.

** ihandlers.h/ihandlers.cpp
Interrupt handler code and volatile variables implementing the communication between
the app and its input/output.

** theremin_sinetable<N>.c **
Wavetable data for a variety of sounds. Switchable via the potentiometer.

** timer.h/timer.cpp **
Definitions and functions for setting delays in tics and in milliseconds

*/

#include "application.h"

Application app;

void setup() {
  app.setup();
}

void loop() {
  app.loop();
}

