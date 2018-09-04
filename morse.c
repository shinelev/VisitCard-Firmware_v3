/*
 * morse.c
 * Contains Morse code for leds
 *
 * Created: 01.08.2018 15:15:55
 *  Author: dmitry_shinelev
 *
 * dot - unit of time
 * dash - 3 units
 * space between parts of the same letter is one unit
 * space between letters is three units
 * space between words is seven units
 *
 * speed 60-100 signs/minute, dot = 1 second in this project
 *
 * 1 .----
 * 2 ..---
 * 3 ...--
 * 4 ....-
 *
 * in arrays . - is 0
 *           - - is 1
 */

#include "morse.h"

/******************************************************************************************
* Prototypes
*/ 
 //play pause between symbol, letter, word
void play_morse_pause(char pause); 

 //play symbol
void play_morse_symbol(uint8_t symbol); 

uint8_t const one_morse[] EEMEM = {0, 1, 1, 1, 1}; //array for 1
uint8_t const two_morse[] EEMEM = {0, 0, 1, 1, 1}; //array for 2
uint8_t const three_morse[] EEMEM = {0, 0, 0, 1, 1}; //array for 3
uint8_t const four_morse[] EEMEM = {0, 0, 0, 0, 1}; //array for 4
uint16_t const dotTime EEMEM = 0xFA0;//0x1458;//0x3D09; //dec 15625 1 second for 16Mhz and prescaler 1024

//play full signal
 void do_morse_signal(uint8_t *pCode) {
  for (char i = 0; i < 5; i++) {
    uint8_t symbol = eeprom_read_byte(&pCode[i]);
    usbPoll();
    play_morse_symbol(symbol);
    check_button();
    usbPoll();
    play_morse_pause(3); //pause between letter
    check_button();
    usbPoll();
  }
  play_morse_pause(7); //pause between words
  check_button();
  usbPoll();
}

 void play_morse_symbol(uint8_t symbol) {
  //Timer1 init
  timer1_init();
  uint16_t work_time = eeprom_read_word(&dotTime);
  switch (symbol) {
    case 1: {
      for (char z = 0; z < 6; z++) {
        if (eeprom_read_byte(one_morse[z]) == 1) { //dash
          TCNT1 = 0x00;
          usbPoll();          
          while (TCNT1 < (3 * work_time)) {
            if (TCNT1 < 2 * work_time) {
              LED10_ON();
            } else LED10_OFF();           
            LED9_ON();
            usbPoll();
          }
          LED9_OFF();          
        }
        if (eeprom_read_byte(one_morse[z]) == 0) { //dot
          TCNT1 = 0x00;
          usbPoll();
          while (TCNT1 < (3 * work_time)) {
             if (TCNT1 < 2 * work_time) {
               LED10_ON();
             } else LED10_OFF();
            LED9_ON();
            usbPoll();
          }
          LED9_OFF();
        }
        LED11_ON();
        play_morse_pause(1);
        LED11_OFF();
      }
      break;
    }
    case 2: {
       for (char z = 0; z < 6; z++) {
         if (eeprom_read_byte(two_morse[z]) == 1) { //dash
           TCNT1 = 0x00;
           usbPoll();
           while (TCNT1 < (3 * work_time)) {
             if (TCNT1 < 2 * work_time) {
                LED10_ON();
             } else LED10_OFF(); 
             LED9_ON();
             usbPoll();
           }
           LED9_OFF();
         }
         if (eeprom_read_byte(two_morse[z]) == 0) { //dot
           TCNT1 = 0x00;
           usbPoll();
           while (TCNT1 < (3 * work_time)) {
             if (TCNT1 < 2 * work_time) {
               LED10_ON();
             } else LED10_OFF(); 
             LED9_ON();
           }
           usbPoll();
           LED9_OFF();
         }
         LED11_ON();
         play_morse_pause(1);
         LED11_OFF();
       }      
      break;
    }
    case 3: {
       for (char z = 0; z < 6; z++) {
         if (eeprom_read_byte(three_morse[z]) == 1) { //dash
           TCNT1 = 0x00;
           usbPoll();
           while (TCNT1 < (3 * work_time)) {
             if (TCNT1 < 2 * work_time) {
               LED10_ON();
             } else LED10_OFF(); 
             LED9_ON();
           }
           usbPoll();
           LED9_OFF();
         }
         if (eeprom_read_byte(three_morse[z]) == 0) { //dot
           TCNT1 = 0x00;
           usbPoll();
           while (TCNT1 < (3 * work_time)) {
             if (TCNT1 < 2 * work_time) {
               LED10_ON();
             } else LED10_OFF(); 
             LED9_ON();
           }
           usbPoll();
           LED9_OFF();
         }
         LED11_ON();
         play_morse_pause(1);
         LED11_OFF();
       }
      break;
    }
    case 4: {
       for (char z = 0; z < 6; z++) {
         if (eeprom_read_byte(four_morse[z]) == 1) { //dash
           TCNT1 = 0x00;
           usbPoll();
           while (TCNT1 < (3 * work_time)) {
             if (TCNT1 < 2 * work_time) {
               LED10_ON();
             } else LED10_OFF();
             LED9_ON();
           }
           usbPoll();
           LED9_OFF();
         }
         if (eeprom_read_byte(four_morse[z]) == 0) { //dot
           TCNT1 = 0x00;
           usbPoll();
           while (TCNT1 < (3 * work_time)) {
             if (TCNT1 < 2 * work_time) {
               LED10_ON();
             } else LED10_OFF();
             LED9_ON();
           }
           usbPoll();
           LED9_OFF();
         }
         LED11_ON();
         play_morse_pause(1);
         LED11_OFF();
       }      
      break;
    }
  }  
}

void play_morse_pause(char pause) {
  TCNT1 = 0x00;
  uint16_t counterValue = eeprom_read_word(&dotTime);
  usbPoll();
  switch (pause) {
    case 1: {
      while (TCNT1 < counterValue) {
      usbPoll();
      } //pause for 1 second      
      break;
    }
    case 3: { //pause for 1 second
      for (char i = 1; i <= pause; i++) {
        while (TCNT1 < counterValue) {
        LED10_ON();
        usbPoll();
        }        
      }
      LED10_OFF();           
      break;
    }
    case 7: { //pause for 3 second
      for (char i = 1; i <= pause; i++) {
        while (TCNT1 < counterValue) {
        usbPoll();
        }
      }
      break;
    }
  }
}



