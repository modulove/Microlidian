#include <Arduino.h>
#include <LinkedList.h>

// class to receive triggers from a sequencer and do something with them
class BaseOutput {
    public:

    virtual void receive_event(byte event_value_1, byte event_value_2, byte event_value_3) = 0;
    virtual void reset() = 0;
};

#include <Adafruit_TinyUSB.h>
#include "MIDI.h"
#include "Drums.h"
#include "bpm.h"

#include "sequencer/Sequencer.h"

#include "midi_helpers.h"

class MIDIDrumOutput : public BaseOutput {
    public:

    byte note_number = -1, last_note_number = -1;
    byte channel = 10;
    byte event_value_1, event_value_2, event_value_3;

    MIDIDrumOutput(byte note_number) {
        this->note_number = note_number;
    }

    virtual byte get_note_number() {
        return this->note_number;
    }
    virtual byte get_last_note_number() {
        return this->last_note_number;
    }
    virtual byte set_last_note_number(byte note_number) {
        this->last_note_number = note_number;
    }
    virtual byte get_channel() {
        return this->channel;
    }

    virtual void receive_event(byte event_value_1, byte event_value_2, byte event_value_3) override {
        this->event_value_1 += event_value_1;
        this->event_value_2 += event_value_2;
        this->event_value_3 += event_value_3;
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

    virtual void reset() {
        this->event_value_1 = this->event_value_2 = this->event_value_3 = 0;
    }
};

class MIDINoteOutput : public MIDIDrumOutput {
    public:
        LinkedList<MIDIDrumOutput*> *nodes = nullptr;

        MIDINoteOutput(LinkedList<MIDIDrumOutput*> *nodes) : MIDIDrumOutput(0) {
            this->channel = 1;
            this->nodes = nodes;
        }

        virtual byte get_note_number() override {
            int count = 0;
            for (int i = 0 ; i < this->nodes->size() ; i++) {
                MIDIDrumOutput *o = this->nodes->get(i);
                if (o==this) continue;
                count += o->should_go_on() ? i : 0;
            }
            Serial.printf("get_note_number in MIDINoteOutput is %i\n", count);
            return count;
        }
};

class MIDIOutputProcessor {
    public:

    LinkedList<MIDIDrumOutput*> nodes = LinkedList<MIDIDrumOutput*>();
    midi::MidiInterface<midi::SerialMIDI<Adafruit_USBD_MIDI>> *midi = nullptr;

    MIDIOutputProcessor(midi::MidiInterface<midi::SerialMIDI<Adafruit_USBD_MIDI>> *midi) {
        this->midi = midi;

        this->nodes.add(new MIDIDrumOutput(GM_NOTE_ACOUSTIC_BASS_DRUM));
        this->nodes.add(new MIDIDrumOutput(GM_NOTE_ACOUSTIC_SNARE));
        this->nodes.add(new MIDIDrumOutput(GM_NOTE_OPEN_HI_HAT));
        this->nodes.add(new MIDIDrumOutput(GM_NOTE_PEDAL_HI_HAT));
        this->nodes.add(new MIDIDrumOutput(GM_NOTE_CLOSED_HI_HAT));
        this->nodes.add(new MIDINoteOutput(&this->nodes));
    }

    //virtual void on_tick(uint32_t ticks) {
        //if (is_bpm_on_sixteenth(ticks)) {
    virtual void process() {
        Serial.println("process-->");
        static int count = 0;
        midi->sendNoteOff(35 + count, 0, 1);
        count = 0;
        for (int i = 0 ; i < this->nodes.size() ; i++) {
            MIDIDrumOutput *o = this->nodes.get(i);
            Serial.printf("\tnode %i\n", i);
            if (o->should_go_off()) {
                int note_number = o->get_last_note_number();
                Serial.printf("\t\tgoes off note %i (%s), ", note_number, get_note_name_c(note_number));
                //Serial.printf("Sending note off for node %i on note_number %i chan %i\n", i, o->get_note_number(), o->get_channel());
                midi->sendNoteOff(note_number, 0, o->get_channel());
                //this->nodes.get(i)->went_off();
            }
            if (o->should_go_on()) {
                int note_number = o->get_note_number();
                Serial.printf("\t\tgoes on note %i (%s), ", note_number, get_note_name_c(note_number));
                //Serial.printf("Sending note on  for node %i on note_number %i chan %i\n", i, o->get_note_number(), o->get_channel());
                o->set_last_note_number(note_number);
                midi->sendNoteOn(note_number, MIDI_MAX_VELOCITY, o->get_channel());
                //this->nodes.get(i)->went_on();
                //count += i;
            }
            Serial.println();
        }
        /*if (count>0) {
            Serial.printf("sending combo note %i\n", count);
            midi->sendNoteOn(35 + count, 127, 1);
            //count = 35;
        }*/

        for (int i = 0 ; i < this->nodes.size() ; i++) {
            this->nodes.get(i)->reset();
        }

        Serial.println(".end.");
    }

    virtual void configure_sequencer(BaseSequencer *sequencer) {
        for (int i = 0 ; i < this->nodes.size() ; i++) {
            sequencer->configure_pattern_output(i, this->nodes.get(i));
        }
        //sequencer->configure_pattern_output(0, this->nodes.get(0));
    }
};