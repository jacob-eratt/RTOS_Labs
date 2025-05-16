// TFLuna.h
// Runs on LM4F120/TM4C123
// Use UART1 to implement bidirectional data transfer to and from SJ-PM-TF-Luna+A01
// Use UART3 to implement bidirectional data transfer to and from second SJ-PM-TF-Luna+A01

// SJ-PM-TF-Luna+A01
// Pin
// 1    Red  5V
// 2    RxD receiving data    U1Tx PC5 is TxD (output of this microcontroller)
// 3    TxD transmitting data U1Rx PC4 is RxD (input to this microcontroller)
// 4    black ground

// SJ-PM-TF-Luna+A01
// Pin
// 1    Red  5V
// 2    RxD receiving data    U3Tx PC7 is TxD (output of this microcontroller)
// 3    TxD transmitting data U3Rx PC6 is RxD (input to this microcontroller)
// 4    black ground

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2020
   Program 5.11 Section 5.6, Program 3.10

 Copyright 2020 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */
#include <stdint.h>
#ifndef __TFLuna_h__
#define __TFLuna_h__ 1
#define TFLunaRate 100

//------------TFLuna_Init------------
// Initialize the UART1 for 115,200 baud rate (assuming 80 MHz clock),
// 8 bit word length, no parity bits, one stop bit, hardware FIFOs enabled
// Input: function=0 for debug with software FIFO, otherwise user function for real time with message processing
// Output: none
void TFLuna_Init(void (*function)(uint32_t));

// these three set the data format
void TFLuna_Format_Standard_mm(void);  // mm
void TFLuna_Format_Standard_cm(void);  // cm
void TFLuna_Format_Pixhawk(void);      // see datasheet (never tested this)
// set the sampling rate to TFLunaRate
void TFLuna_Frame_Rate(void);
// execute TFLuna_SaveSettings  to activate changes to Format and Frame_Rate
void TFLuna_SaveSettings(void);
// execute TFLuna_System_Reset to start measurments
void TFLuna_System_Reset(void);

// I didn't use these because output enabled was default
void TFLuna_Output_Enable(void);
void TFLuna_Output_Disable(void);

// the following functions only needed for low-level debugging
//------------TFLuna_SendMessage------------
// Output message, msg[1] is length
// Input: pointer to message to be transferred
// Output: none
void TFLuna_SendMessage(const uint8_t msg[]);
//------------TFLuna_InChar------------
// Wait for new serial port input
// Input: none
// Output: 8-bit code from TF Luna
uint8_t TFLuna_InChar(void);

//------------TFLuna_InStatus------------
// Returns how much data available for reading
// Input: none
// Output: number of elements in receive FIFO
uint32_t TFLuna_InStatus(void);

//------------TFLuna_OutChar------------
// Output 8-bit to serial port
// Input: letter is an 8-bit ASCII character to be transferred
// Output: none
void TFLuna_OutChar(uint8_t data);

//------------TFLuna_OutString------------
// Output String (NULL termination)
// Input: pointer to a NULL-terminated string to be transferred
// Output: none
void TFLuna_OutString(uint8_t *pt);

//------------TFLuna_FinishOutput------------
// Wait for all transmission to finish
// Input: none
// Output: none
void TFLuna_FinishOutput(void);

void TFLuna2_Init(void (*function)(uint32_t));

// these three set the data format
void TFLuna2_Format_Standard_mm(void);  // mm
void TFLuna2_Format_Standard_cm(void);  // cm
void TFLuna2_Format_Pixhawk(void);      // see datasheet (never tested this)
// set the sampling rate to TFLunaRate
void TFLuna2_Frame_Rate(void);
// execute TFLuna_SaveSettings  to activate changes to Format and Frame_Rate
void TFLuna2_SaveSettings(void);
// execute TFLuna_System_Reset to start measurments
void TFLuna2_System_Reset(void);

// I didn't use these because output enabled was default
void TFLuna2_Output_Enable(void);
void TFLuna2_Output_Disable(void);

// the following functions only needed for low-level debugging
//------------TFLuna2_SendMessage------------
// Output message, msg[1] is length
// Input: pointer to message to be transferred
// Output: none
void TFLuna2_SendMessage(const uint8_t msg[]);
//------------TFLuna2_InChar------------
// Wait for new serial port input
// Input: none
// Output: 8-bit code from TF Luna
uint8_t TFLuna2_InChar(void);

//------------TFLuna2_InStatus------------
// Returns how much data available for reading
// Input: none
// Output: number of elements in receive FIFO
uint32_t TFLuna2_InStatus(void);

//------------TFLuna2_OutChar------------
// Output 8-bit to serial port
// Input: letter is an 8-bit ASCII character to be transferred
// Output: none
void TFLuna2_OutChar(uint8_t data);

//------------TFLuna2_OutString------------
// Output String (NULL termination)
// Input: pointer to a NULL-terminated string to be transferred
// Output: none
void TFLuna2_OutString(uint8_t *pt);

//------------TFLuna2_FinishOutput------------
// Wait for all transmission to finish
// Input: none
// Output: none
void TFLuna2_FinishOutput(void);
#endif
