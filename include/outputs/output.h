#ifndef MIDI_OUTPUT__INCLUDED
#define MIDI_OUTPUT__INCLUDED

#include <Arduino.h>
#include <LinkedList.h>

#include "debug.h"

#ifdef USE_TINYUSB
    #include <Adafruit_TinyUSB.h>
#endif
#include "MIDI.h"
#include "Drums.h"
#include "bpm.h"

#include "sequencer/Sequencer.h"

#include "midi_helpers.h"

#define MAX_LABEL 40

#include "midi_usb/midi_usb_rp2040.h"

#include "ParameterManager.h"
#include "parameters/MIDICCParameter.h"

byte get_muso_note_for_drum(byte drum_note);

// todo: port usb_midi_clocker's OutputWrapper to work here?
// wrapper class to wrap different MIDI output types
class MIDIOutputWrapper : public IMIDICCTarget {
    public:
    #ifdef USE_TINYUSB
        midi::MidiInterface<midi::SerialMIDI<Adafruit_USBD_MIDI>> *usbmidi = &USBMIDI;
    #endif
    #ifdef USE_DINMIDI
        midi::MidiInterface<midi::SerialMIDI<SerialPIO>> *dinmidi = &DINMIDI;
    #endif

    void sendNoteOn(byte pitch, byte velocity, byte channel) {
        //Serial.printf("MIDIOutputWrapper#sendNoteOn(%i, %i, %i)\n", pitch, velocity, channel);
        if (!is_valid_note(pitch)) 
            return;

        #ifdef USE_TINYUSB
            usbmidi->sendNoteOn(pitch, velocity, channel);
        #endif
        #ifdef USE_DINMIDI
            if (channel==GM_CHANNEL_DRUMS)
                dinmidi->sendNoteOn(get_muso_note_for_drum(pitch), velocity, MUSO_TRIGGER_CHANNEL);
        #endif
    }
    void sendNoteOff(byte pitch, byte velocity, byte channel) {
        if (!is_valid_note(pitch)) 
            return;
            
        #ifdef USE_TINYUSB
            usbmidi->sendNoteOff(pitch, velocity, channel);
        #endif
        #ifdef USE_DINMIDI
            if (channel==GM_CHANNEL_DRUMS)
                dinmidi->sendNoteOff(get_muso_note_for_drum(pitch), velocity, MUSO_TRIGGER_CHANNEL);
        #endif
    }

    void sendControlChange(byte number, byte value, byte channel) {
        if (!is_valid_note(number))
            return;
        #ifdef USE_TINYUSB
            usbmidi->sendControlChange(number, value, channel);
        #endif
        #ifdef USE_DINMIDI
            dinmidi->sendControlChange(number, value, channel);
        #endif
    }

    void sendClock() {
        #ifdef USE_TINYUSB
            usbmidi->sendClock();
        #endif
        #ifdef USE_DINMIDI
            if (is_bpm_on_beat(ticks))  // todo: make clock tick sends to din MIDI use custom divisor value
                dinmidi->sendClock();   // send divisions of clock to muso, to make the clock output more useful
        #endif
    }
    void sendStart() {
        #ifdef USE_TINYUSB
            usbmidi->sendStart();
        #endif
        #ifdef USE_DINMIDI
            dinmidi->sendStart();
        #endif
    }
    void sendStop() {
        #ifdef USE_TINYUSB
            usbmidi->sendStop();
        #endif
        #ifdef USE_DINMIDI
            dinmidi->sendStop();
        #endif
    }

    MIDICCParameter<> midi_cc_parameters[6] = {
        MIDICCParameter<> ("A",     this, 1, 1, true),
        MIDICCParameter<> ("B",     this, 2, 1, true),
        MIDICCParameter<> ("C",     this, 3, 1, true),
        MIDICCParameter<> ("Mix1",  this, 4, 1, true),
        MIDICCParameter<> ("Mix2",  this, 5, 1, true),
        MIDICCParameter<> ("Mix3",  this, 6, 1, true),
    };

    void setup_parameters() {
        for (int i = 0 ; i < 6 ; i++) {
            parameter_manager->addParameter(&midi_cc_parameters[i]);
        }
        midi_cc_parameters[0].connect_input(0/*parameter_manager->getInputForName("A")*/, 1.0f);
        midi_cc_parameters[0].connect_input(1/*parameter_manager->getInputForName("A")*/, 0.f);
        midi_cc_parameters[0].connect_input(2/*parameter_manager->getInputForName("A")*/, 0.f);

        midi_cc_parameters[1].connect_input(0/*parameter_manager->getInputForName("B")*/, 0.f);
        midi_cc_parameters[1].connect_input(1/*parameter_manager->getInputForName("B")*/, 1.0f);
        midi_cc_parameters[1].connect_input(2/*parameter_manager->getInputForName("B")*/, 0.f);

        midi_cc_parameters[2].connect_input(0/*parameter_manager->getInputForName("C")*/, 0.f);
        midi_cc_parameters[2].connect_input(1/*parameter_manager->getInputForName("C")*/, 0.f);
        midi_cc_parameters[2].connect_input(2/*parameter_manager->getInputForName("C")*/, 1.0f);

        midi_cc_parameters[3].connect_input(0, 1.0f);
        midi_cc_parameters[3].connect_input(1, 1.0f);
        midi_cc_parameters[3].connect_input(2, .0f);

        midi_cc_parameters[4].connect_input(0, .0f);
        midi_cc_parameters[4].connect_input(1, 1.0f);
        midi_cc_parameters[4].connect_input(2, 1.0f);

        midi_cc_parameters[5].connect_input(0, 1.0f);
        midi_cc_parameters[5].connect_input(1, .0f);
        midi_cc_parameters[5].connect_input(2, 1.0f);
    }

    /*LinkedList<String> *add_all_save_lines(LinkedList<String> *lines) {
        for (int i = 0 ; i < 6 ; i++) {
            MIDICCParameter<> p = midi_cc_parameters[i];
            lines->add(String("midi_cc_parameter_") + String(p.label) + String("_channel=") + String(p.channel));
            lines->add(String("midi_cc_parameter_") + String(p.label) + String("_cc=") + String(p.cc_number));
        }
        return lines;
    }

    bool load_parse_key_value(String key, String value) {
        String prefix = "midi_cc_parameter_";
        if (!key.startsWith(prefix))
            return false;

        key.replace(prefix, "");
        for (int i = 0 ; i < 6 ; i++) {
            String label = key.substring(0,key.indexOf("_"));
            if (label.equals(midi_cc_parameters[i].label)) {
                if (key.substring(key.indexOf("_"), key.length()).equals("channel"))
                    midi_cc_parameters[i].channel = value.toInt();
                else if (key.substring(key.indexOf("_"), key.length()).equals("cc"))
                    midi_cc_parameters[i].cc_number = value.toInt();
            }
        }        
    }*/

    #ifdef ENABLE_SCREEN
        void create_menu_items();
    #endif
};


#ifdef ENABLE_SCREEN
    class Menu;
#endif

// class to receive triggers from a sequencer and return values to the owner Processor
class BaseOutput {
    public:
    bool enabled = true;

    char label[MAX_LABEL];
    BaseOutput (const char *label) {
        strncpy(this->label, label, MAX_LABEL);
    }
    
    // event_value_1 = send a note on
    // event_value_2 = send a note off
    // event_value_3 = ??
    virtual void receive_event(byte event_value_1, byte event_value_2, byte event_value_3) = 0;
    virtual void reset() = 0;
    virtual bool matches_label(const char *compare) {
        return strcmp(compare, this->label)==0;
    }

    virtual bool should_go_on() = 0;
    virtual bool should_go_off() = 0;

    virtual void stop() {};
    virtual void process() {};

    virtual void loop() {};

    void set_enabled(bool state) {
        this->enabled = state;
    }
    bool is_enabled() {
        return this->enabled;
    }

    #ifdef ENABLE_SCREEN
        virtual void make_menu_items(Menu *menu, int index) {}
    #endif
};


// track basic monophonic MIDI output
class MIDIBaseOutput : public BaseOutput {
    public:
    
    byte note_number = -1, last_note_number = -1;
    byte channel = GM_CHANNEL_DRUMS;
    byte event_value_1, event_value_2, event_value_3;

    MIDIOutputWrapper *output_wrapper = nullptr;

    MIDIBaseOutput(const char *label, byte note_number, MIDIOutputWrapper *output_wrapper) 
        : BaseOutput(label), note_number(note_number), output_wrapper(output_wrapper) {}

    virtual byte get_note_number() {
        return this->note_number;
    }
    virtual byte get_last_note_number() {
        return this->last_note_number;
    }
    virtual void set_last_note_number(byte note_number) {
        this->last_note_number = note_number;
    }
    virtual byte get_channel() {
        return this->channel;
    }

    virtual void stop() override {
        if(is_valid_note(last_note_number)) {
            output_wrapper->sendNoteOff(last_note_number, 0, this->get_channel());
            this->set_last_note_number(NOTE_OFF);
        }
    }
    virtual void process() override {
        if (should_go_off()) {
            int note_number = get_last_note_number();
            Debug_printf("\t\tgoes off note\t%i\t(%s), ", note_number, get_note_name_c(note_number));
            //Serial.printf("Sending note off for node %i on note_number %i chan %i\n", i, o->get_note_number(), o->get_channel());
            if (is_enabled() && is_valid_note(note_number)) {
                output_wrapper->sendNoteOff(note_number, 0, get_channel());
                set_last_note_number(NOTE_OFF);
                //this->went_off();
            }
        }
        if (should_go_on()) {
            this->stop();

            int note_number = get_note_number();
            Debug_printf("\t\tgoes on note\t%i\t(%s), ", note_number, get_note_name_c(note_number));
            //Serial.printf("Sending note on  for node %i on note_number %i chan %i\n", i, o->get_note_number(), o->get_channel());
            if (is_enabled() && is_valid_note(note_number)) {
                set_last_note_number(note_number);
                output_wrapper->sendNoteOn(note_number, MIDI_MAX_VELOCITY, get_channel());
                //this->went_on();
            }
            //count += i;
        }
    }

    virtual bool should_go_on() {
        if (this->event_value_1>0)
            return true;
        return false;
    }
    virtual bool should_go_off() {
        if (this->event_value_2>0)
            return true;
        return false;
    }
    virtual void went_on() {
        this->event_value_1 -= 1;
    }
    virtual void went_off() {
        this->event_value_2 -= 1;
    }

    // receive an event from a sequencer
    virtual void receive_event(byte event_value_1, byte event_value_2, byte event_value_3) override {
        this->event_value_1 += event_value_1;
        this->event_value_2 += event_value_2;
        this->event_value_3 += event_value_3;
    }

    // forget the last message
    virtual void reset() {
        this->event_value_1 = this->event_value_2 = this->event_value_3 = 0;
    }
};

// an output that tracks MIDI drum triggers
class MIDIDrumOutput : public MIDIBaseOutput {
    public:
    MIDIDrumOutput(const char *label, byte note_number, MIDIOutputWrapper *output_wrapper) 
        : MIDIBaseOutput(label, note_number, output_wrapper) {
        this->channel = GM_CHANNEL_DRUMS;
    }
    MIDIDrumOutput(const char *label, byte note_number, byte channel, MIDIOutputWrapper *output_wrapper) 
        : MIDIBaseOutput(label, note_number, output_wrapper) {
        this->channel = channel;
    }
};

#ifdef ENABLE_SCALES
    #include "scales.h"

    // class that counts up all active triggers from passed-in nodes, and calculates a note from that, for eg monophonic basslines
    class MIDINoteTriggerCountOutput : public MIDIBaseOutput {
        public:
            LinkedList<BaseOutput*> *nodes = nullptr;   // output nodes that will count towards the note calculation

            byte octave = 3;
            byte scale_root = SCALE_ROOT_A;
            SCALE scale_number = SCALE::MAJOR;
            //int base_note = scale_root * octave;

            MIDINoteTriggerCountOutput(const char *label, LinkedList<BaseOutput*> *nodes, MIDIOutputWrapper *output_wrapper, byte channel = 1, byte scale_root = SCALE_ROOT_A, SCALE scale_number = SCALE::MAJOR, byte octave = 3) 
                : MIDIBaseOutput(label, 0, output_wrapper) {
                this->channel = channel;
                this->nodes = nodes;

                this->octave = octave;
                this->scale_root = scale_root;
                this->scale_number = scale_number;
                //this->base_note = scale_root * octave;
            }

            int note_mode = 0;
            virtual byte get_note_number() override {
                if (!note_mode)
                    return get_note_number_count();
                else
                    return quantise_pitch(get_base_note() + BPM_CURRENT_BEAT_OF_PHRASE, scale_root, scale_number);
            }

            virtual byte get_note_number_count() {
                // count all the triggering notes and add that value ot the root note
                // then quantise according to selected scale to get final note number
                int count = 0;
                for (int i = 0 ; i < this->nodes->size() ; i++) {
                    BaseOutput *o = this->nodes->get(i);
                    if (o==this) continue;
                    count += o->should_go_on() ? (i%12) : 0;
                }
                Debug_printf("get_note_number in MIDINoteTriggerCountOutput is %i\n", count);
                //return base_note + quantise_pitch(count);

                // test mode, increment over 2 octaves to test scale quantisation
                // best used with pulses = 6 so that it loops round
                /*static int count = 0;
                count++;
                count %= 24;*/
                return quantise_pitch(get_base_note() + count, scale_root, scale_number);
            }

            virtual byte get_base_note() {
                //return this->scale_root * octave;
                return (octave * 12) + scale_root;
            }

            SCALE get_scale_number() {
                return scale_number;
            }
            void set_scale_number(SCALE scale_number) {
                this->scale_number = scale_number;
            }

            int get_scale_root() {
                return this->scale_root;
            }
            void set_scale_root(int scale_root) {
                this->scale_root = scale_root;
                //base_note = scale_root * octave;
            }

            void set_note_mode(bool mode) {
                this->note_mode = mode;
            }
            bool get_note_mode() {
                return this->note_mode;
            }

            #ifdef ENABLE_SCREEN
                virtual void make_menu_items(Menu *menu, int index) override;
            #endif

    };
#endif

#ifdef ENABLE_SCREEN
    void setup_output_menu();
#endif

void setup_output_parameters();

#endif