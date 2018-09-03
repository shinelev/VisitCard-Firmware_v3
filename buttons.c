#include "buttons.h"

#define MASK_BUTTONS 	(1<<BUTTON1)|(1<<BUTTON2)|(1<<BUTTON3)|(1<<BUTTON4)
#define ClearBit(reg, bit)       (reg) &= (~(1<<(bit)))
#define SetBit(reg, bit)          (reg) |= (1<<(bit))	
#define BitIsClear(reg, bit)    ((reg & (1<<(bit))) == 0)
#define BitIsSet(reg, bit)       ((reg & (1<<(bit))) != 0)

volatile unsigned char pressedKey = 0;
unsigned char comp = 0;

//_______________________________________
void BUT_Init(void)
{
  DDRX_BUTTON &= ~(MASK_BUTTONS); 
  PORT_BUTTON |= MASK_BUTTONS;
}

//_______________________________________
void BUT_Debrief(void)
{
unsigned char key;

  //последовательный опрос выводов мк
  if (BitIsClear(PIN_BUTTON, BUTTON1))     
    key = KEY_1;
  else if (BitIsClear(PIN_BUTTON, BUTTON2))    
    key = KEY_2;
  else if (BitIsClear(PIN_BUTTON, BUTTON3))        
    key = KEY_3;        
  else if (BitIsClear(PIN_BUTTON, BUTTON4))      
    key = KEY_4;
  else {
    key = KEY_NULL;
  }

  //если во временной переменной что-то есть
  if (key) {
  
    //и если кнопка удерживается долго
	//записать ее номер в буфер 
    if (comp == THRESHOLD) {
	  comp = THRESHOLD+10; 
      pressedKey = key;
      return;
    }
	else if (comp < (THRESHOLD+5)) comp++;
	
  } 
  else comp=0;
}

//__________________________
unsigned char BUT_GetKey(void)
{
  unsigned char key = pressedKey;
  pressedKey = KEY_NULL;
  return key;
}


//____________________________
void BUT_SetKey(unsigned char key)
{
    pressedKey = key;
}

