/*
 * VisitCard-Firmware_v3.c
 *
 * Created: 03.09.2018 16:32:05
 * Author : dmitry_shinelev
 *
 *
 * dot - unit of time
 * dash - 3 units
 * space between parts of the same letter is one unit
 * space between letters is three units
 * space between words is seven units
 *
 * speed 60-100 signs/minute
 *
 * 1 .----
 * 2 ..---
 * 3 ...--
 * 4 ....-
 *
 * in arrays . - is 0
 *           - - is 1
 */
 
// please see http://www.frank-zhao.com/card/ - take example from him

// required avr-libc modules, see http://www.nongnu.org/avr-libc/user-manual/modules.html
#include <avr/io.h> // allows access to AVR hardware registers
#include <avr/interrupt.h> // allows enabling/disabling and declaring interrupts
#include <util/delay.h> // includes delay functions
#include <avr/wdt.h> // allows enabling/disabling watchdog timer
#include <avr/pgmspace.h> // descriptor must be stored in flash memory
#include <avr/eeprom.h> // text file and calibration data is stored in EEPROM
#include <stdio.h> // allows streaming strings
#include <stdlib.h>
#include <stdint.h>
#include <avr/fuse.h>

// configure settings for V-USB then include the V-USB driver so V-USB uses your settings
#include "usbconfig.h"
#include "usbdrv.h"

//buttons library
#include "buttons.h"

//my files
#include "commons.h"


/****************************
* FUSES
****************************/
FUSES =
{
  .low = 0xFF,
  .high = HFUSE_DEFAULT,
};

// USB HID report descriptor for boot protocol keyboard
// see HID1_11.pdf appendix B section 1
// USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH is defined in usbconfig
PROGMEM const char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,                    // USAGE (Keyboard)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)(Key Codes)
    0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)(224)
    0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)(231)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs) ; Modifier byte
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x81, 0x03,                    //   INPUT (Cnst,Var,Abs) ; Reserved byte
    0x95, 0x05,                    //   REPORT_COUNT (5)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x05, 0x08,                    //   USAGE_PAGE (LEDs)
    0x19, 0x01,                    //   USAGE_MINIMUM (Num Lock)
    0x29, 0x05,                    //   USAGE_MAXIMUM (Kana)
    0x91, 0x02,                    //   OUTPUT (Data,Var,Abs) ; LED report
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x03,                    //   REPORT_SIZE (3)
    0x91, 0x03,                    //   OUTPUT (Cnst,Var,Abs) ; LED report padding
    0x95, 0x06,                    //   REPORT_COUNT (6)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x65,                    //   LOGICAL_MAXIMUM (101)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)(Key Codes)
    0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))(0)
    0x29, 0x65,                    //   USAGE_MAXIMUM (Keyboard Application)(101)
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
    0xc0                           // END_COLLECTION
};

// data structure for boot protocol keyboard report
// see HID1_11.pdf appendix B section 1
typedef struct {
	uint8_t modifier;
	uint8_t reserved;
	uint8_t keycode[6];
} keyboard_report_t;

// global variables
static keyboard_report_t keyboard_report;
#define keyboard_report_reset() keyboard_report.modifier=0;keyboard_report.reserved=0;keyboard_report.keycode[0]=0;keyboard_report.keycode[1]=0;keyboard_report.keycode[2]=0;keyboard_report.keycode[3]=0;keyboard_report.keycode[4]=0;keyboard_report.keycode[5]=0;
static uint8_t idle_rate = 500 / 4; // see HID1_11.pdf sect 7.2.4
static uint8_t protocol_version = 0; // see HID1_11.pdf sect 7.2.6
static uint8_t LED_state = 0; // see HID1_11.pdf appendix B section 1
static uint8_t blink_count = 0; // keep track of how many times caps lock have toggled

unsigned char key; //current key
uint8_t morze_count = 0; //variable-flag when guessing morse
uint8_t current_number = 0; //current number for morze code
uint8_t morze[4]; //array of morse code in RAM

uint8_t one_morse[] = {0, 1, 1, 1, 1}; //array for 1
uint8_t two_morse[] = {0, 0, 1, 1, 1}; //array for 2
uint8_t three_morse[] = {0, 0, 0, 1, 1}; //array for 3
uint8_t four_morse[] = {0, 0, 0, 0, 1}; //array for 4
uint16_t dotTime = 0xFA0;//0x1458;//0x3D09; //dec 15625 1 second for 16Mhz and prescaler 1024

//uint8_t seed EEMEM; //variable for function rand()
uint8_t Num EEMEM;//counter for number of mcu start

// see http://vusb.wikidot.com/driver-api
// constants are found in usbdrv.h
usbMsgLen_t usbFunctionSetup(uint8_t data[8])
{
	// see HID1_11.pdf sect 7.2 and http://vusb.wikidot.com/driver-api
	usbRequest_t *rq = (void *)data;

	if ((rq->bmRequestType & USBRQ_TYPE_MASK) != USBRQ_TYPE_CLASS)
		return 0; // ignore request if it's not a class specific request

	// see HID1_11.pdf sect 7.2
	switch (rq->bRequest)
	{
		case USBRQ_HID_GET_IDLE:
			usbMsgPtr = &idle_rate; // send data starting from this byte
			return 1; // send 1 byte
		case USBRQ_HID_SET_IDLE:
			idle_rate = rq->wValue.bytes[1]; // read in idle rate
			return 0; // send nothing
		case USBRQ_HID_GET_PROTOCOL:
			usbMsgPtr = &protocol_version; // send data starting from this byte
			return 1; // send 1 byte
		case USBRQ_HID_SET_PROTOCOL:
			protocol_version = rq->wValue.bytes[1];
			return 0; // send nothing
		case USBRQ_HID_GET_REPORT:
			usbMsgPtr = &keyboard_report; // send the report data
			return sizeof(keyboard_report);
		case USBRQ_HID_SET_REPORT:
			if (rq->wLength.word == 1) // check data is available
			{
				// 1 byte, we don't check report type (it can only be output or feature)
				// we never implemented "feature" reports so it can't be feature
				// so assume "output" reports
				// this means set LED status
				// since it's the only one in the descriptor
				return USB_NO_MSG; // send nothing but call usbFunctionWrite
			}
			else // no data or do not understand data, ignore
			{
				return 0; // send nothing
			}
		default: // do not understand data, ignore
			return 0; // send nothing
	}
}

// see http://vusb.wikidot.com/driver-api
usbMsgLen_t usbFunctionWrite(uint8_t * data, uchar len)
{
	if (data[0] != LED_state)
	{
		// increment count when LED has toggled
		blink_count = blink_count < 10 ? blink_count + 1 : blink_count;
	}
	
	LED_state = data[0];
  
  return 1; // 1 byte read
}

// translates ASCII to appropriate keyboard report, taking into consideration the status of caps lock
void ASCII_to_keycode(uint8_t ascii)
{
	keyboard_report.keycode[0] = 0x00;
	keyboard_report.modifier = 0x00;
	
	// see scancode.doc appendix C
	
	if (ascii >= 'A' && ascii <= 'Z')
	{
		keyboard_report.keycode[0] = 4 + ascii - 'A'; // set letter
		if (bit_is_set(LED_state, 1)) // if caps is on
		{
			keyboard_report.modifier = 0x00; // no shift
		}
		else
		{
			keyboard_report.modifier = _BV(1); // hold shift // hold shift
		}
	}
	else if (ascii >= 'a' && ascii <= 'z')
	{
		keyboard_report.keycode[0] = 4 + ascii - 'a'; // set letter
		if (bit_is_set(LED_state, 1)) // if caps is on
		{
			keyboard_report.modifier = _BV(1); // hold shift // hold shift
		}
		else
		{
			keyboard_report.modifier = 0x00; // no shift
		}
	}
	else if (ascii >= '0' && ascii <= '9')
	{
		keyboard_report.modifier = 0x00;
		if (ascii == '0')
		{
			keyboard_report.keycode[0] = 0x27;
		}
		else
		{
			keyboard_report.keycode[0] = 30 + ascii - '1'; 
		}
	}
	else
	{
		switch (ascii) // convert ascii to keycode according to documentation
		{
			case '!':
				keyboard_report.modifier = _BV(1); // hold shift
				keyboard_report.keycode[0] = 29 + 1;
				break;
			case '@':
				keyboard_report.modifier = _BV(1); // hold shift
				keyboard_report.keycode[0] = 29 + 2;
				break;
			case '#':
				keyboard_report.modifier = _BV(1); // hold shift
				keyboard_report.keycode[0] = 29 + 3;
				break;
			case '$':
				keyboard_report.modifier = _BV(1); // hold shift
				keyboard_report.keycode[0] = 29 + 4;
				break;
			case '%':
				keyboard_report.modifier = _BV(1); // hold shift
				keyboard_report.keycode[0] = 29 + 5;
				break;
			case '^':
				keyboard_report.modifier = _BV(1); // hold shift
				keyboard_report.keycode[0] = 29 + 6;
				break;
			case '&':
				keyboard_report.modifier = _BV(1); // hold shift
				keyboard_report.keycode[0] = 29 + 7;
				break;
			case '*':
				keyboard_report.modifier = _BV(1); // hold shift
				keyboard_report.keycode[0] = 29 + 8;
				break;
			case '(':
				keyboard_report.modifier = _BV(1); // hold shift
				keyboard_report.keycode[0] = 29 + 9;
				break;
			case ')':
				keyboard_report.modifier = _BV(1); // hold shift
				keyboard_report.keycode[0] = 0x27;
				break;
			case '~':
				keyboard_report.modifier = _BV(1); // hold shift
				// fall through
			case '`':
				keyboard_report.keycode[0] = 0x35;
				break;
			case '_':
				keyboard_report.modifier = _BV(1); // hold shift
				// fall through
			case '-':
				keyboard_report.keycode[0] = 0x2D;
				break;
			case '+':
				keyboard_report.modifier = _BV(1); // hold shift
				// fall through
			case '=':
				keyboard_report.keycode[0] = 0x2E;
				break;
			case '{':
				keyboard_report.modifier = _BV(1); // hold shift
				// fall through
			case '[':
				keyboard_report.keycode[0] = 0x2F;
				break;
			case '}':
				keyboard_report.modifier = _BV(1); // hold shift
				// fall through
			case ']':
				keyboard_report.keycode[0] = 0x30;
				break;
			case '|':
				keyboard_report.modifier = _BV(1); // hold shift
				// fall through
			case '\\':
				keyboard_report.keycode[0] = 0x31;
				break;
			case ':':
				keyboard_report.modifier = _BV(1); // hold shift
				// fall through
			case ';':
				keyboard_report.keycode[0] = 0x33;
				break;
			case '"':
				keyboard_report.modifier = _BV(1); // hold shift
				// fall through
			case '\'':
				keyboard_report.keycode[0] = 0x34;
				break;
			case '<':
				keyboard_report.modifier = _BV(1); // hold shift
				// fall through
			case ',':
				keyboard_report.keycode[0] = 0x36;
				break;
			case '>':
				keyboard_report.modifier = _BV(1); // hold shift
				// fall through
			case '.':
				keyboard_report.keycode[0] = 0x37;
				break;
			case '?':
				keyboard_report.modifier = _BV(1); // hold shift
				// fall through
			case '/':
				keyboard_report.keycode[0] = 0x38;
				break;
			case ' ':
				keyboard_report.keycode[0] = 0x2C;
				break;
			case '\t':
				keyboard_report.keycode[0] = 0x2B;
				break;
			case '\n':
				keyboard_report.keycode[0] = 0x28;
				break;
		}
	}
}

void send_report_once()
{
	// perform usb background tasks until the report can be sent, then send it
	while (1)
	{
		usbPoll(); // this needs to be called at least once every 10 ms
		if (usbInterruptIsReady())
		{
			usbSetInterrupt(&keyboard_report, sizeof(keyboard_report)); // send
			break;
			
			// see http://vusb.wikidot.com/driver-api
		}
	}
}

// stdio's stream will use this funct to type out characters in a string
void type_out_char(uint8_t ascii, FILE *stream)
{
	ASCII_to_keycode(ascii);
	send_report_once();
	keyboard_report_reset(); // release keys
	send_report_once();
}

static FILE mystdout = FDEV_SETUP_STREAM(type_out_char, NULL, _FDEV_SETUP_WRITE); // setup writing stream

void init_cpu() {

  //Init port B
  DDRB = 0;
  DDRB |= (1 << PB0)|(1 << PB1)|(1 << PB2)|(1 << PB4)|(1 << PB6); //Pin 0, 1, 2, 4, 6 - output
  PORTB &= ~((1 << PB0)|(1 << PB1)|(1 << PB2)|(1 << PB4)|(1 << PB6)); //Set to 0 on output

  //Init port C
  DDRC |= (1 << PC0)|(1 << PC1); //Pin 0,1 - output
  PORTC &= ~((1 << PC0)|(1 << PC1)); //Set to 0

  //Init port D
  DDRD |= (1 << PD1)|(1 << PD3)|(1 << PD4)|(1 << PD5)|(1 << PD6)|(1 << PD7); //Pin 1,3,4,5,6,7 - output
  PORTD &= ~((1 << PD1)|(1 << PD3)|(1 << PD4)|(1 << PD5)|(1 << PD6)); //Set to 0
  
}

void generate_full_code() {

  //next actions counts number of mcu starts
  uint8_t tmp = eeprom_read_byte(&Num); //read value from eeprom using address of Num
  if (tmp == 11) tmp = 0;
  tmp++;
  eeprom_write_byte(&Num, tmp); //write value of tmp to Num
  
  //generate first number of code
  if (tmp == 1) {    
    //4231
    morze[0] = 4;
    morze[1] = 2;
    morze[2] = 3;
    morze[3] = 1;

  }
  if (tmp == 2) {
    //2143
    morze[0] = 2;
    morze[1] = 1;
    morze[2] = 4;
    morze[3] = 3;
  }
  if (tmp == 3) {
    //3142
    morze[0] = 3;
    morze[1] = 1;
    morze[2] = 4;
    morze[3] = 2;
  }
  if (tmp == 4) {
    //1423
    morze[0] = 1;
    morze[1] = 4;
    morze[2] = 2;
    morze[3] = 3;
  }
  if (tmp == 5) {
    //4312
    morze[0] = 4;
    morze[1] = 3;
    morze[2] = 1;
    morze[3] = 2;
  }
  if (tmp == 6) {
    //1324
    morze[0] = 1;
    morze[1] = 3;
    morze[2] = 2;
    morze[3] = 4;
  }
  if (tmp == 7) {
    //3412
    morze[0] = 3;
    morze[1] = 4;
    morze[2] = 1;
    morze[3] = 2;
  }
  if (tmp == 8) {
    //2134
    morze[0] = 2;
    morze[1] = 1;
    morze[2] = 3;
    morze[3] = 4;
  }
  if (tmp == 9) {
    //4123
    morze[0] = 4;
    morze[1] = 1;
    morze[2] = 2;
    morze[3] = 3;
  }
  if (tmp == 10) {
    //3241
    morze[0] = 3;
    morze[1] = 2;
    morze[2] = 4;
    morze[3] = 1;
  }
}

void morse_led_on() {
  LED9_ON();
  LED10_ON();
  LED11_ON();
}

void morse_led_off() {
  LED9_OFF();
  LED10_OFF();
  LED11_OFF();
}

void true_led_off() {
  LED1_OFF();
  LED3_OFF();
  LED5_OFF();
  LED7_OFF();
}

void fake_led_off() {
  LED2_OFF();
  LED4_OFF();
  LED8_OFF();
  LED6_OFF();
}

void all_led_off() {
  LED1_OFF();
  LED2_OFF();
  LED3_OFF();
  LED4_OFF();
  LED5_OFF();
  LED6_OFF();
  LED7_OFF();
  LED8_OFF();
  LED9_OFF();
  LED10_OFF();
  LED11_OFF();
}

void all_led_on() {
  LED1_ON();
  LED2_ON();
  LED3_ON();
  LED4_ON();
  LED5_ON();
  LED6_ON();
  LED7_ON();
  LED8_ON();
  LED9_ON();
  LED10_ON();
  LED11_ON();
}

void timer0_init() {
  TCCR0 |= (1 << CS00)|(0 << CS01)|(1 << CS02); //prescaler 1024 - 16 ms
  TCNT0 = 100;
  TIMSK |= (1 << TOIE0); //enable overflow interrupt
}

//interrupt, pressed button
ISR(TIMER0_OVF_vect) {
  BUT_Debrief();
  usbPoll();
  TCNT0 = 100;
}

void timer1_init() {
  //init 16bit T1 Counter, 1 sec = TCNT < 15625
  TCCR1A = 0x00;
  TCCR1B = 0x05; //Prescaler 1024
  TCNT1 = 0x00;
}

/*
void timer2_init() {
  TCCR2 |= (1 << CS20)|(1 << CS21)|(1 << CS22);
  TCNT2 =0;
  TIMSK |= (1 << TOIE2); //enable overflow interrupt
}

ISR(TIMER2_OVF_vect){
  usbPoll();
  BUT_Debrief();
  check_button();
}*/

void check_win() {
  if (morze_count == 4) {        
    while(1) {
      usbPoll();
      all_led_off();
      _delay_ms(500);
      all_led_on();
      usbPoll();
      _delay_ms(500);
      usbPoll();      
    }    
  }
}

void check_button() {
  key = BUT_GetKey();
  check_win();
    switch (key) {
      case KEY_1: {
        if (current_number == 1) {
          fake_led_off();
          LED1_ON();          
          if (morze_count < 4) {
            morze_count++;
            current_number = morze[morze_count];
            send_key1_true();
          }
          } else {
          morze_count = 0;
          current_number = morze[0];
          true_led_off();
          fake_led_off();
          LED2_ON();
          send_key1_false();
        }
        usbPoll();
        break;
      }
      case KEY_2: {
        if (current_number == 2) {
          fake_led_off();
          LED3_ON();          
          if (morze_count < 4) {
            morze_count++;
            current_number = morze[morze_count];
            send_key2_true();
          }
          } else {
          morze_count = 0;
          current_number = morze[0];
          true_led_off();
          fake_led_off();
          LED4_ON();
          send_key2_false();
        }
        usbPoll();
        break;
      }
      case KEY_3: {
        if (current_number == 3) {
          fake_led_off();
          LED5_ON();         
          if (morze_count < 4) {
             morze_count++;
            current_number = morze[morze_count];
            send_key3_true();
          }
          } else {
          morze_count = 0;
          current_number = morze[0];
          true_led_off();
          fake_led_off();
          LED6_ON();
         send_key3_false();
        }
        usbPoll();
        break;
      }
      case KEY_4: {
        if (current_number == 4) {
          fake_led_off();
          LED7_ON();         
          if (morze_count < 4) {
            morze_count++;
            current_number = morze[morze_count];
            send_key4_true();
          }
          } else {
          morze_count = 0;
          current_number = morze[0];
          true_led_off();
          fake_led_off();
          LED8_ON();
          send_key4_false();
        }
        usbPoll();
        break;
      }
    } 
}

//prints address of RMCSoft if CAPS pressed
void print_address() {
  if (blink_count > 2) // activated by blinking lights
  {    
    //Text message
    //puts_P(PSTR("RMCSoft LLC\nCharlotte, NC\n933 Louise Ave., Suite 101S, Charlotte, NC 28204, USA\n(980) 201-2460\n\nNick Gritsenko\nnick@rmcsoft.com\n\nOlga Muller\nolga.z.muller@gmail.com"));
    //puts_P(PSTR("\nNext code will help you solve the quest.\nCode:"));
    puts_P(PSTR("Persistence is the quality of winners.\nThe following code will help you solve the quest.\n"));
    puts_P(PSTR("Code:"));
    for (char i = 0; i < 4; i++) {
      if (morze[i] == 1) puts_P(PSTR("1"));
      if (morze[i] == 2) puts_P(PSTR("2"));
      if (morze[i] == 3) puts_P(PSTR("3"));
      if (morze[i] == 4) puts_P(PSTR("4"));
    }

    blink_count = 0; // reset
  }
}

//function for internal work
void send_and_reset() {
  send_report_once();
  keyboard_report_reset(); // release keys
  send_report_once();
}

/********************************************
*next functions send statuses to browser
*/
void send_key1_true()
{ 
  keyboard_report.modifier = (1<<0) + (1<<2); //ctrl + alt
  keyboard_report.keycode[0] = 0x14; //q
  send_and_reset();
}

void send_key1_false()
{
  keyboard_report.modifier = (1<<0) + (1<<2); //ctrl + alt
  keyboard_report.keycode[0] = 0x1A; //w
  send_and_reset();
}

void send_key2_true()
{
  keyboard_report.modifier = (1<<0) + (1<<2); //ctrl + alt
  keyboard_report.keycode[0] = 0x08; //e
  send_and_reset();
}

void send_key2_false()
{
  keyboard_report.modifier = (1<<0) + (1<<2); //ctrl + alt
  keyboard_report.keycode[0] = 0x15; //r
  send_and_reset();
}

void send_key3_true()
{
  keyboard_report.modifier = (1<<0) + (1<<2); //ctrl + alt
  keyboard_report.keycode[0] = 0x17; //t
  send_and_reset();
}

void send_key3_false()
{
  keyboard_report.modifier = (1<<0) + (1<<2); //ctrl + alt
  keyboard_report.keycode[0] = 0x1C; //y
  send_and_reset();
}

void send_key4_true()
{
  keyboard_report.modifier = (1<<0) + (1<<2); //ctrl + alt
  keyboard_report.keycode[0] = 0x18; //u
  send_and_reset();
}

void send_key4_false()
{
  keyboard_report.modifier = (1<<0) + (1<<2); //ctrl + alt
  keyboard_report.keycode[0] = 0x0C; //i
  send_and_reset();
}

//send CTRL+ALT+A is alive status (connected to host)
void send_active_status() {
  keyboard_report.modifier = (1<<0) + (1<<2); //ctrl + alt
  keyboard_report.keycode[0] = 0x04; //a
  send_and_reset();
}

/****************************************************
* Morse functions
****************************************************/
//play full signal
void do_morse_signal() {
  for (char i = 0; i < 4; i++) {
    uint8_t symbol = morze[i];
    usbPoll();
    play_morse_symbol(symbol);
    check_button();
    usbPoll();
    play_morse_pause(3); //pause between letter
    check_button();
    usbPoll();
    send_active_status();
  }
  play_morse_pause(7); //pause between words
  check_button();
  usbPoll();
  send_active_status();
}

void play_morse_symbol(uint8_t symbol) {
  timer1_init();

  //uint16_t work_time = dotTime;
  switch (symbol) {
    case 1: {
      for (char z = 0; z < 6; z++) {
        check_button();
        if (one_morse[z] == 1) { //dash
          TCNT1 = 0x00;
          usbPoll();
          //while (TCNT1 < (3 * work_time)) {
          while (TCNT1 < MORSE_UNIT_MIDDLE) {
            //if (TCNT1 < 2 * work_time) {
            if (TCNT1 < MORSE_UNIT_MIDDLE) {
              LED10_ON();
            } else LED10_OFF();
            LED9_ON();
            usbPoll();
          }
          LED9_OFF();
        }
        if (one_morse[z] == 0) { //dot
          TCNT1 = 0x00;
          usbPoll();
          //while (TCNT1 < (3 * work_time)) {
          while (TCNT1 < MORSE_UNIT_STANDART) {
            //if (TCNT1 < 2 * work_time) {
            if (TCNT1 < MORSE_UNIT_SMALL) {
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
        check_button();
        if (two_morse[z] == 1) { //dash
          TCNT1 = 0x00;
          usbPoll();
          //while (TCNT1 < (3 * work_time)) {
          while (TCNT1 < MORSE_UNIT_MIDDLE) {
            //if (TCNT1 < 2 * work_time) {
            if (TCNT1 < MORSE_UNIT_MIDDLE) {
              LED10_ON();
            } else LED10_OFF();
            LED9_ON();
            usbPoll();
          }
          LED9_OFF();
        }
        if (two_morse[z] == 0) { //dot
          TCNT1 = 0x00;
          usbPoll();
          //while (TCNT1 < (3 * work_time)) {
          while (TCNT1 < MORSE_UNIT_STANDART) {
            //if (TCNT1 < 2 * work_time) {
            if (TCNT1 < MORSE_UNIT_SMALL) {
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
        check_button();
        if (three_morse[z] == 1) { //dash
          TCNT1 = 0x00;
          usbPoll();
          //while (TCNT1 < (3 * work_time)) {
          while (TCNT1 < MORSE_UNIT_MIDDLE) {
            //if (TCNT1 < 2 * work_time) {
            if (TCNT1 < MORSE_UNIT_MIDDLE) {
              LED10_ON();
            } else LED10_OFF();
            LED9_ON();
          }
          usbPoll();
          LED9_OFF();
        }
        if (three_morse[z] == 0) { //dot
          TCNT1 = 0x00;
          usbPoll();
          //while (TCNT1 < (3 * work_time)) {
          while (TCNT1 < MORSE_UNIT_STANDART) {
            //if (TCNT1 < 2 * work_time) {
            if (TCNT1 < MORSE_UNIT_SMALL) {
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
        check_button();
        if (four_morse[z] == 1) { //dash
          TCNT1 = 0x00;
          usbPoll();
          //while (TCNT1 < (3 * work_time)) {
          while (TCNT1 < MORSE_UNIT_MIDDLE) {
            //if (TCNT1 < 2 * work_time) {
            if (TCNT1 < MORSE_UNIT_MIDDLE) {
              LED10_ON();
            } else LED10_OFF();
            LED9_ON();
          }
          usbPoll();
          LED9_OFF();
        }
        if (four_morse[z] == 0) { //dot
          TCNT1 = 0x00;
          usbPoll();
          //while (TCNT1 < (3 * work_time)) {
          while (TCNT1 < MORSE_UNIT_STANDART) {
            //if (TCNT1 < 2 * work_time) {
            if (TCNT1 < MORSE_UNIT_SMALL) {
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
  uint16_t counterValue = dotTime;
  usbPoll();
  switch (pause) {
    case 1: {
      while (TCNT1 < counterValue) {
        usbPoll();
      } //pause for 1 second
      //usbPoll();
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

void timeout_blinking() {
  _delay_ms(500);
  for (char y = 0; y < 3; y++) {
    morse_led_on();
    _delay_ms(200);
    morse_led_off();
    _delay_ms(200);
  }
  _delay_ms(500);
}

int main()
{	
  //init ports of CPU
  init_cpu();

  //inits buttons via library
  BUT_Init(); 

  //init timer0 for button library
  timer0_init();

  //init timer2 for check button
  //timer2_init();  

  generate_full_code();

  current_number = morze[0]; //set first morse number as default
  
	stdout = &mystdout; // set default stream
	
	// initialize report (I never assume it's initialized to 0 automatically)
	keyboard_report_reset();
	
	wdt_disable(); // disable watchdog, good habit if you don't use it
	
	// enforce USB re-enumeration by pretending to disconnect and reconnect
	usbDeviceDisconnect();
	_delay_ms(250);
	usbDeviceConnect();
	
	// initialize various modules
	usbInit();
	
	sei(); // enable interrupts

	while (1) // main loop, do forever
	{ 
  
    timeout_blinking(); 

    check_button();

    send_active_status();
    
    usbPoll();

		print_address();                
    
    do_morse_signal();
		
	}
	
	return 0;
}