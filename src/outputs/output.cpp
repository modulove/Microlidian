#include "Drums.h"
#include "outputs/output.h"
#include "outputs/output_processor.h"

byte get_muso_note_for_drum(byte drum_note) {
    byte retval = 60;
    switch (drum_note) {
        case GM_NOTE_ACOUSTIC_BASS_DRUM:
        case GM_NOTE_ELECTRIC_BASS_DRUM:
            retval += TRIGGER_SIDESTICK; break;
        case GM_NOTE_HAND_CLAP:
            retval += TRIGGER_CLAP; break;
        case GM_NOTE_ACOUSTIC_SNARE:
        case GM_NOTE_ELECTRIC_SNARE:
            retval += TRIGGER_SNARE; break;
        case GM_NOTE_CRASH_CYMBAL_1:
            retval += TRIGGER_CRASH_1; break;
        case GM_NOTE_TAMBOURINE:
            retval += TRIGGER_TAMB; break;
        case GM_NOTE_LOW_TOM:
            retval += TRIGGER_LOTOM; break;
        case GM_NOTE_HIGH_TOM:
            retval += TRIGGER_HITOM; break;
        case GM_NOTE_PEDAL_HI_HAT:
            retval += TRIGGER_PEDALHAT; break;
        case GM_NOTE_OPEN_HI_HAT:
            retval += TRIGGER_OPENHAT; break;
        case GM_NOTE_CLOSED_HI_HAT:
            retval += TRIGGER_CLOSEDHAT; break;
    }     
    return retval;
}

MIDIOutputWrapper *output_wrapper = nullptr;
MIDIOutputProcessor *output_processor = nullptr;

void setup_output() {
    output_wrapper = new MIDIOutputWrapper();
    output_processor = new MIDIOutputProcessor(output_wrapper);     // todo: set this up dynamically, probably reading from a config file
}

void setup_output_parameters() {
    output_wrapper->setup_parameters();
}

#ifdef ENABLE_SCREEN
    #include "mymenu.h"
    #include "menuitems_object_multitoggle.h"

    void setup_output_menu() {
        output_wrapper->create_menu_items();
        output_processor->create_menu_items();
    }

    void MIDIOutputProcessor::create_menu_items() {
        for (unsigned int i = 0 ; i < this->nodes->size() ; i++) {
            BaseOutput *node = this->nodes->get(i);
            node->make_menu_items(menu, i);
        }

        menu->add_page("Outputs");
        ObjectMultiToggleControl *toggle = new ObjectMultiToggleControl("Enable outputs", true);
        for (unsigned int i = 0 ; i < this->nodes->size() ; i++) {
            BaseOutput *output = this->nodes->get(i);
            //menu->add(new ToggleControl(output->label, output->disabled));
            // options for whether to auto-advance looper/sequencer/beatstep

            MultiToggleItemClass<BaseOutput> *option = new MultiToggleItemClass<BaseOutput> (
                output->label,
                output,
                &BaseOutput::set_enabled,
                &BaseOutput::is_enabled
            );
            toggle->addItem(option);
        }
        menu->add(toggle);
    }

    #include "submenuitem_bar.h"
    #include "menuitems_object.h"
    #include "menuitems.h"
    void MIDINoteTriggerCountOutput::make_menu_items(Menu *menu, int index) {
        //#ifdef ENABLE_ENVELOPE_MENUS
            char label[40];
            snprintf(label, 40, "MIDINoteOutput %i: %s", index, this->label);
            menu->add_page(label);

            SubMenuItemColumns *sub_menu_item_columns = new SubMenuItemColumns("Options", 2);
            sub_menu_item_columns->add(new ObjectToggleControl("Note mode", this, &MIDINoteTriggerCountOutput::set_note_mode, &MIDINoteTriggerCountOutput::get_note_mode));
            sub_menu_item_columns->add(new DirectNumberControl<byte>("Octave", &this->octave, this->octave, (byte)0, (byte)10));

            menu->add(sub_menu_item_columns);
        //#endif
    }

    void MIDIOutputWrapper::create_menu_items() {
        // controls for cv-to-midi outputs..
        menu->add_page("CV-to-MIDI");
        char label[40];
        for (int i = 0 ; i < 6 ; i++) {
            snprintf(label, 40, "CC Output %s", midi_cc_parameters[i].label);
            SubMenuItem *bar = new SubMenuItemBar(label);

            snprintf(label, 40, "Output %s CC", midi_cc_parameters[i].label);            
            bar->add(new DirectNumberControl<byte>(
                label, 
                &midi_cc_parameters[i].cc_number,
                midi_cc_parameters[i].cc_number,
                0,
                127
            ));

            snprintf(label, 40, "Output %s Channel", midi_cc_parameters[i].label);            
            bar->add(new DirectNumberControl<byte>(
                label, 
                &midi_cc_parameters[i].channel,
                midi_cc_parameters[i].channel,
                1,
                16
            ));

            menu->add(bar);
        }
    }

#endif