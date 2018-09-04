/*
 * morse.h
 *
 * Created: 01.08.2018 16:31:56
 *  Author: dmitry_shinelev
 */ 


#ifndef MORSE_H_INCLUDED
#define MORSE_H_INCLUDED

#ifndef IO_H
#define IO_H

#include <avr/io.h>
#include <avr/iom8a.h>

#endif

#ifndef EEMEM_H
#define EEMEM_H

#include <avr/eeprom.h>

#endif

#ifndef STDINT_H
#define STDINT_H

#include <stdint.h>

#endif

#ifndef COMMONS_H
#define COMMONS_H

#include "commons.h"

#endif

  /******************************************************************************************
 * Prototypes
 */
   void do_morse_signal(uint8_t  *pCode);

#endif

