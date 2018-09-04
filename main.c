/*
 * VisitCard-Firmware_v3.c
 *
 * Created: 03.09.2018 16:32:05
 * Author : dmitry_shinelev
 */
 
// please see http://www.frank-zhao.com/card/
// note, for ATtiny MCUs, fuses -U lfuse:w:0xE1:m -U hfuse:w:0xDF:m -U efuse:w:0xFF:m -U lock:w:0xFF:m
// note, write to ATtiny at a low ISP frequency

// required avr-libc modules, see http://www.nongnu.org/avr-libc/user-manual/modules.html
#include <avr/io.h> // allows access to AVR hardware registers
#include <avr/interrupt.h> // allows enabling/disabling and declaring interrupts
#include <util/delay.h> // includes delay functions
#include <avr/wdt.h> // allows enabling/disabling watchdog timer
#include <avr/pgmspace.h> // descriptor must be stored in flash memory
#include <avr/eeprom.h> // text file and calibration data is stored in EEPROM
#include <stdio.h> // allows streaming strings
#include <stdlib.h>

// configure settings for V-USB then include the V-USB driver so V-USB uses your settings
#include "usbconfig.h"
#include "usbdrv.h"

//buttons library
#include "buttons.h"

//my files
#include "commons.h"
#include "morse.h"

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

uint8_t seed EEMEM; //variable for function rand()
uint8_t Num EEMEM;//counter for number of mcu start
uint8_t morze[4] EEMEM; //array of morse code in eeprom

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
  //DDRC = 0; //Input
  DDRC |= (1 << PC0)|(1 << PC1); //Pin 0,1 - output
  PORTC &= ~((1 << PC0)|(1 << PC1)); //Set to 0
  //PORTC |= (1 << PC2)|(1 << PC3)|(1 << PC4)|(1 << PC5); //Enable pull-up, don't use because there is button library
}

void generate_full_code(uint8_t tmp) {
  int randomValue = 0;
  srand(tmp);
  randomValue = rand();
  uint8_t tempCode3[3] = {10, 11, 12}; //temp array for code, 3 digits
  uint8_t tempCode2[2] = {10, 11}; //temp array for code, 2 digits

  //generate first number of code
  if (randomValue < 8191) {
    //pCode[0] = 1;
    eeprom_write_byte(&morze[0], 1);
    tempCode3[0] = 2;
    tempCode3[1] = 3;
    tempCode3[2] = 4;
  }
  if ((randomValue > 8191) & (randomValue < 16382)) {
    //pCode[0] = 2;
    eeprom_write_byte(&morze[0], 2);
    tempCode3[0] = 1;
    tempCode3[1] = 3;
    tempCode3[2] = 4;
  }
  if ((randomValue > 16382) & (randomValue < 24573)) {
    //pCode[0] = 3;
    eeprom_write_byte(&morze[0], 3);
    tempCode3[0] = 1;
    tempCode3[1] = 2;
    tempCode3[2] = 4;
  }
  if (randomValue > 24573) {
    //pCode[0] = 4;
    eeprom_write_byte(&morze[0], 4);
    tempCode3[0] = 1;
    tempCode3[1] = 2;
    tempCode3[2] = 3;
  }

  randomValue ^= randomValue; //xor for randomvalue

  if (randomValue < 10922) {
    eeprom_write_byte(&morze[1], tempCode3[1]);
    //pCode[1] = tempCode3[1];
    for (uint8_t i = 1; i < 5; i++) {
      if ((eeprom_read_byte(&morze[0]) != i) & (eeprom_read_byte(&morze[1]) != i)) {
        tempCode2[0] = i;
        for (uint8_t j = 1; j < 5; j++) {
          if ((eeprom_read_byte(&morze[0]) != j) & (eeprom_read_byte(&morze[1]) != j) & (tempCode2[0] != j)) {
            tempCode2[1] = j;
            break;
          }
        }
      }
    }
  }
  if ((randomValue > 10922) & (randomValue < 21844)) {
    //pCode[1] = tempCode3[2];
    eeprom_write_byte(&morze[1], tempCode3[2]);
    for (uint8_t i = 1; i < 5; i++) {
      if ((eeprom_read_byte(&morze[0]) != i) & (eeprom_read_byte(&morze[1]) != i)) {
        tempCode2[0] = i;
        for (uint8_t j = 1; j < 5; j++) {
          if ((eeprom_read_byte(&morze[0]) != j) & (eeprom_read_byte(&morze[1]) != j) & (tempCode2[0] != j)) {
            tempCode2[1] = j;
            break;
          }
        }
      }
    }
  }
  if ((randomValue > 16382) & (randomValue < RAND_MAX)) {
    //pCode[1] = tempCode3[0];
    eeprom_write_byte(&morze[1], tempCode3[0]);
    for (uint8_t i = 1; i < 5; i++) {
      if ((eeprom_read_byte(&morze[0]) != i) & (eeprom_read_byte(&morze[1]) != i)) {
        tempCode2[0] = i;
        for (uint8_t j = 1; j < 5; j++) {
          if ((eeprom_read_byte(&morze[0]) != j) & (eeprom_read_byte(&morze[1]) != j) & (tempCode2[0] != j)) {
            tempCode2[1] = j;
            break;
          }
        }
      }
    }
  }

  eeprom_write_byte(&morze[2], tempCode2[1]);
  eeprom_write_byte(&morze[3], tempCode2[0]);
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

void check_win() {
  if (morze_count == 4) {    
    puts_P(PSTR("\nCongratulations!!! You win the PRIZE, but your princess is in another castle"));
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
            current_number = eeprom_read_byte(morze[morze_count]);
            puts_P(PSTR("#1")); //send to Host
          }
          } else {
          morze_count = 0;
          current_number = eeprom_read_byte(morze[0]);
          true_led_off();
          LED2_ON();
          puts_P(PSTR("?1")); //send to Host, error
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
            current_number = eeprom_read_byte(morze[morze_count]);
            puts_P(PSTR("#2")); //send to Host
          }
          } else {
          morze_count = 0;
          current_number = eeprom_read_byte(morze[0]);
          true_led_off();
          LED4_ON();
          puts_P(PSTR("?2")); //send to Host, error
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
            current_number = eeprom_read_byte(morze[morze_count]);
            puts_P(PSTR("#3")); //send to Host
          }
          } else {
          morze_count = 0;
          current_number = eeprom_read_byte(morze[0]);
          true_led_off();
          LED6_ON();
          puts_P(PSTR("?3")); //send to Host, error
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
            current_number = eeprom_read_byte(morze[morze_count]);
            puts_P(PSTR("#4")); //send to Host
          }
          } else {
          morze_count = 0;
          current_number = eeprom_read_byte(morze[0]);
          true_led_off();
          LED8_ON();
          puts_P(PSTR("?4")); //send to Host, error
        }
        usbPoll();
        break;
      }
    } 
}

int main()
{	
  //init ports of CPU
  init_cpu();

  //inits buttons via library
  BUT_Init(); 

  //init timer0 for button library
  timer0_init();

  //next actions counts number of mcu starts
  uint8_t tmp = eeprom_read_byte(&Num); //read value from eeprom using address of Num
  tmp++;
  eeprom_write_byte(&Num, tmp); //write value of tmp to Num
  eeprom_write_byte(&seed, tmp);
  tmp = eeprom_read_byte(&seed);

  uint8_t *pCode = morze;

  //eeprom_write_byte(&morze_count, 0);
  generate_full_code(tmp);
  current_number = eeprom_read_byte(pCode[0]); //set first morse number as default
  
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

 /***************************************/
 //TEST BLOCK

 //test POWER ON led
  //LED10_ON();
  eeprom_write_byte(morze[0], 1);
  eeprom_write_byte(morze[1], 2);
  eeprom_write_byte(morze[2], 3);
  eeprom_write_byte(morze[3], 4);

 /**************************************/
	

	while (1) // main loop, do forever
	{

    check_win();
    
    usbPoll();

		if (blink_count > 2) // activated by blinking lights
		{
			
			// PLACE TEXT HERE
			//puts_P(PSTR(" ")); // test size
			puts_P(PSTR("RMCSoft LLC\nCharlotte, NC\n933 Louise Ave., Suite 101S, Charlotte, NC 28204, USA\n(980) 201-2460\n\nNick Gritsenko\nnick@rmcsoft.com\n\nOlga Muller\nolga.z.muller@gmail.com"));

			blink_count = 0; // reset
		}
    
    check_button();        
    
    do_morse_signal(pCode);
    		
		// perform usb related background tasks
		usbPoll(); // this needs to be called at least once every 10 ms
		// this is also called in send_report_once
	}
	
	return 0;
}