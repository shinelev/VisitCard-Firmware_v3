/******************************************************************************************
 * Macros
 *
 * Created: 18.07.2018 13:54:24
 * Author : Dmitry Shinelev
 */ 

/******************************************************************************************
 * Macroses for leds
 */
#define LED1 PORTC0 
#define LED2 PORTD1
#define LED3 PORTC1
#define LED4 PORTD3
#define LED5 PORTD4
#define LED6 PORTD5
#define LED7 PORTD6
#define LED8 PORTD7
#define LED9 PORTB0
#define LED10 PORTB1
#define LED11 PORTB2
#define LED1_ON() PORTC |= (1 << LED1)
#define LED1_OFF() PORTC &= ~(1 << LED1)
#define LED2_ON() PORTD |= (1 << LED2)
#define LED2_OFF() PORTD &= ~(1 << LED2)
#define LED3_ON() PORTC |= (1 << LED3)
#define LED3_OFF() PORTC &= ~(1 << LED3)
#define LED4_ON() PORTD |= (1 << LED4)
#define LED4_OFF() PORTD &= ~(1 << LED4)
#define LED5_ON() PORTD |= (1 << LED5)
#define LED5_OFF() PORTD &= ~(1 << LED5)
#define LED6_ON() PORTD |= (1 << LED6)
#define LED6_OFF() PORTD &= ~(1 << LED6)
#define LED7_ON() PORTD |= (1 << LED7)
#define LED7_OFF() PORTD &= ~(1 << LED7)
#define LED8_ON() PORTD |= (1 << LED8)
#define LED8_OFF() PORTD &= ~(1 << LED8)
#define LED9_ON() PORTB |= (1 << LED9)
#define LED9_OFF() PORTB &= ~(1 << LED9)
#define LED10_ON() PORTB |= (1 << LED10)
#define LED10_OFF() PORTB &= ~(1 << LED10)
#define LED11_ON() PORTB |= (1 << LED11)
#define LED11_OFF() PORTB &= ~(1 << LED11)

/******************************************************************************************
* Macroses for buttons
*/
//#define Button1 PINC2
//#define Button2 PINC3
//#define Button3 PINC4
//#define Button4 PINC5