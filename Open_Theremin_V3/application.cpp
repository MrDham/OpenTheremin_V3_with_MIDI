#include "Arduino.h"

#include "application.h"

#include "hw.h"
#include "mcpDac.h"
#include "ihandlers.h"
#include "timer.h"
#include "EEPROM.h"

const AppMode AppModeValues[] = {MUTE,NORMAL};
const int16_t CalibrationTolerance = 15;
const int16_t PitchFreqOffset = 700;
const int16_t VolumeFreqOffset = 700;
const int8_t HYST_VAL = 40;

static int32_t pitchCalibrationBase = 0;
static int32_t pitchCalibrationBaseFreq = 0;
static int32_t pitchCalibrationConstant = 0;
static int32_t pitchSensitivityConstant = 70000;
static int16_t pitchDAC = 0;
static int16_t volumeDAC = 0;
static float qMeasurement = 0;

static int32_t volCalibrationBase   = 0;

static uint16_t old_midi_note =0;
static uint16_t old_midi_volume =0;
static uint16_t old_midi_bend = 0;

static double midi_key_follow = 0.5;
static uint8_t midi_channel = 0;
static uint8_t midi_bend_range = 0;
static uint8_t midi_volume_trigger = 0;
 

Application::Application()
  : _state(PLAYING),
    _mode(NORMAL) {
};

void Application::setup() {

  HW_LED1_ON;HW_LED2_OFF;

  pinMode(Application::BUTTON_PIN, INPUT_PULLUP);
  pinMode(Application::LED_PIN_1,    OUTPUT);
  pinMode(Application::LED_PIN_2,    OUTPUT);

  digitalWrite(Application::LED_PIN_1, HIGH);    // turn the LED off by making the voltage LOW

   mcpDacInit();

EEPROM.get(0,pitchDAC);
EEPROM.get(2,volumeDAC);

mcpDac2ASend(pitchDAC);
mcpDac2BSend(volumeDAC);

  
initialiseTimer();
initialiseInterrupts();


  EEPROM.get(4,pitchCalibrationBase);
  EEPROM.get(8,volCalibrationBase);
 
 midi_setup();

}

void Application::initialiseTimer() {
  ihInitialiseTimer();
}

void Application::initialiseInterrupts() {
  ihInitialiseInterrupts();
}

void Application::InitialisePitchMeasurement() {
   ihInitialisePitchMeasurement();
}

void Application::InitialiseVolumeMeasurement() {
   ihInitialiseVolumeMeasurement();
}

unsigned long Application::GetQMeasurement()
{
  int qn=0;
  
  TCCR1B = (1<<CS10);	

while(!(PIND & (1<<PORTD3)));
while((PIND & (1<<PORTD3)));

TCNT1 = 0;
  timer_overflow_counter = 0;
while(qn<31250){
while(!(PIND & (1<<PORTD3)));
qn++;
while((PIND & (1<<PORTD3)));
};

 
  
  TCCR1B = 0;	

 unsigned long frequency = TCNT1;
 unsigned long temp = 65536*(unsigned long)timer_overflow_counter;
  frequency += temp;

return frequency;

}


unsigned long Application::GetPitchMeasurement()
{
  TCNT1 = 0;
  timer_overflow_counter = 0;
  TCCR1B = (1<<CS12) | (1<<CS11) | (1<<CS10);	

  delay(1000);  
  
  TCCR1B = 0;	

 unsigned long frequency = TCNT1;
 unsigned long temp = 65536*(unsigned long)timer_overflow_counter;
  frequency += temp;

return frequency;

}

unsigned long Application::GetVolumeMeasurement()
{timer_overflow_counter = 0;

  TCNT0=0;
  TCNT1=49911;
  TCCR0B = (1<<CS02) | (1<<CS01) | (1<<CS00);	 // //External clock source on T0 pin. Clock on rising edge.
  TIFR1  = (1<<TOV1);        //Timer1 INT Flag Reg: Clear Timer Overflow Flag

while(!(TIFR1&((1<<TOV1)))); // on Timer 1 overflow (1s)
  TCCR0B = 0;	 // Stop TimerCounter 0
 unsigned long frequency = TCNT0; // get counter 0 value
 unsigned long temp = (unsigned long)timer_overflow_counter; // and overflow counter

 frequency += temp*256;

return frequency;
}



#if CV_ENABLED                                 // Initialise PWM Generator for CV output
void initialiseCVOut() {

}
#endif

AppMode Application::nextMode() {
  return _mode == NORMAL ? MUTE : AppModeValues[_mode + 1];
}

void Application::loop() {
  int32_t pitch_v = 0, pitch_l = 0;            // Last value of pitch  (for filtering)
  int32_t vol_v = 0,   vol_l = 0;              // Last value of volume (for filtering)

  uint16_t volumePotValue = 0;
  uint16_t pitchPotValue = 0;
  int registerPotValue,registerPotValueL = 0;
  int wavePotValue,wavePotValueL = 0;
  uint8_t registerValue = 0;



  mloop:                   // Main loop avoiding the GCC "optimization"

  pitchPotValue    = analogRead(PITCH_POT);
  volumePotValue   = analogRead(VOLUME_POT);
  registerPotValue   = analogRead(REGISTER_SELECT_POT);
  wavePotValue = analogRead(WAVE_SELECT_POT);
  
  if ((registerPotValue-registerPotValueL) >= HYST_VAL || (registerPotValueL-registerPotValue) >= HYST_VAL) registerPotValueL=registerPotValue;
  if (((wavePotValue-wavePotValueL) >= HYST_VAL) || ((wavePotValueL-wavePotValue) >= HYST_VAL)) wavePotValueL=wavePotValue;

  vWavetableSelector=wavePotValueL>>7;
  registerValue=4-(registerPotValueL>>8);  

  if (_state == PLAYING && HW_BUTTON_PRESSED) 
  {
    _state = CALIBRATING;
    _midistate = MIDI_STOP;

    resetTimer();
  }

  if (_state == CALIBRATING && HW_BUTTON_RELEASED) 
  {
    if (timerExpired(1500)) 
    {
         _mode = nextMode();
         if (_mode==NORMAL) 
         {
          HW_LED1_ON;HW_LED2_OFF;
          _midistate = MIDI_SILENT;
         } 
         else 
         {
          HW_LED1_OFF;HW_LED2_ON;
         };
         // playModeSettingSound();
    }
    _state = PLAYING;
  };

  if (_state == CALIBRATING && timerExpired(15000)) 
  {
    HW_LED1_OFF; HW_LED2_ON;
  
      
    playStartupSound();

    if (registerPotValue < 512) // if register pot turned CCW 
    {
      // calibrate heterodyne parameters
      calibrate_pitch();
      calibrate_volume();


      initialiseTimer();
      initialiseInterrupts();
   
      playCalibratingCountdownSound();
      calibrate();
    }
    else // if register turned CW 
    {
      // calibrate midi parameters
      midi_calibrate ();
    };
  
    _mode=NORMAL;
    HW_LED1_ON;HW_LED2_OFF;
      
    while (HW_BUTTON_PRESSED)
      ; // NOP
    
    _state = PLAYING;
    _midistate = MIDI_SILENT;
  };

#if CV_ENABLED
  OCR0A = pitch & 0xff;
#endif



  if (pitchValueAvailable) {                        // If capture event

    pitch_v=pitch;                         // Averaging pitch values
    pitch_v=pitch_l+((pitch_v-pitch_l)>>2);
    pitch_l=pitch_v;


//HW_LED2_ON;


    // set wave frequency for each mode
    switch (_mode) {
      case MUTE : /* NOTHING! */;                                        break;
      case NORMAL      : setWavetableSampleAdvance((pitchCalibrationBase-pitch_v)/registerValue+2048-(pitchPotValue<<2)); break;
    };
    
  //  HW_LED2_OFF;

    pitchValueAvailable = false;
  }

  if (volumeValueAvailable) {
    vol = max(vol, 5000);

    vol_v=vol;                  // Averaging volume values
    vol_v=vol_l+((vol_v-vol_l)>>2);
    vol_l=vol_v;

    switch (_mode) {
      case MUTE:  vol_v = 0;                                                      break;
      case NORMAL:      vol_v = MAX_VOLUME-(volCalibrationBase-vol_v)/2+(volumePotValue<<2)-1024;                                     break;
    };

    // Limit and set volume value
    vol_v = min(vol_v, 4095);
    //    vol_v = vol_v - (1 + MAX_VOLUME - (volumePotValue << 2));
    vol_v = vol_v ;
    vol_v = max(vol_v, 0);
    vScaledVolume = vol_v >> 4;

    volumeValueAvailable = false;
  }

  if (midi_timer > 100) // run midi app every 100 ticks equivalent to approximatevely 3 ms to avoid synth's overload
  {
    midi_application ();
    midi_timer = 0; 
  }

  goto mloop;                           // End of main loop
}

void Application::calibrate()
{
  resetPitchFlag();
  resetTimer();
  savePitchCounter();
  while (!pitchValueAvailable && timerUnexpiredMillis(10))
    ; // NOP
  pitchCalibrationBase = pitch;
  pitchCalibrationBaseFreq = FREQ_FACTOR/pitchCalibrationBase;
  pitchCalibrationConstant = FREQ_FACTOR/pitchSensitivityConstant/2+200;

  resetVolFlag();
  resetTimer();
  saveVolCounter();
  while (!volumeValueAvailable && timerUnexpiredMillis(10))
    ; // NOP
  volCalibrationBase = vol;
  
  
  EEPROM.put(4,pitchCalibrationBase);
  EEPROM.put(8,volCalibrationBase);
  
}

void Application::calibrate_pitch()
{
  
static int16_t pitchXn0 = 0;
static int16_t pitchXn1 = 0;
static int16_t pitchXn2 = 0;
static float q0 = 0;
static long pitchfn0 = 0;
static long pitchfn1 = 0;
static long pitchfn = 0;


  
  InitialisePitchMeasurement();
  interrupts();
  mcpDacInit();

  qMeasurement = GetQMeasurement();  // Measure Arudino clock frequency 

q0 = (16000000/qMeasurement*500000);  //Calculated set frequency based on Arudino clock frequency

pitchXn0 = 0;
pitchXn1 = 4095;

pitchfn = q0-PitchFreqOffset;        // Add offset calue to set frequency



mcpDac2BSend(1600);

mcpDac2ASend(pitchXn0);
delay(100);
pitchfn0 = GetPitchMeasurement();

mcpDac2ASend(pitchXn1);
delay(100);
pitchfn1 = GetPitchMeasurement();

 
while(abs(pitchfn0-pitchfn1)>CalibrationTolerance){      // max allowed pitch frequency offset

mcpDac2ASend(pitchXn0);
delay(100);
pitchfn0 = GetPitchMeasurement()-pitchfn;

mcpDac2ASend(pitchXn1);
delay(100);
pitchfn1 = GetPitchMeasurement()-pitchfn;

pitchXn2=pitchXn1-((pitchXn1-pitchXn0)*pitchfn1)/(pitchfn1-pitchfn0); // new DAC value



pitchXn0 = pitchXn1;
pitchXn1 = pitchXn2;

HW_LED2_TOGGLE;

}
delay(100);

EEPROM.put(0,pitchXn0);
  
}

void Application::calibrate_volume()
{


static int16_t volumeXn0 = 0;
static int16_t volumeXn1 = 0;
static int16_t volumeXn2 = 0;
static float q0 = 0;
static long volumefn0 = 0;
static long volumefn1 = 0;
static long volumefn = 0;

    
  InitialiseVolumeMeasurement();
  interrupts();
  mcpDacInit();


volumeXn0 = 0;
volumeXn1 = 4095;

q0 = (16000000/qMeasurement*460765);
volumefn = q0-VolumeFreqOffset;



mcpDac2BSend(volumeXn0);
delay_NOP(44316);//44316=100ms

volumefn0 = GetVolumeMeasurement();

mcpDac2BSend(volumeXn1);

delay_NOP(44316);//44316=100ms
volumefn1 = GetVolumeMeasurement();



while(abs(volumefn0-volumefn1)>CalibrationTolerance){

mcpDac2BSend(volumeXn0);
delay_NOP(44316);//44316=100ms
volumefn0 = GetVolumeMeasurement()-volumefn;

mcpDac2BSend(volumeXn1);
delay_NOP(44316);//44316=100ms
volumefn1 = GetVolumeMeasurement()-volumefn;

volumeXn2=volumeXn1-((volumeXn1-volumeXn0)*volumefn1)/(volumefn1-volumefn0); // calculate new DAC value



volumeXn0 = volumeXn1;
volumeXn1 = volumeXn2;
HW_LED2_TOGGLE;

}

EEPROM.put(2,volumeXn0);


}

void Application::hzToAddVal(float hz) {
  setWavetableSampleAdvance((uint16_t)(hz * HZ_ADDVAL_FACTOR));
}

void Application::playNote(float hz, uint16_t milliseconds = 500, uint8_t volume = 255) {
  vScaledVolume = volume;
  hzToAddVal(hz);
  millitimer(milliseconds);
  vScaledVolume = 0;
}

void Application::playStartupSound() {
  playNote(MIDDLE_C, 150, 25);
  playNote(MIDDLE_C * 2, 150, 25);
  playNote(MIDDLE_C * 4, 150, 25);
}

void Application::playCalibratingCountdownSound() {
  playNote(MIDDLE_C * 2, 150, 25);
  playNote(MIDDLE_C * 2, 150, 25);
}

void Application::playModeSettingSound() {
  for (int i = 0; i <= _mode; i++) {
    playNote(MIDDLE_C * 2, 200, 25);
    millitimer(100);
  }
}

void Application::delay_NOP(unsigned long time) {
  volatile unsigned long i = 0;
  for (i = 0; i < time; i++) {
      __asm__ __volatile__ ("nop");
  }
}



void Application::midi_setup() 
{

  EEPROM.get(12,midi_channel);
  EEPROM.get(13,midi_bend_range);
  EEPROM.get(14,midi_volume_trigger);

  // Set MIDI baud rate:
  Serial.begin(115200); // Baudrate for midi to serial. Use a serial to midi router http://projectgus.github.com/hairless-midiserial/
  //Serial.begin(31250); // Baudrate for real midi. Use din connection https://www.arduino.cc/en/Tutorial/Midi or HIDUINO https://github.com/ddiakopoulos/hiduino

  _midistate = MIDI_SILENT; 
}


void Application::midi_msg_send(uint8_t channel, uint8_t midi_cmd1, uint8_t midi_cmd2, uint8_t midi_value) 
{
  uint8_t mixed_cmd1_channel; 

  mixed_cmd1_channel = (midi_cmd1 & 0xF0)| (channel & 0x0F);
  
  Serial.write(mixed_cmd1_channel);
  Serial.write(midi_cmd2);
  Serial.write(midi_value);
}

// midi_application sends note and volume and uses pitch bend to simulate continuous picth. 
// Calibrate pitch bend and other parameters accordingly to the receiver synth (see midi_calibrate). 
// New notes won't be generated as long as pitch bend will do the job. 
// The bigger is synth's pitch bend range the beter is the effect.  
// If pitch bend range = 1 no picth bend is generated (portamento will do a better job)
void Application::midi_application ()
{
  uint16_t new_midi_note;
  uint16_t new_midi_volume; 
  
  uint16_t new_midi_bend;
  uint8_t midi_bend_low; 
  uint8_t midi_bend_high;
    
  double double_log_freq;
  double double_log_bend;

  // Calculate volume for midi 
  new_midi_volume = vScaledVolume >> 1; 
  new_midi_volume = min (new_midi_volume, 127);

  // Calculate note and pitch bend for midi 
  if (vPointerIncrement < 18) 
  {
    // Highest note
    new_midi_note = 0; 
    new_midi_bend = 8192;
  }
  else if (vPointerIncrement > 26315) 
  {
    // Lowest note
    new_midi_note = 127; 
    new_midi_bend = 8192;
  }
  else
  {
    // Find note in the playing range
    double_log_freq = (log (vPointerIncrement/17.152) / 0.057762265); // Precise note played in the logaritmic scale
    double_log_bend = double_log_freq - old_midi_note; // How far from last played midi chromatic note we are

    // If too much far from last midi chromatic note played (midi_key_follow depends on pitch bend range)
    if (abs (double_log_bend) >= midi_key_follow)
    {
      new_midi_note = round (double_log_freq);  // Select the new midi chromatic note 
      double_log_bend = double_log_freq - new_midi_note; // calculate bend to reach precise note played
    }
    else
    {
       new_midi_note = old_midi_note; // No change 
    }

    // If pitch bend range greater than 1 
    if (midi_bend_range > 1)
    {
      // use it to reach precise note played
      new_midi_bend = 8192 + (8191 * double_log_bend / midi_bend_range); // Calculate midi pitch bend
    }
    else
    {
      // Don't use pitch bend (portamento would do a beter job)
      new_midi_bend = 8192; 
    }
  }

  // Prepare the 2 bites of picth bend midi message
  midi_bend_low = (int8_t) (new_midi_bend & 0x007F);
  midi_bend_high = (int8_t) ((new_midi_bend & 0x3F80)>> 7);
  
  // State machine for MIDI
  switch (_midistate)
  {
  case MIDI_SILENT:  
    // Synth sound could be in Release phase of ADSR or may have some delay or reverb effect so...
    
    // ... don't refresh pitch bend: 
      // Instruction "midi_key_follow = 0.5;" and unrefreshed notes would make pitch bend verry messy. 
    
    // ... but always refresh midi volume value. 
    if (new_midi_volume != old_midi_volume)
    {
      midi_msg_send(midi_channel, 0xB0, 0x07, new_midi_volume);
      old_midi_volume = new_midi_volume;
    }
    else
    {
      // do nothing
    }

    // If player's hand moves away from volume antenna
    if (new_midi_volume > midi_volume_trigger)
    {
      // Send pitch bend to reach precise played note (send 8192 (no pitch bend) in case of midi_bend_range == 1)
      midi_msg_send(midi_channel, 0xE0, midi_bend_low, midi_bend_high);
      old_midi_bend = new_midi_bend;
      
      // Play the note
      midi_msg_send(midi_channel, 0x90, new_midi_note, 0x45);
      old_midi_note = new_midi_note;

      // Set key follow so as next played note will be at limit of pitch bend range
      midi_key_follow = (double)(midi_bend_range) - 0.2;

      _midistate = MIDI_PLAYING;
    }
    else
    {
      // Do nothing
    }
    break; 
  
  case MIDI_PLAYING:  
    // Always refresh midi volume value
    if (new_midi_volume != old_midi_volume)
    {
      midi_msg_send(midi_channel, 0xB0, 0x07, new_midi_volume);
      old_midi_volume = new_midi_volume;
    }
    else
    {
      // do nothing
    }

    // If player's hand is far from volume antenna
    if (new_midi_volume > midi_volume_trigger)
    {
      // Refresh midi pitch bend value
      if (new_midi_bend != old_midi_bend)
      {
        midi_msg_send(midi_channel, 0xE0, midi_bend_low, midi_bend_high);   
        old_midi_bend = new_midi_bend;
      }
      else
      {
        // do nothing
      } 
      
      // Refresh midi note
      if (new_midi_note != old_midi_note) 
      {
        // Play new note before muting old one to play legato on monophonic synth 
        // (pitch pend management tends to break expected effect here)
        midi_msg_send(midi_channel, 0x90, new_midi_note, 0x45);
        midi_msg_send(midi_channel, 0x90, old_midi_note, 0);
        old_midi_note = new_midi_note;
      }
      else 
      {
        // do nothing
      } 
    }
    else // Means that player's hand moves to the volume antenna
    {
      // Send note off
      midi_msg_send(midi_channel, 0x90, old_midi_note, 0);

      // Set key follow to the minimum in order to use closest note played as the center note for pitch bend next time
      midi_key_follow = 0.5;
      
      _midistate = MIDI_SILENT;
    }
    break;
    
  case MIDI_STOP:
    // Send all note off
    midi_msg_send(midi_channel, 0xB0, 0x7B, 0x00);

    // Mute long release notes
    midi_msg_send(midi_channel, 0xB0, 0x07, 0);
    old_midi_volume = 0;

    _midistate = MIDI_MUTE;
    break;

  case MIDI_MUTE:
    //do nothing
    break;
    
  }
}

// midi_calibrate allows the user to set some midi parameters
// Set potentiometer accordingly to comments bellow BEFORE entering in midi calibration mode. 
// Hear may help somewhat to determine entered values
void Application::midi_calibrate ()
{
  uint16_t pot_channel;
  uint16_t pot_bend_range;
  uint16_t pot_volume_trigger;

  uint16_t bend_range_scale; 

  // Midi channel uses "Timbre" pot. 
  // Waveform may help user do determine which couple of channel is chosen (WF 1 Lo -> Ch1, WF 1 Hi -> Ch2, WF 2 Lo -> Ch3, etc...)
  pot_channel = analogRead(WAVE_SELECT_POT);
  midi_channel = (uint8_t)((pot_channel >> 6) & 0x000F);
  EEPROM.put(12,midi_channel);
  
  
  // Pitch bend range and associated distance between notes jumps use "Pitch" pot. 
  // The user shall set synth's pitch bend range acordingly to the selected Theremin's pitch bend range:
  // 1 semitone, 7 semitones (a fifth), 12 semitones (an octave) or 24 semitones (two octaves). 
  // The "1 semitone" setting blocks pitch bend generation (use portamento on the synth)
  pot_bend_range = analogRead(PITCH_POT);
  bend_range_scale = pot_bend_range >> 8;
  if (bend_range_scale == 0)
  {
    midi_bend_range = 1; 
  }
  else if (bend_range_scale == 1)
  {
    midi_bend_range = 7; 
  }
  else if (bend_range_scale == 2)
  {
    midi_bend_range = 12; 
  }
  else
  {
    midi_bend_range = 24; 
  }
  EEPROM.put(13,midi_bend_range);
  
  // Volume trigger uses "Volume" pot 
  // Select a high value if some percussive sounds are played (so as it is heard when volume is not null)
  pot_volume_trigger = analogRead(VOLUME_POT);
  midi_volume_trigger = (uint8_t)((pot_volume_trigger >> 3) & 0x007F);
  EEPROM.put(14,midi_volume_trigger);
}


