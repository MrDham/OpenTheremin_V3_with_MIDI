#ifndef _APPLICATION_H
#define _APPLICATION_H

#include <avr/io.h>

#include "build.h"

enum AppState {CALIBRATING = 0, PLAYING};
enum AppMode  {MUTE = 0, NORMAL};
enum AppMidiState {MIDI_SILENT = 0, MIDI_PLAYING, MIDI_STOP, MIDI_MUTE};

class Application {
  public:
    Application();
    
    void setup();
    void loop();
    
  private:
    static const uint16_t MAX_VOLUME = 4095;
    static const uint32_t TRIM_PITCH_FACTOR = 33554432;
    static const uint32_t FREQ_FACTOR = 1600000000;

    static const int16_t BUTTON_PIN = 6;
    static const int16_t LED_PIN_1  = 18;
    static const int16_t LED_PIN_2  = 19;
    

    static const int16_t PITCH_POT = 0;
    static const int16_t VOLUME_POT = 1;
    static const int16_t WAVE_SELECT_POT = 2;
    static const int16_t REGISTER_SELECT_POT = 3;


    

    AppState _state;
    AppMode  _mode;
    AppMidiState _midistate;
        
    void calibrate();
    void calibrate_pitch();
    void calibrate_volume();


    AppMode nextMode();
    
    void initialiseTimer();
    void initialiseInterrupts();
    void InitialisePitchMeasurement();
    void InitialiseVolumeMeasurement();
    unsigned long GetPitchMeasurement();
    unsigned long GetVolumeMeasurement();
    unsigned long GetQMeasurement();


    const float HZ_ADDVAL_FACTOR = 2.09785;
    const float MIDDLE_C = 261.6;

    void playNote(float hz, uint16_t milliseconds, uint8_t volume);
    uint16_t log2U16 (uint16_t lin_input);
    void hzToAddVal(float hz);
    void playStartupSound();
    void playCalibratingCountdownSound();
    void playModeSettingSound();
    void delay_NOP(unsigned long time);

    void midi_setup();
    void midi_msg_send(uint8_t channel, uint8_t midi_cmd1, uint8_t midi_cmd2, uint8_t midi_value);
    void midi_application ();
    void calculate_note_bend ();
    void init_parameters ();
    void set_parameters ();

};

#endif // _APPLICATION_H
