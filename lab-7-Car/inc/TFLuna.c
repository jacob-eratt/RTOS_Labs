// TFLuna.c
// Runs on TM4C123
// Use UART1 to implement bidirectional data transfer to and from SJ-PM-TF-Luna+A01

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
#include "../inc/tm4c123gh6pm.h"
#include "../inc/CortexM.h"
#include "TFLuna.h"
const uint8_t ObtainFirmware[4]={0x5A,0x04,0x01,0x5F};
// expected response is 0x5A,0x07,0x01,a,b,c,SU version c.b.a
const uint8_t System_Reset[4]={0x5A,0x04,0x02,0x60};
// expected response is 0x5A,0x05,0x02,0x00, 0x60 for success
// expected response is 0x5A,0x05,0x02,0x01, 0x61 for failed
#define SU1 ((0x54+0x06+0x3+TFLunaRate)&0xFF)
const uint8_t Frame_Rate[6]={0x5A,0x06,0x03,TFLunaRate,0,SU1};
const uint8_t Trigger[4]={0x5A,0x04,0x04,0x62};
const uint8_t Format_Standard_cm[5]={0x5A,0x05,0x05,0x01,0x66};
const uint8_t Format_Pixhawk[5]={0x5A,0x05,0x05,0x02,0x67};
const uint8_t Format_Standard_mm[5]={0x5A,0x05,0x05,0x06,0x6A};
const uint8_t Output_Enable[5]={0x5A,0x05,0x07,0x00,0x66};
const uint8_t Output_Disable[5]={0x5A,0x05,0x07,0x01,0x67};
const uint8_t SaveSettings[4]={0x5A,0x04,0x11,0x6F};

#define FIFOSIZE   256       // size of the FIFOs (must be power of 2)
#define FIFOSUCCESS 1        // return value on success
#define FIFOFAIL    0        // return value on failure
uint32_t RxPutI;      // should be 0 to SIZE-1
uint32_t RxGetI;      // should be 0 to SIZE-1 
uint32_t RxFifoLost;  // should be 0 
uint8_t RxFIFO[FIFOSIZE];
void RxFifo_Init2(void){
  RxPutI = RxGetI = 0;                      // empty
  RxFifoLost = 0; // occurs on overflow
}
int RxFifo_Put2(uint8_t data){
  if(((RxPutI+1)&(FIFOSIZE-1)) == RxGetI){
    RxFifoLost++;
    return FIFOFAIL; // fail if full  
  }    
  RxFIFO[RxPutI] = data;                    // save in FIFO
  RxPutI = (RxPutI+1)&(FIFOSIZE-1);         // next place to put
  return FIFOSUCCESS;
}
int RxFifo_Get2(uint8_t *datapt){ 
  if(RxPutI == RxGetI) return 0;            // fail if empty
  *datapt = RxFIFO[RxGetI];                 // retrieve data
  RxGetI = (RxGetI+1)&(FIFOSIZE-1);         // next place to get
  return FIFOSUCCESS; 
}
uint32_t RxPutI2;      // should be 0 to SIZE-1
uint32_t RxGetI2;      // should be 0 to SIZE-1 
uint32_t RxFifoLost2;  // should be 0 
uint8_t RxFIFO2[FIFOSIZE];
void RxFifo2_Init(void){
  RxPutI2 = RxGetI2 = 0;                      // empty
  RxFifoLost2 = 0; // occurs on overflow
}
int RxFifo2_Put(uint8_t data){
  if(((RxPutI2+1)&(FIFOSIZE-1)) == RxGetI){
    RxFifoLost2++;
    return FIFOFAIL; // fail if full  
  }    
  RxFIFO2[RxPutI2] = data;                    // save in FIFO
  RxPutI2 = (RxPutI2+1)&(FIFOSIZE-1);         // next place to put
  return FIFOSUCCESS;
}
int RxFifo2_Get(uint8_t *datapt){ 
  if(RxPutI2 == RxGetI2) return 0;            // fail if empty
  *datapt = RxFIFO2[RxGetI2];                 // retrieve data
  RxGetI2 = (RxGetI2+1)&(FIFOSIZE-1);         // next place to get
  return FIFOSUCCESS; 
}                  
//------------TFLuna_InStatus------------
// Returns how much data available for reading
// Input: none
// Output: number of elements in receive FIFO
uint32_t TFLuna_InStatus(void){  
 return ((RxPutI - RxGetI)&(FIFOSIZE-1));  
}
//------------TFLuna2_InStatus------------
// Returns how much data available for reading from second TFLuna
// Input: none
// Output: number of elements in receive FIFO
uint32_t TFLuna2_InStatus(void){  
 return ((RxPutI2 - RxGetI2)&(FIFOSIZE-1));  
}

#define NVIC_EN0_INT6           0x00000040  // Interrupt 6 enable

#define UART_FR_TXFE            0x00000080  // UART Transmit FIFO Empty
#define UART_FR_RXFF            0x00000040  // UART Receive FIFO Full
#define UART_FR_TXFF            0x00000020  // UART Transmit FIFO Full
#define UART_FR_RXFE            0x00000010  // UART Receive FIFO Empty
#define UART_FR_BUSY            0x00000008  // UART Transmit Busy
#define UART_LCRH_WLEN_8        0x00000060  // 8 bit word length
#define UART_LCRH_FEN           0x00000010  // UART Enable FIFOs
#define UART_CTL_UARTEN         0x00000001  // UART Enable
#define UART_IFLS_RX1_8         0x00000000  // RX FIFO >= 1/8 full
#define UART_IFLS_TX1_8         0x00000000  // TX FIFO <= 1/8 full
#define UART_IM_RTIM            0x00000040  // UART Receive Time-Out Interrupt
                                            // Mask
#define UART_IM_TXIM            0x00000020  // UART Transmit Interrupt Mask
#define UART_IM_RXIM            0x00000010  // UART Receive Interrupt Mask
#define UART_RIS_RTRIS          0x00000040  // UART Receive Time-Out Raw
                                            // Interrupt Status
#define UART_RIS_TXRIS          0x00000020  // UART Transmit Raw Interrupt
                                            // Status
#define UART_RIS_RXRIS          0x00000010  // UART Receive Raw Interrupt
                                            // Status
#define UART_ICR_RTIC           0x00000040  // Receive Time-Out Interrupt Clear
#define UART_ICR_TXIC           0x00000020  // Transmit Interrupt Clear
#define UART_ICR_RXIC           0x00000010  // Receive Interrupt Clear


//------------TFLuna_Init------------
// Initialize the UART1 for 115,200 baud rate (assuming 80 MHz clock),
// 8 bit word length, no parity bits, one stop bit, FIFOs enabled
// Input: function 0 for debug, user function for real time
// Output: none

int TFLunaIndex;
// 0 for looking for two 59s
// 2-8 filling the TFLunaDataMessage with 9-byte message
void (*TFLunaFunction)(uint32_t); // data in mm
uint8_t TFLunaLastByte;
uint32_t TFLunaDistance; // in mm
uint8_t TFLunaDataMessage[12]; // 9 byte fixed size message
int BadCheckSum; // errors
// distance parameter units are mm
void TFLuna_Init(void (*function)(uint32_t)){
  SYSCTL_RCGCUART_R |= 0x02;            // activate UART1
  SYSCTL_RCGCGPIO_R |= 0x04;            // activate port C
  TFLunaFunction = function;
  TFLunaIndex = 0; // looking for two 59s
  TFLunaLastByte = 0;
  TFLunaDataMessage[0] = 0x59;
  TFLunaDataMessage[1] = 0x59;
  BadCheckSum = 0;
  RxFifo_Init2();                        // initialize empty FIFO
  UART1_CTL_R &= ~UART_CTL_UARTEN;      // disable UART
  UART1_IBRD_R = 43;                    // IBRD = int(80,000,000 / (16 * 115200)) = int(43.402778)
  UART1_FBRD_R = 26;                    // FBRD = round(0.402778 * 64) = 26
                                        // 8 bit word length (no parity bits, one stop bit, FIFOs)
  UART1_LCRH_R = (UART_LCRH_WLEN_8|UART_LCRH_FEN);
  UART1_IFLS_R &= ~0x3F;                // clear TX and RX interrupt FIFO level fields
                                        // configure interrupt for TX FIFO <= 1/8 full
                                        // configure interrupt for RX FIFO >= 1/8 full
  UART1_IFLS_R += (UART_IFLS_TX1_8|UART_IFLS_RX1_8);
                                        // enable RX FIFO interrupts and RX time-out interrupt
  UART1_IM_R |= (UART_IM_RXIM|UART_IM_RTIM);
  UART1_CTL_R |= 0x301;                 // enable UART
  GPIO_PORTC_AFSEL_R |= 0x30;           // enable alt funct on PC5-4
  GPIO_PORTC_DEN_R |= 0x30;             // enable digital I/O on PC5-4
                                        // configure PC5-4 as UART
  GPIO_PORTC_PCTL_R = (GPIO_PORTC_PCTL_R&0xFF00FFFF)+0x00220000;
  GPIO_PORTC_AMSEL_R &= ~0x30;          // disable analog functionality on PC5-4
                                        // UART1=priority 2
  NVIC_PRI1_R = (NVIC_PRI1_R&0xFF00FFFF)|0x00400000; // bits 21-23
  NVIC_EN0_R = NVIC_EN0_INT6;           // enable interrupt 6 in NVIC
 // EnableInterrupts();
}
// copy from hardware RX FIFO to software RX FIFO
// stop when hardware RX FIFO is empty or software RX FIFO is full
void static copyHardwareToSoftware(void){
  uint8_t letter;
  if(TFLunaFunction==0){ // raw data mode
    while(((UART1_FR_R&UART_FR_RXFE) == 0) && (TFLuna_InStatus() < (FIFOSIZE - 1))){
      letter = UART1_DR_R;
      RxFifo_Put2(letter);
    }
  }else{
    while((UART1_FR_R&UART_FR_RXFE) == 0){
      letter = UART1_DR_R;
      if(TFLunaIndex == 0){
        if((letter == 0x59)&&(TFLunaLastByte == 0x59)){
          TFLunaIndex = 2; // looking for message
        }
        TFLunaLastByte = letter;
      }else{
        TFLunaDataMessage[TFLunaIndex] = letter;
        TFLunaIndex++;
        if(TFLunaIndex == 9){
          TFLunaLastByte = 0;
          TFLunaIndex = 0;
          uint8_t check=0;
          for(int i=0;i<8;i++){
            check += TFLunaDataMessage[i];
          }
          if(check == TFLunaDataMessage[8]){
            TFLunaDistance = TFLunaDataMessage[3]*256+TFLunaDataMessage[2];
            (*TFLunaFunction)(TFLunaDistance);      
          }else{
            BadCheckSum++; // error
          }
        }
      }  
    }
  }
}

// input ASCII character from UART
// spin if RxFifo is empty
uint8_t TFLuna_InChar(void){
  uint8_t letter;
  while(RxFifo_Get2(&letter) == FIFOFAIL){};
  return(letter);
}
//------------TFLuna_OutChar------------
// Output 8-bit to serial port
// Input: letter is an 8-bit ASCII character to be transferred
// Output: none
void TFLuna_OutChar(uint8_t data){
  while((UART1_FR_R&UART_FR_TXFF) != 0);
  UART1_DR_R = data;
}
// at least one of two things has happened:
// hardware RX FIFO goes from 1 to 2 or more items
// UART receiver has timed out
void UART1_Handler(void){
  if(UART1_RIS_R&UART_RIS_RXRIS){       // hardware RX FIFO >= 2 items
    UART1_ICR_R = UART_ICR_RXIC;        // acknowledge RX FIFO
    // copy from hardware RX FIFO to software RX FIFO
    copyHardwareToSoftware();
  }
  if(UART1_RIS_R&UART_RIS_RTRIS){       // receiver timed out
    UART1_ICR_R = UART_ICR_RTIC;        // acknowledge receiver time out
    // copy from hardware RX FIFO to software RX FIFO
    copyHardwareToSoftware();
  }
}

//------------TFLuna_OutString------------
// Output String (NULL termination)
// Input: pointer to a NULL-terminated string to be transferred
// Output: none
void TFLuna_OutString(uint8_t *pt){
  while(*pt){
    TFLuna_OutChar(*pt);
    pt++;
  }
}



//------------TFLuna_SendMessage------------
// Output message, msg[1] is length
// Input: pointer to message to be transferred
// Output: none
void TFLuna_SendMessage(const uint8_t msg[]){
  for(int i=0; i<msg[1]; i++){
    TFLuna_OutChar(msg[i]);
  }
}
void TFLuna_Format_Standard_mm(void){
  TFLuna_SendMessage(Format_Standard_mm);
}
void TFLuna_Format_Standard_cm(void){
  TFLuna_SendMessage(Format_Standard_cm);
}
void TFLuna_Format_Pixhawk(void){
  TFLuna_SendMessage(Format_Pixhawk);
}
void TFLuna_Output_Enable(void){
  TFLuna_SendMessage(Output_Enable);
}
void TFLuna_Output_Disable(void){
  TFLuna_SendMessage(Output_Disable);
}
void TFLuna_Frame_Rate(void){
  TFLuna_SendMessage(Frame_Rate);
}
void TFLuna_SaveSettings(void){
  TFLuna_SendMessage(SaveSettings);
}
void TFLuna_System_Reset(void){
  TFLuna_SendMessage(System_Reset);
}



//------------TFLuna2_Init------------
// Initialize the UART1 for 115,200 baud rate (assuming 80 MHz clock),
// 8 bit word length, no parity bits, one stop bit, FIFOs enabled
// Input: function 0 for debug, user function for real time
// Output: none

int TFLunaIndex2;
// 0 for looking for two 59s
// 2-8 filling the TFLunaDataMessage with 9-byte message
void (*TFLunaFunction2)(uint32_t); // data in mm
uint8_t TFLunaLastByte2;
uint32_t TFLunaDistance2; // in mm
uint8_t TFLunaDataMessage2[12]; // 9 byte fixed size message
int BadCheckSum2; // errors
// distance parameter units are mm
void TFLuna2_Init(void (*function)(uint32_t)){
  SYSCTL_RCGCUART_R |= 0x08;            // activate UART3
  SYSCTL_RCGCGPIO_R |= 0x04;            // activate port C
  TFLunaFunction2 = function;
  TFLunaIndex2 = 0; // looking for two 59s
  TFLunaLastByte2 = 0;
  TFLunaDataMessage2[0] = 0x59;
  TFLunaDataMessage2[1] = 0x59;
  BadCheckSum2 = 0;
  RxFifo2_Init();                        // initialize empty FIFO
  UART3_CTL_R &= ~UART_CTL_UARTEN;      // disable UART
  UART3_IBRD_R = 43;                    // IBRD = int(80,000,000 / (16 * 115200)) = int(43.402778)
  UART3_FBRD_R = 26;                    // FBRD = round(0.402778 * 64) = 26
                                        // 8 bit word length (no parity bits, one stop bit, FIFOs)
  UART3_LCRH_R = (UART_LCRH_WLEN_8|UART_LCRH_FEN);
  UART3_IFLS_R &= ~0x3F;                // clear TX and RX interrupt FIFO level fields
                                        // configure interrupt for TX FIFO <= 1/8 full
                                        // configure interrupt for RX FIFO >= 1/8 full
  UART3_IFLS_R += (UART_IFLS_TX1_8|UART_IFLS_RX1_8);
                                        // enable RX FIFO interrupts and RX time-out interrupt
  UART3_IM_R |= (UART_IM_RXIM|UART_IM_RTIM);
  UART3_CTL_R |= 0x301;                 // enable UART
  GPIO_PORTC_AFSEL_R |= 0xC0;           // enable alt funct on PC7-6
  GPIO_PORTC_DEN_R |= 0xC0;             // enable digital I/O on PC7-6
                                        // configure PC5-4 as UART
  GPIO_PORTC_PCTL_R = (GPIO_PORTC_PCTL_R&0x00FFFFFF)+0x11000000;
  GPIO_PORTC_AMSEL_R &= ~0xC0;          // disable analog functionality on PC7-6
                                        // UART3=priority 2
  NVIC_PRI14_R = (NVIC_PRI14_R&0x00FFFFFF)|0x40000000; // bits 31-29
  NVIC_EN1_R = 1<<(59-32); 
//  EnableInterrupts();
}
// copy from hardware RX FIFO to software RX FIFO
// stop when hardware RX FIFO is empty or software RX FIFO is full
void static copyHardwareToSoftware2(void){
  uint8_t letter;
  if(TFLunaFunction2==0){ // raw data mode
    while(((UART3_FR_R&UART_FR_RXFE) == 0) && (TFLuna2_InStatus() < (FIFOSIZE - 1))){
      letter = UART3_DR_R;
      RxFifo2_Put(letter);
    }
  }else{
    while((UART3_FR_R&UART_FR_RXFE) == 0){
      letter = UART3_DR_R;
      if(TFLunaIndex2 == 0){
        if((letter == 0x59)&&(TFLunaLastByte2 == 0x59)){
          TFLunaIndex2 = 2; // looking for message
        }
        TFLunaLastByte2 = letter;
      }else{
        TFLunaDataMessage2[TFLunaIndex2] = letter;
        TFLunaIndex2++;
        if(TFLunaIndex2 == 9){
          TFLunaLastByte2 = 0;
          TFLunaIndex2 = 0;
          uint8_t check=0;
          for(int i=0;i<8;i++){
            check += TFLunaDataMessage2[i];
          }
          if(check == TFLunaDataMessage2[8]){
            TFLunaDistance2 = TFLunaDataMessage2[3]*256+TFLunaDataMessage2[2];
            (*TFLunaFunction2)(TFLunaDistance2);      
          }else{
            BadCheckSum2++; // error
          }
        }
      }  
    }
  }
}

// input ASCII character from UART
// spin if RxFifo is empty
uint8_t TFLuna2_InChar(void){
  uint8_t letter;
  while(RxFifo2_Get(&letter) == FIFOFAIL){};
  return(letter);
}
//------------TFLuna2_OutChar------------
// Output 8-bit to serial port UART3
// Input: letter is an 8-bit ASCII character to be transferred
// Output: none
void TFLuna2_OutChar(uint8_t data){
  while((UART3_FR_R&UART_FR_TXFF) != 0);
  UART3_DR_R = data;
}
// at least one of two things has happened:
// hardware RX FIFO goes from 1 to 2 or more items
// UART receiver has timed out
void UART3_Handler(void){
  if(UART3_RIS_R&UART_RIS_RXRIS){       // hardware RX FIFO >= 2 items
    UART3_ICR_R = UART_ICR_RXIC;        // acknowledge RX FIFO
    // copy from hardware RX FIFO to software RX FIFO
    copyHardwareToSoftware2();
  }
  if(UART3_RIS_R&UART_RIS_RTRIS){       // receiver timed out
    UART3_ICR_R = UART_ICR_RTIC;        // acknowledge receiver time out
    // copy from hardware RX FIFO to software RX FIFO
    copyHardwareToSoftware2();
  }
}

//------------TFLuna2_OutString------------
// Output String (NULL termination)
// Input: pointer to a NULL-terminated string to be transferred
// Output: none
void TFLuna2_OutString(uint8_t *pt){
  while(*pt){
    TFLuna2_OutChar(*pt);
    pt++;
  }
}



//------------TFLuna2_SendMessage------------
// Output message, msg[1] is length
// Input: pointer to message to be transferred
// Output: none
void TFLuna2_SendMessage(const uint8_t msg[]){
  for(int i=0; i<msg[1]; i++){
    TFLuna2_OutChar(msg[i]);
  }
}
void TFLuna2_Format_Standard_mm(void){
  TFLuna2_SendMessage(Format_Standard_mm);
}
void TFLuna2_Format_Standard_cm(void){
  TFLuna2_SendMessage(Format_Standard_cm);
}
void TFLuna2_Format_Pixhawk(void){
  TFLuna2_SendMessage(Format_Pixhawk);
}
void TFLuna2_Output_Enable(void){
  TFLuna2_SendMessage(Output_Enable);
}
void TFLuna2_Output_Disable(void){
  TFLuna2_SendMessage(Output_Disable);
}
void TFLuna2_Frame_Rate(void){
  TFLuna2_SendMessage(Frame_Rate);
}
void TFLuna2_SaveSettings(void){
  TFLuna2_SendMessage(SaveSettings);
}
void TFLuna2_System_Reset(void){
  TFLuna2_SendMessage(System_Reset);
}

