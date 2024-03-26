// framework
#include <Arduino.h>

// needed for set_sys_clock_khz
#include "pico/stdlib.h"

// our app config
#include "Config.h"
#include "BootConfig.h"

#include "debug.h"

// midihelpers library clock handling
#include <clock.h>
#include <bpm.h>
//#include "midi_usb/midi_usb_rp2040.h"

#include "sequencer/sequencing.h"

#ifdef ENABLE_STORAGE
    #include "storage/storage.h"
#endif

#ifdef ENABLE_SCREEN
    #include "mymenu/screen.h"
#endif

#ifdef ENABLE_CV_INPUT
    #include "cv_input.h"
    #ifdef ENABLE_CLOCK_INPUT_CV
        #include "cv_input_clock.h"
    #endif
#endif

#include "outputs/output_processor.h"

#include "core_safe.h"

#include <atomic>
#include <SimplyAtomic.h>

#ifdef USE_UCLOCK_GENERIC
    void uClockCheckTime(uint32_t micros_time);
#endif

std::atomic<bool> started = false;
std::atomic<bool> ticked = false;

void do_tick(uint32_t ticks);

// serial console to host, for debug etc
void setup_serial() {
    #ifdef WAIT_FOR_SERIAL
        Serial.begin(115200);
        Serial.setTimeout(0);

        while(!Serial) {};
    #endif
}

// call this when global clock should be reset
void global_on_restart() {
  set_restart_on_next_bar(false);

  //Serial.println(F("on_restart()==>"));
  clock_reset();
  last_processed_tick = -1;
  
  //send_midi_serial_stop_start();
  //behaviour_manager->on_restart();
  //Serial.println(F("<==on_restart()"));
}

void auto_handle_start();

void auto_handle_start_wrapper() {
    messages_log_add("auto_handle_start_wrapper()!");
    auto_handle_start();
}

void setup() {
    // overclock the CPU so that we can afford all those CPU cycles drawing the UI!
    //set_sys_clock_khz(225000, true);
    //set_sys_clock_khz(230000, true);
    set_sys_clock_khz(200000, true);

    ticked = false;
    started = false;

    setup_serial();
    Debug_println("setup() starting");

    #ifdef USE_UCLOCK
        setup_uclock(do_tick, uClock.PPQN_24);
    #else
        setup_cheapclock();
    #endif
    set_global_restart_callback(global_on_restart);

    #if defined(ENABLE_CV_INPUT) && defined(ENABLE_CLOCK_INPUT_CV)
        set_check_cv_clock_ticked_callback(actual_check_cv_clock_ticked);
        set_clock_mode_changed_callback(clock_mode_changed);
    #endif

    setup_midi();
    setup_usb();
    USBMIDI.setHandleStart(auto_handle_start_wrapper);
    setup_output();

    #ifdef ENABLE_CV_INPUT
        setup_cv_input();
        Debug_printf("after setup_cv_input(), free RAM is %u\n", freeRam());
        setup_parameter_inputs();
        Debug_printf("after setup_parameter_inputs(), free RAM is %u\n", freeRam());
        setup_output_parameters();
    #endif

    #ifdef ENABLE_SCREEN
        //delay(1000);    // see if giving 1 second to calm down will help reliability of screen initialisation... it does not. :(
        setup_screen();
        Debug_printf("after setup_screen(), free RAM is %u\n", freeRam());
    #endif

    // check if the two buttons are held down, if so, enter firmware reset mode as quickly as possible!
    #ifdef ENABLE_SCREEN
        pushButtonA.update(); 
        pushButtonB.update();
        if (pushButtonA.read() && pushButtonB.read()) {
            reset_upload_firmware();
        }
    #endif

    #ifdef ENABLE_STORAGE
        setup_storage();
        #ifdef ENABLE_SCREEN
            setup_storage_menu();
        #endif
        Debug_printf("after setup_storage(), free RAM is %u\n", freeRam());
    #endif    

    #ifdef ENABLE_EUCLIDIAN
        //Serial.println("setting up sequencer..");
        setup_sequencer();
        output_processor->configure_sequencer(&sequencer);
        #ifdef ENABLE_SCREEN
            setup_sequencer_menu();
            Debug_printf("after setup_sequencer_menu, free RAM is %u\n", freeRam());
        #endif
    #endif

    #if defined(ENABLE_CV_INPUT) && defined(ENABLE_EUCLIDIAN)
        //Serial.println("..calling sequencer.getParameters()..");
        LinkedList<FloatParameter*> *params = sequencer.getParameters();
        //Serial.println("calling parameter_manager->addParameters.."); Serial.flush();
        //parameter_manager->addParameters(params);
        //params->clear();
        //delete params;
        Debug_printf("after setting up sequencer parameters, free RAM is %u\n", freeRam());
    #endif


    #if defined(ENABLE_SCREEN) && defined(ENABLE_CV_INPUT)
        //menu->add_page("Parameter Inputs");
        Debug_printf("before setup_parameter_menu(), free RAM is %u\n", freeRam());
        setup_parameter_menu();
        Debug_printf("after setup_parameter_menu(), free RAM is %u\n", freeRam());
    #endif

    #ifdef ENABLE_CV_INPUT
        parameter_manager->setDefaultParameterConnections();
    #endif

    #ifdef ENABLE_SCREEN
        setup_output_menu();
        setup_debug_menu();
        menu->setup_quickjump();

        menu->select_page(0);
    #endif

    //debug_free_ram();

    started = true;

    #ifdef DEBUG_ENVELOPES
        set_bpm(10);
    #endif

    #ifdef ENABLE_SCREEN
    menu->set_last_message((String("Started up, free RAM is ") + String(freeRam())).c_str());
    #endif

    Debug_printf("at end of setup(), free RAM is %u\n", freeRam());

    Debug_println("setup() finished!");

    #ifdef USE_UCLOCK
        //if (Serial) Serial.println("Starting uClock...");
        //Serial_flush();
        clock_reset();
        clock_start();
        //if (Serial) Serial.println("Started uClock!");
        //Serial_flush();
    #endif
    started = true;
}

//volatile bool ticked = false;

#define LOOP_AVG_COUNT 50
uint_fast32_t loop_averages[LOOP_AVG_COUNT];
uint_fast32_t loop_average = 0;
uint_fast8_t pos = 0;
void add_loop_length(int length) {
    loop_averages[pos] = length;
    pos++;
    if (pos>=LOOP_AVG_COUNT) {
        uint_fast32_t c = 0;
        for (uint_fast32_t i = 0 ; i < LOOP_AVG_COUNT ; i++) {
            c += loop_averages[i];
        }
        loop_average = c / LOOP_AVG_COUNT;
        pos = 0;
    }
}

void read_serial_buffer() {
    //uint32_t interrupts = save_and_disable_interrupts();
    if (Serial) {
        Serial.read();
        Serial.clearWriteError();
    }
    //restore_interrupts(interrupts);
}

bool menu_tick_pending = false;
//uint32_t menu_tick_pending_tick = -1;

void do_tick(uint32_t in_ticks) {
    #ifdef USE_UCLOCK
        ::ticks = in_ticks;
    #endif
    //if (Serial) Serial.printf("ticked %u\n", ticks);
    if (is_restart_on_next_bar() && is_bpm_on_bar(ticks)) {
        //if (debug) Serial.println(F("do_tick(): about to global_on_restart"));
        global_on_restart();

        set_restart_on_next_bar(false);
    }

    output_wrapper->sendClock();

    #ifdef ENABLE_EUCLIDIAN
        sequencer.on_tick(ticks);
        if (is_bpm_on_sixteenth(ticks)) {
            output_processor->process();
        }
    #endif

}

void loop() {
    uint32_t mics_start = micros();
    //Serial.println("loop()");
    
    // doing this on the second core means that two Microlidians powered up at the same time won't drift too much
    // however, it causes crashes...
    #ifndef PROCESS_USB_ON_SECOND_CORE
        #ifdef USE_TINYUSB
            ATOMIC()
            {
                USBMIDI.read();
            }
        #endif
    #endif

    ATOMIC() 
    {
        ticked = update_clock_ticks();
    }

    #ifdef USE_UCLOCK
        // do_tick is called from interrupt via uClock, so we don't need to do it manually here
        #ifdef USE_UCLOCK_GENERIC
            uClockCheckTime(micros());
        #endif
    #else
        if (ticked) {
            ATOMIC() {
                do_tick(ticks);
            }
        }
    #endif

    #ifdef ENABLE_SCREEN
        static uint32_t last_tick = -1;
        static unsigned long last_drawn;
        /*if (ticked) {         // block updating menu if its currently being updated - works but very high BPMs (like 3000bpm!) have a lot of jitter
            while(locked) {}
            menu->update_ticks(ticks);
            last_tick = ticks;
        }*/
        ATOMIC() 
        {
            if ((ticked || menu_tick_pending) && !is_locked()) {     // don't block, assume that we can make up for the missed tick next loop; much less jitter when at very very high BPMs
                //acquire_lock();
                menu->update_ticks(ticks);
                //release_lock();
                last_tick = ticks;
                menu_tick_pending = false;
            } else if (ticked && is_locked()) {
                menu_tick_pending = true;
                //menu->update_ticks(ticks);
                //last_tick = ticks;
            }
        }
    #endif

    ATOMIC() 
    {
        output_processor->loop();
    }

    ATOMIC() 
    {
        if (playing && clock_mode==CLOCK_INTERNAL && last_ticked_at_micros>0 && micros() + loop_average >= last_ticked_at_micros + micros_per_tick) {
            // don't process anything else this loop, since we probably don't have time before the next tick arrives
            //Serial.printf("early return because %i + %i >= %i + %i\n", micros(), loop_average, last_ticked_at_micros, micros_per_tick);
            //Serial.flush();
        } else {
            //read_serial_buffer();

            #ifdef ENABLE_SCREEN
            if (!is_locked()) {
                //acquire_lock();
                menu->update_inputs();
                //release_lock();
            }
            #endif

            add_loop_length(micros()-mics_start);
        }
    }

    ATOMIC() 
    {
        // if the back button is held down for 4 seconds, do a soft reboot
        #ifdef ENABLE_SCREEN
        if (!pushButtonA.isPressed() && pushButtonB.isPressed() && pushButtonB.currentDuration() >= 4000) {
            //#define AIRCR_Register (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C)))
            //AIRCR_Register = 0x5FA0004;
            reset_rp2040();
        } else if (
            pushButtonA.isPressed() && pushButtonB.isPressed() && pushButtonA.currentDuration() >= 3000 && pushButtonA.currentDuration() >= 4000
        ) {
            reset_upload_firmware();
        }
        #endif
    }
}

