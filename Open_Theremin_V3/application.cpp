#include "Arduino.h"

#include "application.h"

#include "hw.h"
#include "SPImcpDAC.h"
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

static uint8_t new_midi_note =0;
static uint8_t old_midi_note =0;

static uint8_t new_midi_loop_cc_val =0;
static uint8_t old_midi_loop_cc_val =0;

static uint8_t midi_velocity = 0;

static uint8_t loop_hand_pos = 0; 

static uint16_t new_midi_rod_cc_val =0;
static uint16_t old_midi_rod_cc_val =0;

static uint16_t new_midi_bend =0;
static uint16_t old_midi_bend = 0;
static uint8_t midi_bend_low; 
static uint8_t midi_bend_high;

static double double_log_freq = 0;
static double midi_key_follow = 0.5;
      
// Configuration parameters
static uint8_t registerValue = 2;
  // wavetable selector is defined and initialized in ihandlers.cpp
static uint8_t midi_channel = 0;
static uint8_t old_midi_channel = 0;
static uint8_t midi_bend_range = 2;
static uint8_t midi_volume_trigger = 0;
static uint8_t flag_legato_on = 1;
static uint8_t flag_pitch_bend_on = 1;
static uint8_t loop_midi_cc = 7;
static uint8_t rod_midi_cc = 255; 
static uint8_t rod_midi_cc_lo = 255; 
static double rod_cc_scale = 1;


// tweakable paramameters
#define VELOCITY_SENS  9 // How easy it is to reach highest velocity (127). Something betwen 5 and 12.
#define PLAYER_ACCURACY  0.2 // between 0 (very accurate players) and 0.5 (not accurate at all)

static uint16_t data_pot_value = 0; 
static uint16_t old_data_pot_value = 0; 

static uint16_t param_pot_value = 0; 
static uint16_t old_param_pot_value = 0; 


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

   SPImcpDACinit();

EEPROM.get(0,pitchDAC);
EEPROM.get(2,volumeDAC);

SPImcpDAC2Asend(pitchDAC);
SPImcpDAC2Bsend(volumeDAC);

  
initialiseTimer();
initialiseInterrupts();


  EEPROM.get(4,pitchCalibrationBase);
  EEPROM.get(8,volCalibrationBase);
 
 init_parameters();
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

  mloop:                   // Main loop avoiding the GCC "optimization"

  pitchPotValue    = analogRead(PITCH_POT);
  volumePotValue   = analogRead(VOLUME_POT);
  
  set_parameters ();
  
  if (_state == PLAYING && HW_BUTTON_PRESSED) 
  {
    _state = CALIBRATING;

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
          _midistate = MIDI_STOP;

         };
         // playModeSettingSound();
    }
    _state = PLAYING;
  };

  if (_state == CALIBRATING && timerExpired(15000)) 
  {
    HW_LED1_OFF; HW_LED2_ON;
  
      
    playStartupSound();

    // calibrate heterodyne parameters
    calibrate_pitch();
    calibrate_volume();


    initialiseTimer();
    initialiseInterrupts();
   
    playCalibratingCountdownSound();
    calibrate();
  
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
      case NORMAL      : setWavetableSampleAdvance(((pitchCalibrationBase-pitch_v)+2048-(pitchPotValue<<2))>>registerValue); break;
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
    loop_hand_pos = vol_v >> 4;

    // Give vScaledVolume a pseudo-exponential characteristic:
    vScaledVolume = loop_hand_pos * (loop_hand_pos + 2);
    
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


  // limit the number of calibration iteration to 12 
  // the algorythm used is normaly faster than dichotomy which normaly finds a 12Bit number in 12 iterations max
  static uint16_t l_iteration_pitch = 0;
  
  InitialisePitchMeasurement();
  interrupts();
  SPImcpDACinit();

  qMeasurement = GetQMeasurement();  // Measure Arudino clock frequency 

q0 = (16000000/qMeasurement*500000);  //Calculated set frequency based on Arudino clock frequency

pitchXn0 = 0;
pitchXn1 = 4095;

pitchfn = q0-PitchFreqOffset;        // Add offset calue to set frequency



SPImcpDAC2Bsend(1600);

SPImcpDAC2Asend(pitchXn0);
delay(100);
pitchfn0 = GetPitchMeasurement();

SPImcpDAC2Asend(pitchXn1);
delay(100);
pitchfn1 = GetPitchMeasurement();

 
l_iteration_pitch = 0;
while ((abs(pitchfn0 - pitchfn1) > CalibrationTolerance) && (l_iteration_pitch < 12))
{      
      
SPImcpDAC2Asend(pitchXn0);
delay(100);
pitchfn0 = GetPitchMeasurement()-pitchfn;

SPImcpDAC2Asend(pitchXn1);
delay(100);
pitchfn1 = GetPitchMeasurement()-pitchfn;

pitchXn2=pitchXn1-((pitchXn1-pitchXn0)*pitchfn1)/(pitchfn1-pitchfn0); // new DAC value


pitchXn0 = pitchXn1;
pitchXn1 = pitchXn2;

HW_LED2_TOGGLE;
      
l_iteration_pitch ++;
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

  // limit the number of calibration iteration to 12 
  // the algorythm used is normaly faster than dichotomy which normaly finds a 12Bit number in 12 iterations max
  static uint16_t l_iteration_volume = 0; 
    
  InitialiseVolumeMeasurement();
  interrupts();
  SPImcpDACinit();


volumeXn0 = 0;
volumeXn1 = 4095;

q0 = (16000000/qMeasurement*460765);
volumefn = q0-VolumeFreqOffset;



SPImcpDAC2Bsend(volumeXn0);
delay_NOP(44316);//44316=100ms

volumefn0 = GetVolumeMeasurement();

SPImcpDAC2Bsend(volumeXn1);

delay_NOP(44316);//44316=100ms
volumefn1 = GetVolumeMeasurement();



l_iteration_volume = 0;
while ((abs(volumefn0 - volumefn1) > CalibrationTolerance) && (l_iteration_volume < 12))
{

SPImcpDAC2Bsend(volumeXn0);
delay_NOP(44316);//44316=100ms
volumefn0 = GetVolumeMeasurement()-volumefn;

SPImcpDAC2Bsend(volumeXn1);
delay_NOP(44316);//44316=100ms
volumefn1 = GetVolumeMeasurement()-volumefn;

volumeXn2=volumeXn1-((volumeXn1-volumeXn0)*volumefn1)/(volumefn1-volumefn0); // calculate new DAC value


volumeXn0 = volumeXn1;
volumeXn1 = volumeXn2;

HW_LED2_TOGGLE;

l_iteration_volume ++;
}

EEPROM.put(2,volumeXn0);


}

void Application::hzToAddVal(float hz) {
  setWavetableSampleAdvance((uint16_t)(hz * HZ_ADDVAL_FACTOR));
}

void Application::playNote(float hz, uint16_t milliseconds = 500, uint8_t volume = 255) {
  vScaledVolume = volume * (volume + 2);
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
  // Set MIDI baud rate:
  Serial.begin(115200); // Baudrate for midi to serial. Use a serial to midi router https://github.com/projectgus/hairless-midiserial
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
void Application::midi_application ()
{
  double delta_loop_cc_val = 0; 
  double calculated_velocity = 0;
  
  
  // Calculate loop antena cc value for midi 
  new_midi_loop_cc_val = loop_hand_pos >> 1; 
  new_midi_loop_cc_val = min (new_midi_loop_cc_val, 127);
  delta_loop_cc_val = (double)new_midi_loop_cc_val - (double)old_midi_loop_cc_val;

  // Calculate log freq 
  if ((vPointerIncrement < 18) || (vPointerIncrement > 65518)) 
  {
    // Lowest note
    double_log_freq = 0; 
  }
  else if ((vPointerIncrement > 26315) && (vPointerIncrement < 39221))
  {
    // Highest note
    double_log_freq = 127; 
  }
  else if (vPointerIncrement < 32768)
  {
    // Positive frequencies
    // Find note in the playing range
    double_log_freq = (log (vPointerIncrement/17.152) / 0.057762265); // Precise note played in the logaritmic scale
  }
  else
  {
    // Negative frequencies
    // Find note in the playing range
    double_log_freq = (log ((65535-vPointerIncrement+1)/17.152) / 0.057762265); // Precise note played in the logaritmic scale
  }
  
  // Calculate rod antena cc value for midi 
  new_midi_rod_cc_val = round (min(((double_log_freq * 128) * rod_cc_scale), 16383)); // 14 bit value !

  // State machine for MIDI
  switch (_midistate)
  {
  case MIDI_SILENT:  
    // Always refresh midi loop antena cc. 
    if (new_midi_loop_cc_val != old_midi_loop_cc_val)
    {
      midi_msg_send(midi_channel, 0xB0, loop_midi_cc, new_midi_loop_cc_val);
      old_midi_loop_cc_val = new_midi_loop_cc_val;
    }
    else
    {
      // do nothing
    }

    // Always refresh midi rod antena cc if applicable. 
    if (new_midi_rod_cc_val != old_midi_rod_cc_val)
    {
      if (rod_midi_cc != 255) 
      {
        midi_msg_send(midi_channel, 0xB0, rod_midi_cc, (uint8_t)(new_midi_rod_cc_val >> 7));
        if (rod_midi_cc_lo != 255)
        {
          midi_msg_send(midi_channel, 0xB0, rod_midi_cc_lo, (uint8_t)(new_midi_rod_cc_val & 0x007F)); 
        }
      }
      old_midi_rod_cc_val = new_midi_rod_cc_val;
    }
    else
    {
      // do nothing
    }

    // If player's hand moves away from volume antenna
    if (new_midi_loop_cc_val > midi_volume_trigger)
    {
      // Set key follow to the minimum in order to use closest note played as the center note 
      midi_key_follow = 0.5;

      // Calculate note and associated pitch bend 
      calculate_note_bend ();
      
      // Send pitch bend to reach precise played note (send 8192 (no pitch bend) in case of midi_bend_range == 1)
      midi_msg_send(midi_channel, 0xE0, midi_bend_low, midi_bend_high);
      old_midi_bend = new_midi_bend;

      // Calculate velocity
      if (midi_timer != 0)
      {
        calculated_velocity = (64 * (127 - (double)midi_volume_trigger) / 127) + (VELOCITY_SENS * (double)midi_volume_trigger * delta_loop_cc_val / (double)midi_timer);
        midi_velocity = min (round (abs (calculated_velocity)), 127);
      }
      else 
      {
        // should not happen
        midi_velocity = 64;
      }

      
      // Play the note
      midi_msg_send(midi_channel, 0x90, new_midi_note, midi_velocity);
      old_midi_note = new_midi_note;

      _midistate = MIDI_PLAYING;
    }
    else
    {
      // Do nothing
    }
    break; 
  
  case MIDI_PLAYING:  
    // Always refresh midi loop antena cc. 
    if (new_midi_loop_cc_val != old_midi_loop_cc_val)
    {
      midi_msg_send(midi_channel, 0xB0, loop_midi_cc, new_midi_loop_cc_val);
      old_midi_loop_cc_val = new_midi_loop_cc_val;
    }
    else
    {
      // do nothing
    }

    // Always refresh midi rod antena cc if applicable. 
    if (new_midi_rod_cc_val != old_midi_rod_cc_val)
    {
      if (rod_midi_cc != 255) 
      {
        midi_msg_send(midi_channel, 0xB0, rod_midi_cc, (uint8_t)(new_midi_rod_cc_val >> 7));
        if (rod_midi_cc_lo != 255)
        {
          midi_msg_send(midi_channel, 0xB0, rod_midi_cc_lo, (uint8_t)(new_midi_rod_cc_val & 0x007F)); 
        }
      }
      old_midi_rod_cc_val = new_midi_rod_cc_val;
    }
    else
    {
      // do nothing
    }

    // If player's hand is far from volume antenna
    if (new_midi_loop_cc_val > midi_volume_trigger)
    {
      if ( flag_legato_on == 1)
      {
        // Set key follow so as next played note will be at limit of pitch bend range
        midi_key_follow = (double)(midi_bend_range) - PLAYER_ACCURACY;
      }
      else
      {
        // Set key follow to max so as no key follows
        midi_key_follow = 127;
      }

      // Calculate note and associated pitch bend 
      calculate_note_bend (); 
      
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
        midi_msg_send(midi_channel, 0x90, new_midi_note, midi_velocity);
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

      _midistate = MIDI_SILENT;
    }
    break;
    
  case MIDI_STOP:
    // Send all note off
    midi_msg_send(midi_channel, 0xB0, 0x7B, 0x00);

    _midistate = MIDI_MUTE;
    break;

  case MIDI_MUTE:
    //do nothing
    break;
    
  }
}

void Application::calculate_note_bend ()
{
  double double_log_bend;
  double double_norm_log_bend;
    
  double_log_bend = double_log_freq - old_midi_note; // How far from last played midi chromatic note we are

  // If too much far from last midi chromatic note played (midi_key_follow depends on pitch bend range)
  if ((abs (double_log_bend) >= midi_key_follow) && (midi_key_follow != 127))
  {
    new_midi_note = round (double_log_freq);  // Select the new midi chromatic note 
    double_log_bend = double_log_freq - new_midi_note; // calculate bend to reach precise note played
  }
  else
  {
     new_midi_note = old_midi_note; // No change 
  }

  // If pitch bend activated 
  if (flag_pitch_bend_on == 1)
  {
    // use it to reach precise note played
    double_norm_log_bend = (double_log_bend / midi_bend_range);
    if (double_norm_log_bend > 1)
    {
      double_norm_log_bend = 1; 
    }
    else if (double_norm_log_bend < -1)
    {
      double_norm_log_bend = -1;
    }
    new_midi_bend = 8192 + (8191 * double_norm_log_bend); // Calculate midi pitch bend
  }
  else
  {
    // Don't use pitch bend 
    new_midi_bend = 8192; 
  }
  

  // Prepare the 2 bites of picth bend midi message
  midi_bend_low = (int8_t) (new_midi_bend & 0x007F);
  midi_bend_high = (int8_t) ((new_midi_bend & 0x3F80)>> 7);
}



void Application::init_parameters ()
{
  // init data pot value to avoid 1st position to be taken into account
  data_pot_value = analogRead(WAVE_SELECT_POT);
  old_data_pot_value = data_pot_value;
  
}

void Application::set_parameters ()
{
  uint16_t data_steps;
  
  param_pot_value = analogRead(REGISTER_SELECT_POT);
  data_pot_value = analogRead(WAVE_SELECT_POT);

  // If parameter pot moved
  if (abs((int32_t)param_pot_value - (int32_t)old_param_pot_value) >= 8)
  {
    // Blink the LED relatively to pot position
    resetTimer();
    if (((param_pot_value >> 7) % 2) == 0)
    {
      HW_LED1_OFF;
      HW_LED2_OFF;
    }
    else
    {
      HW_LED1_ON;
      HW_LED2_ON;
    }

    // Memorize data pot value to monitor changes
    old_param_pot_value = param_pot_value;
  }
  
  // Else If data pot moved
  else if (abs((int32_t)data_pot_value - (int32_t)old_data_pot_value) >= 8)
  {
    // Modify selected parameter
    switch (param_pot_value >> 7)
    {
    case 0:
      // Transpose
      switch (data_pot_value >> 8)
      {
      case 0:
        registerValue=3; // -1 Octave
        data_steps = 1;
        break; 
      case 1:
      case 2:
        registerValue=2; // Center
        data_steps = 2;
        break; 
      default:
        registerValue=1; // +1 Octave 
        data_steps = 3;
        break; 
      }
      break;
      
    case 1:
      // Waveform
      data_steps = data_pot_value >> 7;
      vWavetableSelector = data_steps;
      break;
      
    case 2:
      // Channel
      data_steps = data_pot_value >> 6;
      midi_channel = (uint8_t)(data_steps & 0x000F);
      if (old_midi_channel != midi_channel)
      {
        // Send all note off to avoid stuck notes
        midi_msg_send(old_midi_channel, 0xB0, 0x7B, 0x00);
        old_midi_channel = midi_channel;
      }
      break;
        
    case 3:
      // Rod antenna mode
      data_steps = data_pot_value >> 8;
      switch (data_steps)
      {
      case 0:
        flag_legato_on = 0;
        flag_pitch_bend_on = 0;
        break; 
      case 1:
        flag_legato_on = 0;
        flag_pitch_bend_on = 1;
        break; 
      case 2:
        flag_legato_on = 1;
        flag_pitch_bend_on = 0;
        break; 
      default:
        flag_legato_on = 1;
        flag_pitch_bend_on = 1;
        break;  
      }
      break;
      
    case 4:
      // Pitch-Bend range
      data_steps = data_pot_value >> 7;
      switch (data_steps)
      {
      case 0:
        midi_bend_range = 1; 
        break; 
      case 1:
        midi_bend_range = 2; 
        break; 
      case 2:
        midi_bend_range = 4; 
        break; 
      case 3:
        midi_bend_range = 5; 
        break; 
      case 4:
        midi_bend_range = 7; 
        break; 
      case 5:
        midi_bend_range = 12; 
        break; 
      case 6:
        midi_bend_range = 24; 
        break;  
      default:
        midi_bend_range = 48; 
        break;  
      }
      break;
      
    case 5:
      // Volume trigger
      data_steps = data_pot_value >> 8;
      midi_volume_trigger = (uint8_t)((data_pot_value >> 3) & 0x007F);
      break;
      
    case 6:
      //Rod antenna cc
      data_steps = data_pot_value >> 7;
      switch (data_steps)
      {
      case 0:
        rod_midi_cc = 255; // Nothing
        rod_midi_cc_lo = 255; // Nothing
        rod_cc_scale = 1.0;
        break; 
      case 1:
        rod_midi_cc = 8; // Balance
        rod_midi_cc_lo = 255; // No least significant bits
        rod_cc_scale = 1.0;
        break; 
      case 2:
        rod_midi_cc = 10; // Pan
        rod_midi_cc_lo = 255; // No least significant bits
        rod_cc_scale = 1.0;
        break; 
      case 3:
        rod_midi_cc = 16; // General Purpose 1 (14 Bits)
        rod_midi_cc_lo = 48; // General Purpose 1 least significant bits
        rod_cc_scale = 1.0;
        break; 
      case 4:
        rod_midi_cc = 17; // General Purpose 2 (14 Bits)
        rod_midi_cc_lo = 49; // General Purpose 2 least significant bits
        rod_cc_scale = 1.0;
        break; 
      case 5:
        rod_midi_cc = 18; // General Purpose 3 (7 Bits)
        rod_midi_cc_lo = 255; // No least significant bits
        rod_cc_scale = 1.0;
        break; 
      case 6:
        rod_midi_cc = 19; // General Purpose 4 (7 Bits)
        rod_midi_cc_lo = 255; // No least significant bits
        rod_cc_scale = 1.0;
        break; 
      default:
        rod_midi_cc = 74; // Cutoff (exists of both loop and rod)
        rod_midi_cc_lo = 255; // No least significant bits
        rod_cc_scale = 1.0;
        break; 
      }
      break;
      
          
    default:
      // Loop antenna cc
      data_steps = data_pot_value >> 7;
      switch (data_steps)
      {
      case 0:
        loop_midi_cc = 1; // Modulation
        break; 
      case 1:
        loop_midi_cc = 7; // Volume
        break; 
      case 2:
        loop_midi_cc = 11; // Expression
        break; 
      case 3:
        loop_midi_cc = 71; // Resonnance
        break; 
      case 4:
        loop_midi_cc = 74; // Cutoff (exists of both loop and rod)
        break; 
      case 5:
        loop_midi_cc = 91; // Reverb
        break; 
      case 6:
        loop_midi_cc = 93; // Chorus
        break; 
      default:
        loop_midi_cc = 95; // Phaser
        break; 
      }
      break;
    }

    // Blink the LED relatively to pot position
    resetTimer();
    if ((data_steps % 2) == 0)
    {
      HW_LED1_OFF;
      HW_LED2_OFF;
    }
    else
    {
      HW_LED1_ON;
      HW_LED2_ON;
    }


    // Memorize data pot value to monitor changes
    old_data_pot_value = data_pot_value;
  }

  else
  {
    if (timerExpired(65000))
    //restore LED status
    {
      if (_mode == NORMAL)
      {
        HW_LED1_ON;
        HW_LED2_OFF;
      }
      else
      {
        HW_LED1_OFF;
        HW_LED2_ON;
      }
    }
  }
}
