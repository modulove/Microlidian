/*#ifndef ENABLE_SCREEN
    #define ENABLE_SCREEN       // tft
#endif*/
#ifdef ENABLE_SCREEN
    #define DisplayTranslator_Configured DisplayTranslator_ST7789
    //#ifndef TFT_ST7789_T3
    //    #define TFT_ST7789_T3
    //    #define TFT_ST7789_T3_BIG
    //#endif
    #define MENU_MS_BETWEEN_REDRAW  75
#endif

/*#ifndef ENABLE_SCREEN
    #define tft_print(X) { Serial.println(X) }
#endif*/

#ifdef ENABLE_SCREEN
    #define ENCODER_STEP_DIVISOR    4
    #define PIN_BUTTON_A    4   // pin to receive encoder button
    #define PIN_BUTTON_B    5   // pin to receive back button
    #define PIN_BUTTON_C    26  // pin to receive right-hand / save button
    #define ENCODER_KNOB_L  2   // pin to receive left-encoder pulses
    #define ENCODER_KNOB_R  3   // pin to receive right-encoder pulses
#endif