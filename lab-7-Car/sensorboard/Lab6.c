// Lab6.c
// Runs on LM4F120/TM4C123
// Real Time Operating System for Lab 6

// Jonathan W. Valvano 3/29/17, valvano@mail.utexas.edu
// Andreas Gerstlauer 3/1/16, gerstl@ece.utexas.edu
// EE445M/EE380L.6 
// You may use, edit, run or distribute this file 
// You are free to change the syntax/organization of this file

// LED outputs to logic analyzer for use by OS profile 
// PF1 is preemptive thread switch
// PF2 is first periodic background task (if any)
// PF3 is second periodic background task (if any)
// PC4 is PF4 button touch (SW1 task)

// Outputs for task profiling
// PD0 is idle task
// PD1 is button task

// Button inputs
// PF0 is SW2 task
// PF4 is SW1 button input

// Analog inputs
// PE3 Ain0 sampled at 2kHz, sequencer 3, by Interpreter, using software start

#include <stdint.h>
#include <stdbool.h> 
#include <stdio.h> 
#include <string.h>
#include <stdlib.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/CortexM.h"
#include "../inc/LaunchPad.h"
#include "../inc/PLL.h"
#include "../inc/LPF.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../inc/ADCSWTrigger.h"
#include "../RTOS_Labs_common/ADC.h"
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/heap.h"
#include "../RTOS_Labs_common/Interpreter.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/can0.h"
#include "../RTOS_Labs_common/esp8266.h"
#include "../inc/PWMrobot.h"
#include "../inc/IRDistance.h"

#define PF4             (*((volatile uint32_t *)0x40025040))
#define PF3             (*((volatile uint32_t *)0x40025020))
#define PF2             (*((volatile uint32_t *)0x40025010))
#define PF1             (*((volatile uint32_t *)0x40025008))
#define PF0             (*((volatile uint32_t *)0x40025004))

// bunch of shi for motors
#define SERVOMAX 2750
#define SERVOMIN 950
#define SERVOMID (SERVOMAX+SERVOMIN)/2
#define TURNRIGHT 300
#define TURNLEFT 300
#define GOSTRAIGHT 100

#define OFFSET 10000

#define MAXLEFTIRVAL 2800

#define SERVODELTA 100
// SW2 cycles through 12 positions
// mode=0 1875,...,1275
// mode=1 1870,...,2375
uint32_t Steering;     // 625 to 3125
uint32_t SteeringMode; // 0 for increase, 1 for decrease
#define PERIOD10MS 12500  // 800ns units
#define POWERMIN 5
#define POWERMAX (PERIOD10MS-100)
#define POWERDELTA 2000
uint32_t Power;


// CAN IDs are set dynamically at time of CAN0_Open
// Reverse on other microcontroller
#define RCV_ID 4
#define XMT_ID 2

//*********Prototype for PID in PID_stm32.s, STMicroelectronics
short PID_stm32(short Error, short *Coeff);
short IntTerm;     // accumulated error, RPM-sec
short PrevError;   // previous error, RPM
static int32_t integral_sum = 0;

uint32_t NumCreated;   // number of foreground threads created
uint32_t IdleCount;    // CPU idle counter

//---------------------User debugging-----------------------
extern int32_t MaxJitter;             // largest time jitter between interrupts in usec

#define PD0  (*((volatile uint32_t *)0x40007004))
#define PD1  (*((volatile uint32_t *)0x40007008))
#define PD2  (*((volatile uint32_t *)0x40007010))
#define PD3  (*((volatile uint32_t *)0x40007020))

void PortD_Init(void){ 
  SYSCTL_RCGCGPIO_R |= 0x08;       // activate port D
  while((SYSCTL_RCGCGPIO_R&0x08)==0){};      
  GPIO_PORTD_DIR_R |= 0x0F;        // make PD3-0 output heartbeats
  GPIO_PORTD_AFSEL_R &= ~0x0F;     // disable alt funct on PD3-0
  GPIO_PORTD_DEN_R |= 0x0F;        // enable digital I/O on PD3-0
  GPIO_PORTD_PCTL_R = ~0x0000FFFF;
  GPIO_PORTD_AMSEL_R &= ~0x0F;;    // disable analog functionality on PD
}



//------------------Idle Task--------------------------------
// foreground thread, runs when nothing else does
// never blocks, never sleeps, never dies
// inputs:  none
// outputs: none
void Idle(void){
  IdleCount = 0;          
  while(1) {
    IdleCount++;
    PD0 ^= 0x01;
    WaitForInterrupt();
  }
}

//--------------end of Idle Task-----------------------------
Sema4Type WebServerSema;

void WebServerInterpreter(void){
  // Initialize and bring up Wifi adapter  
  if(!ESP8266_Init(true,false)) {  // verbose rx echo on UART for debugging
    ST7735_DrawString(0,1,"No Wifi adapter",ST7735_YELLOW); 
    OS_Kill();
  }
  // Get Wifi adapter version (for debugging)
  ESP8266_GetVersionNumber(); 
  // Connect to access point
  if(!ESP8266_Connect(true)) {  
    ST7735_DrawString(0,1,"No Wifi network",ST7735_YELLOW); 
    OS_Kill();
  }
  ST7735_DrawString(0,1,"Wifi connected",ST7735_GREEN);
  if(!ESP8266_StartServer(80,600)) {  // port 80, 5min timeout
    ST7735_DrawString(0,2,"Server failure",ST7735_YELLOW); 
    OS_Kill();
  }  
  ST7735_DrawString(0,2,"Server started",ST7735_GREEN);
  
  while(1) {
    // Wait for connection
    ESP8266_WaitForConnection();
    
    // Launch thread with higher priority to serve request
    if(OS_AddThread(&ESP_Interpreter,128,1)) NumCreated++;
    
    // The ESP driver only supports one connection, wait for the thread to complete
    OS_Wait(&WebServerSema);
  }
}  



//*******************final user main - bare bones OS, extend with your code**********
int realmain(void){ // realmain
  OS_Init();        // initialize, disable interrupts
  PortD_Init();     // debugging profile
  Heap_Init();      // initialize heap
  MaxJitter = 0;    // in 1us units
	OS_InitSemaphore(&WebServerSema,0);
	
  // hardware init
  ADC_Init(0);  // sequencer 3, channel 0, PE3, sampling in Interpreter
  CAN0_Open(RCV_ID,XMT_ID);    

  // attach background tasks
  OS_AddPeriodicThread(&disk_timerproc,TIME_1MS,0);   // time out routines for disk  

  // create initial foreground threads
  NumCreated = 0;
	NumCreated += OS_AddThread(&WebServerInterpreter, 128, 2);
  NumCreated += OS_AddThread(&Interpreter,128,2); 
  NumCreated += OS_AddThread(&Idle,128,5);  // at lowest priority 
 
  OS_Launch(TIME_2MS); // doesn't return, interrupts enabled in here
  return 0;            // this never executes
}

//+++++++++++++++++++++++++DEBUGGING CODE++++++++++++++++++++++++
// ONCE YOUR RTOS WORKS YOU CAN COMMENT OUT THE REMAINING CODE
// 

//*****************Test project 0*************************
// This is the simplest configuration, 
// Just see if you can import your OS
// no UART interrupts
// no SYSTICK interrupts
// no timer interrupts
// no switch interrupts
// no ADC serial port or LCD output
// no calls to semaphores
uint32_t Count1;   // number of times thread1 loops
uint32_t Count2;   // number of times thread2 loops
uint32_t Count3;   // number of times thread3 loops
void Thread1(void){
  Count1 = 0;          
  for(;;){
    PD0 ^= 0x01;       // heartbeat
    Count1++;
  }
}
void Thread2(void){
  Count2 = 0;          
  for(;;){
    PD1 ^= 0x02;       // heartbeat
    Count2++;
  }
}
void Thread3(void){
  Count3 = 0;          
  for(;;){
    PD2 ^= 0x04;       // heartbeat
    Count3++;
  }
}

int Testmain0(void){  // Testmain0
  OS_Init();          // initialize, disable interrupts
  PortD_Init();       // profile user threads
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&Thread1,128,1); 
  NumCreated += OS_AddThread(&Thread2,128,2); 
  NumCreated += OS_AddThread(&Thread3,128,3); 
  // Count1 Count2 Count3 should be equal or off by one at all times
  OS_Launch(TIME_2MS); // doesn't return, interrupts enabled in here
  return 0;            // this never executes
}

//*****************Test project 1*************************
// CAN test, exchange CAN messages with second instance
uint8_t XmtData[4];
uint8_t RcvData[4];
uint32_t RcvCount=0;
uint8_t sequenceNum=0;  

// periodic background task to send CAN message
void CANSendTask(void){
  XmtData[0] = PF0<<1;  // 0 or 2
  XmtData[1] = PF4>>2;  // 0 or 4
  XmtData[2] = 0;       // unassigned field
  XmtData[3] = sequenceNum;  // sequence count
  CAN0_SendData(XmtData);
  sequenceNum++;
}

// foreground receiver task 
void CANReceiveTask(void){
  while(1){
    CAN0_GetMail(RcvData);
    RcvCount++;
    ST7735_Message(1,0,"RcvCount   = ",RcvCount); 
    ST7735_Message(1,0,"RcvData[0] = ",RcvData[0]); 
    ST7735_Message(1,0,"RcvData[1] = ",RcvData[1]);
  } 
}

void ButtonWork1(void){
  uint32_t myId = OS_Id(); 
  ST7735_Message(0,1,"SequenceNum = ",sequenceNum); 
  OS_Kill();  // done, OS does not return from a Kill
} 

void SW1Push1(void){
  if(OS_MsTime() > 20){ // debounce
    if(OS_AddThread(&ButtonWork1,128,1)){
      NumCreated++;
    }
    OS_ClearMsTime();  // at least 20ms between touches
  }
}

int Testmain1(void){   // Testmain1
  OS_Init();           // initialize, disable interrupts
  PortD_Init();

  // Initialize CAN with given IDs
  CAN0_Open(RCV_ID,XMT_ID);    

  // attach background tasks
  OS_AddPeriodicThread(&CANSendTask,80000000/10,2);   // 10 Hz  
  OS_AddSW1Task(&SW1Push1,2);
  
  // create initial foreground threads
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&Idle,128,3); 
  NumCreated += OS_AddThread(&CANReceiveTask,128,2); 
 
  OS_Launch(10*TIME_1MS); // doesn't return, interrupts enabled in here
  return 0;               // this never executes
}

//*****************Test project 2*************************
// ESP8266 Wifi client test, fetch weather info from internet 

const char Fetch[] = "GET /data/2.5/weather?q=Austin&APPID=1bc54f645c5f1c75e681c102ed4bbca4 HTTP/1.1\r\nHost:api.openweathermap.org\r\n\r\n";
//char Fetch[] = "GET /data/2.5/weather?q=Austin%20Texas&APPID=1234567890abcdef1234567890abcdef HTTP/1.1\r\nHost:api.openweathermap.org\r\n\r\n";
// 1) go to http://openweathermap.org/appid#use 
// 2) Register on the Sign up page
// 3) get an API key (APPID) replace the 1234567890abcdef1234567890abcdef with your APPID

char Response[64];
uint32_t Running;           // true while fetch is running
void FetchWeather(void){ uint32_t len; char *p; char *s; char *e; 
  int32_t data;
  ST7735_DrawString(0,2,"                 ",ST7735_YELLOW);
  ESP8266_GetStatus();  // debugging
  // Fetch weather from server
  if(!ESP8266_MakeTCPConnection("api.openweathermap.org", 80, 0, false)){ // open socket to web server on port 80
    ST7735_DrawString(0,2,"Connection failed",ST7735_YELLOW); 
    Running = 0;
    OS_Kill();
  }    
  // Send request to server
  if(!ESP8266_Send(Fetch)){
    ST7735_DrawString(0,2,"Send failed",ST7735_YELLOW); 
    ESP8266_CloseTCPConnection();
    Running = 0;
    OS_Kill();
  }    
  // Receive response
  if(!ESP8266_Receive(Response, 64)) {
    ST7735_DrawString(0,2,"No response",ST7735_YELLOW); 
    ESP8266_CloseTCPConnection();
    Running = 0;
    OS_Kill();
  }
  if(strncmp(Response, "HTTP", 4) == 0) { 
    // We received a HTTP response
    ST7735_DrawString(0,2,"Weather fetched",ST7735_YELLOW);
    // Parse HTTP headers until empty line
    len = 0;    
    while(strlen(Response)) {
      if(!ESP8266_Receive(Response, 64)) {
        len = 0;
        break;
      }
      // check for body size
      if(strncmp(Response, "Content-Length: ", 16) == 0) { 
        len = atol(Response+16);
      }
    }
    if(len) {
      // Get HTML body and parse for weather info
      p = Heap_Malloc(len+1);
      ESP8266_Receive(p, len+1);
      s = strstr(p, "\"temp\":");  // get temperature
      if(s){
        data = atol(s+7);
        ST7735_Message(1,1,"Temp [K]: ",data);
      }
      s = strstr(p, "\"description\":");  // get description   
      if(s){
        e = strchr(s+15, '"'); // find end of substring
        if(e){  
          *e = 0;  // temporarily terminate with zero
          ST7735_DrawString(0,8,s+15,ST7735_YELLOW);
        }
      }
      Heap_Free(p);
    } else {
      ST7735_DrawString(0,2,"Empty response",ST7735_YELLOW); 
    }
  } else {
    ST7735_DrawString(0,2,"Invalid response",ST7735_YELLOW); 
  }    
  // Close connection and end
  ESP8266_CloseTCPConnection();
  Running = 0;
  OS_Kill();
}
void ConnectWifi(void){
  // Initialize and bring up Wifi adapter  
  if(!ESP8266_Init(true,false)) {  // verbose rx echo on UART for debugging
    ST7735_DrawString(0,1,"No Wifi adapter",ST7735_YELLOW); 
    OS_Kill();
  }
  // Get Wifi adapter version (for debugging)
  ESP8266_GetVersionNumber(); 
  // Connect to access point
  if(!ESP8266_Connect(true)) {  
    ST7735_DrawString(0,1,"No Wifi network",ST7735_YELLOW); 
    OS_Kill();
  }
  ST7735_DrawString(0,1,"Wifi connected",ST7735_GREEN);
  // Launch thread to fetch weather  
  if(OS_AddThread(&FetchWeather,128,1)) NumCreated++;
  // Kill thread (should really loop to check and reconnect if necessary
  OS_Kill(); 
}  

void SW1Push2(void){
  if(!Running){ 
    Running = 1;  // don't start twice
    if(OS_AddThread(&FetchWeather,128,1)){
      NumCreated++;
    }
  }
}

int Testmain2(void){   // Testmain2
  OS_Init();           // initialize, disable interrupts
  PortD_Init();
  Heap_Init(); 
  Running = 1; 
  
  // attach background tasks
  OS_AddSW1Task(&SW1Push2,2);
  
  // create initial foreground threads
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&Idle,128,3); 
  NumCreated += OS_AddThread(&ConnectWifi,128,2); 
 
  OS_Launch(10*TIME_1MS); // doesn't return, interrupts enabled in here
  return 0;               // this never executes
}

//*****************Test project 3*************************
// ESP8266 web server test, serve a web page with a form to submit message

const char formBody[] = 
  "<!DOCTYPE html><html><body><center> \
  <h1>Enter a message to send to your microcontroller:</h1> \
  <form> \
  <input type=\"text\" name=\"message\" value=\"Hello ESP8266!\"> \
  <br><input type=\"submit\" value=\"Go!\"> \
  </form></center></body></html>";
const char statusBody[] = 
  "<!DOCTYPE html><html><body><center> \
  <h1>Message sent successfully!</h1> \
  </body></html>";

int HTTP_ServePage(const char* body){
  char header[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: ";
    
  char contentLength[16];
  sprintf(contentLength, "%d\r\n\r\n", strlen(body));

  if(!ESP8266_SendBuffered(header)) return 0;
  if(!ESP8266_SendBuffered(contentLength)) return 0;
  if(!ESP8266_SendBuffered(body)) return 0;    
  
  if(!ESP8266_SendBufferedStatus()) return 0;
  
  return 1;
}
void ServeHTTPRequest(void){ 
  ST7735_DrawString(0,3,"Connected           ",ST7735_YELLOW); 
  
  // Receive request
  if(!ESP8266_Receive(Response, 64)){
    ST7735_DrawString(0,3,"No request",ST7735_YELLOW); 
    ESP8266_CloseTCPConnection();
    OS_Signal(&WebServerSema);
    OS_Kill();
  }
    
  // check for HTTP GET
  if(strncmp(Response, "GET", 3) == 0) {
    char* messagePtr = strstr(Response, "?message=");
    if(messagePtr) {
      // Clear any previous message
      ST7735_DrawString(0,8,"                    ",ST7735_YELLOW); 
      // Process form reply
      if(HTTP_ServePage(statusBody)) {
        ST7735_DrawString(0,3,"Served status",ST7735_YELLOW); 
        
      } else {
        ST7735_DrawString(0,3,"Error serving status",ST7735_YELLOW); 
      }
      // Terminate message at first separating space
      char* messageEnd = strchr(messagePtr, ' ');
      if(messageEnd) *messageEnd = 0;  // terminate with null character
      // Print message on terminal
      ST7735_DrawString(0,8,messagePtr + 9,ST7735_YELLOW); 
    } else {
      // Serve web page
      if(HTTP_ServePage(formBody)) {
        ST7735_DrawString(0,3,"Served form",ST7735_YELLOW); 
      } else {
        ST7735_DrawString(0,3,"Error serving form",ST7735_YELLOW); 
      }
    }        
  } else {
    // handle data that may be sent via means other than HTTP GET
    ST7735_DrawString(0,3,"Not a GET request",ST7735_YELLOW); 
  }
  ESP8266_CloseTCPConnection();
  OS_Signal(&WebServerSema);
  OS_Kill();
}
void WebServer(void){
  // Initialize and bring up Wifi adapter  
  if(!ESP8266_Init(true,false)) {  // verbose rx echo on UART for debugging
    ST7735_DrawString(0,1,"No Wifi adapter",ST7735_YELLOW); 
    OS_Kill();
  }
  // Get Wifi adapter version (for debugging)
  ESP8266_GetVersionNumber(); 
  // Connect to access point
  if(!ESP8266_Connect(true)) {  
    ST7735_DrawString(0,1,"No Wifi network",ST7735_YELLOW); 
    OS_Kill();
  }
  ST7735_DrawString(0,1,"Wifi connected",ST7735_GREEN);
  if(!ESP8266_StartServer(80,600)) {  // port 80, 5min timeout
    ST7735_DrawString(0,2,"Server failure",ST7735_YELLOW); 
    OS_Kill();
  }  
  ST7735_DrawString(0,2,"Server started",ST7735_GREEN);
  
  while(1) {
    // Wait for connection
    ESP8266_WaitForConnection();
    
    // Launch thread with higher priority to serve request
    if(OS_AddThread(&ServeHTTPRequest,128,1)) NumCreated++;
    
    // The ESP driver only supports one connection, wait for the thread to complete
    OS_Wait(&WebServerSema);
  }
}  

int Testmain3(void){   // Testmain3
  OS_Init();           // initialize, disable interrupts
  PortD_Init();
  
  OS_InitSemaphore(&WebServerSema,0);
  
  // create initial foreground threads
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&Idle,128,3); 
  NumCreated += OS_AddThread(&WebServer,128,2); 
 
  OS_Launch(10*TIME_1MS); // doesn't return, interrupts enabled in here
  return 0;               // this never executes
}

//////////////////////////////// MOTOR MAIN ////////////////////////////////////////

void motorthread(void){
  Steering = SERVOMID;  // 20ms period 1.5ms pulse
  Servo_Init(25000, SERVOMID);   
  Left_Init(12500, POWERMAX, 0);          // left wheel low power, forward, initialize PWM0, 100 Hz
  Right_Init(12500, 12500-(POWERMAX), 1);   // right wheel low power, forward, initialize PWM0, 100 Hz
	int counter = 0;
	while(1){
		Clock_Delay1ms(250);
		Servo_Duty(SERVOMAX);
		Clock_Delay1ms(250);
		Servo_Duty(SERVOMIN);
	}
}

int motormain(void){
  OS_Init();           // initialize, disable interrupts
  PortD_Init();
    
  // create initial foreground threads
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&Idle,128,3); 
  NumCreated += OS_AddThread(&motorthread,128,2); 
 
  OS_Launch(10*TIME_1MS); // doesn't return, interrupts enabled in here
  return 0;               // this never executes
}

/////////////////////// SENSOR MAIN ////////////////////////////

uint8_t XmtData[4];

void setXmt(uint16_t servo, uint8_t left, uint8_t right){
	XmtData[0] = (servo & 0xFF00) >> 8;
	XmtData[1] = servo & 0x00FF;
	XmtData[2] = left;
	XmtData[3] = right;
}

uint32_t FIRdata[4];
void controlupdatethread(void){ // PORT RESPONDING TO ADC1 DOESN'T WORK
	uint32_t IRdata[4];
	
	ADC_Init3210();

	
	//setXmt(SERVOMID, 60, 60);
	//CAN0_SendData(XmtData);
	Clock_Delay1ms(250);
		LPF_Init(IRdata[0],4);  // prime 4 deep averaging LPF
    LPF_Init2(IRdata[1],4);
    LPF_Init3(IRdata[2],4);
    LPF_Init4(IRdata[3],4);
		Clock_Delay1ms(250);
	
	while(1){
		setXmt(SERVOMAX, 0, 0);
		CAN0_SendData(XmtData);
		Clock_Delay1ms(250);
		setXmt(SERVOMID, 0, 0);
		CAN0_SendData(XmtData);
		Clock_Delay1ms(250);
		setXmt(SERVOMIN, 0, 0);
		CAN0_SendData(XmtData);
		Clock_Delay1ms(250);
		/*
    int leftVal = OFFSET - LPF_Calc(IRdata[0]);   // 4 deep averaging LPF
    int broken = OFFSET - LPF_Calc2(IRdata[1]);  
		int midVal = OFFSET - LPF_Calc4(IRdata[2]); 
    int rightVal = OFFSET - LPF_Calc3(IRdata[3]);  
//	
     
		Clock_Delay1ms(5);
		
		ADC_In3210(IRdata); // IR[0] = LEFT IR        IR[2] = MID IR         IR[3] = RIGHT IR
		

		
		int leftDif = midVal - leftVal;
		int rightDif = midVal - rightVal;
		int centerDif = rightVal - leftVal;
		ST7735_MessageNoVal(1,0,"             "); //SERVOMAX GOES LEFT, SERVOMIN GOES RIGHT
//		if(leftDif > GOSTRAIGHT && rightDif > GOSTRAIGHT){ // Goes straight
//			setXmt(SERVOMID, 20, 20);
//			CAN0_SendData(XmtData);
//		}
//		else{ // Stops
//			setXmt(0, 0, 0);
//			CAN0_SendData(XmtData);
//		}
		UART_OutUDec(leftVal);
		UART_OutString(" leftVal \n\r");
		UART_OutUDec(rightVal);
		UART_OutString(" rightVal \n\r");
		UART_OutUDec(midVal);
		UART_OutString(" midVal \n\r");
		UART_OutString("\n\r");
		
		ST7735_Message(0,0,"CenterDif: ", centerDif);
		ST7735_Message(0,1,"CenterDif: ", centerDif);
		ST7735_Message(0,2,"LeftDif: ", leftVal);
		ST7735_Message(1,0,"LeftDif: ", leftDif);
		ST7735_Message(1,1,"RightDif: ", rightDif);
		ST7735_Message(1,2,"CenterDif: ", centerDif);
		
		ST7735_MessageNoVal(1,0,"             "); //SERVOMAX GOES LEFT, SERVOMIN GOES RIGHT
		if(IRDistance_Convert(IRdata[0], 0) < 300){
			setXmt(SERVOMIN, 20, 20);
			CAN0_SendData(XmtData);
			ST7735_MessageNoVal(1,0,"Wall detected by left sensor"); 
		}
		else if(IRDistance_Convert(IRdata[3], 0) < 300){
			setXmt(SERVOMAX, 20, 20);
			CAN0_SendData(XmtData);
			ST7735_MessageNoVal(1,0,"Wall detected by right sensor"); 
		}
		else if(IRDistance_Convert(IRdata[2], 0) < 100){
			setXmt(SERVOMID, 160, 160);
			CAN0_SendData(XmtData);
			ST7735_MessageNoVal(1,0,"Wall detected by middle sensor"); 
		}
		else{
			setXmt(SERVOMID, 60, 60);
			CAN0_SendData(XmtData);
		}
			
		*/
		//Clock_Delay1ms(250);		
		
	}
}
uint32_t IRdata[4];

void initiallize_control(){

	ADC_Init3210();
	
	Clock_Delay1ms(250);
	LPF_Init(IRdata[0],4);  // prime 4 deep averaging LPF
  LPF_Init2(IRdata[1],4);
  LPF_Init3(IRdata[2],4);
  LPF_Init4(IRdata[3],4);
	Clock_Delay1ms(250);
	
}

void PID_sharp(int mid_val);
void PID_new(void);
void ir_calibrate(void);

int error = 0;
int prev_error = 0;
uint16_t ir_left;
uint16_t ir_right;
uint16_t ir_least;
uint16_t ir_mid;
int startflag = 0;

#define IR_MAX 800
#define IR_MIN 50

void sensorloop(){
//	while(1){
		ADC_In3210(IRdata);
		int leftVal = LPF_Calc(IRdata[0]);
		int rightVal = LPF_Calc3(IRdata[3]);
		int midVal = LPF_Calc2(IRdata[2]);
//		ST7735_Message(0,0,"Bef_Con: ", 0);
//		ST7735_Message(1,0,"Left: ", leftVal);
//		ST7735_Message(1,1,"Mid: ", midVal);
//		ST7735_Message(1,2,"Right: ", rightVal);
		
		ir_left = IRDistance_Convert(leftVal,0);
		ir_right = IRDistance_Convert(rightVal, 3);
		ir_mid = IRDistance_Convert(midVal, 2);
//	}
}

void PID(){
	
	setXmt(SERVOMID, 0, 0);
	CAN0_SendData(XmtData);
	ST7735_MessageNoVal(1, 1, "push to start");

	while(!startflag){};
		
	uint32_t starttime = OS_Time();
	ST7735_MessageNoVal(1, 1, "alive");
	
	while(1){
		uint32_t currtime = OS_Time();
		if((currtime - starttime) > 180000){
				setXmt(SERVOMID, 0, 0);
				Clock_Delay1ms(50);
				CAN0_SendData(XmtData);
				Clock_Delay1ms(50);
				ST7735_MessageNoVal(1, 1, "dead");
				OS_Kill();
		}
		ADC_In3210(IRdata); // IR[0] = LEFT IR        IR[2] = MID IR         IR[3] = RIGHT IR	//int leftVal = OFFSET - LPF_Calc(IRdata[0]);
	//int midVal = OFFSET - LPF_Calc4(IRdata[2]); 
  //int rightVal = OFFSET - LPF_Calc3(IRdata[3]);
		
//		
//		ST7735_Message(0,0,"Left: ", ir_left);
//		ST7735_Message(0,1,"Mid: ", ir_mid);
//		ST7735_Message(0,2,"Right: ", ir_right);
		
	
		if(ir_left < ir_right) ir_least = ir_left;
		else ir_least = ir_right;
		
	
	//turn left  (+)
	//turn right (-)
//<<<<<<< HEAD
	error = ir_left-ir_right;
	//ST7735_Message(1,5, "ERROR: ", error);
//=======
	//error = (ir_left-ir_mid) - (ir_right-ir_mid);
//>>>>>>> 9e6e04c713c96c79f45cbd4bfc589412e9ec11bf
	
	//PID_sharp(ir_mid);
	PID_new();
		
//		if(ir_mid < ir_left || ir_mid < ir_right){
//			PID_sharp(ir_mid);
//		}
//		else{
//			PID_broad(ir_mid);
//		}
//		
	} 
}

//SERVOMAX 2375
//SERVOMIN 1275

#define IR_CEILING 730
#define SERVO_OFFSET 950
#define SERVO_MAX_OFFSET 1800

#define MID_MAX 8
#define MID_MIN 1
#define WALLDIST 90
uint16_t reverseservo;
uint16_t prev_servo;

#define LEFT_SERVO 2750
#define RIGHT_SERVO 950
#define MID_SERVO 1750

#define THRESH_LEFT 2550

//#define IR_CEILING 730


void PID_new(void){
	if(ir_mid < WALLDIST){ //want to back up
			int16_t new_servo = 1750;
			setXmt(reverseservo, -90, -90);
			//ST7735_MessageNoVal(1,1,"Reverse");
			CAN0_SendData(XmtData);
			prev_error = error;
			Clock_Delay1ms(200);
		return;
	}
	uint16_t ir_average = (ir_left + ir_right + ir_mid)/3;
	int16_t weighted_error = error * (2 * abs(error)/((ir_left + ir_right)/2));
		
	if(weighted_error < -730) weighted_error = -730;
	else if(weighted_error > 730) weighted_error = 730;
	//if(abs(weighted_error)>730) weighted_error = 730 * ((weighted_error>>15)*-1);
	//ST7735_Message(1, 0, "w_err: ", weighted_error);
	
				
	uint8_t Kp; // Proportional
	if(ir_mid < 800 ) Kp = 18;
	else if(error > 600) Kp = 12;
	else Kp = 4;
	
	uint8_t Ks = 7;
	uint8_t Km = 16;
	
	// Servo 
	uint16_t servo_value = (uint16_t)((((Kp * weighted_error)/10 + IR_CEILING)* SERVO_MAX_OFFSET) / ((2 * IR_CEILING)));
	servo_value = servo_value + SERVO_OFFSET;
	
	//ST7735_Message(1, 3, "Before Servo: ", servo_value);
	if(servo_value > 1950) servo_value = (servo_value * 12)/10; //turns left less than right
	if(servo_value > SERVOMAX) servo_value = SERVOMAX;
	if(servo_value < SERVOMIN) servo_value = SERVOMIN;

	int16_t motor_speed = (Ks * ((ir_mid * 127)/800))/10 ;
	if(motor_speed < 60) motor_speed = 60;
	if(motor_speed > 100) motor_speed = 100;
	
	int16_t motor_value = (Km * (((abs(error))* (127))/((IR_CEILING))))/10;
	if(motor_value > 127) motor_value = 127;
	
	int motor_left;
	int motor_right;
	if(ir_mid > 700){
		if(error <0){
			motor_right = 127;
			motor_left = 127;
		}
		else{
			motor_right = 127;
			motor_left = 127;
		}
	}
	else{
			if(error < 0){
				motor_right = motor_speed - motor_value;
				motor_left = motor_speed + motor_value;
			}
			else{
				motor_left = motor_speed - motor_value;
				motor_right = motor_speed + motor_value;
		}
	}

	
	
	
	//ST7735_Message(1, 4, "After servo: ", servo_value);
	if(motor_left > 127) motor_left = 127;
	if(motor_left < 0) motor_left = -100;
	if(motor_right > 127) motor_right = 127;
	if(motor_right < 0) motor_right = -100;
	
	setXmt(servo_value, motor_left, motor_right);
	CAN0_SendData(XmtData);
	reverseservo = (-(servo_value - 1850)) + 1850;
	prev_error = error;
}

void ir_calibrate(void){
	initiallize_control();
	while(1){
		 // IR[0] = LEFT IR        IR[2] = MID IR         IR[3] = RIGHT IR	//int leftVal = OFFSET - LPF_Calc(IRdata[0]);
		

	
		int leftVal = LPF_Calc(IRdata[0]);
		int rightVal = LPF_Calc3(IRdata[3]);
		int midVal = LPF_Calc2(IRdata[2]);
		
	ir_left = IRDistance_Convert(leftVal,0);
	ir_right = IRDistance_Convert(rightVal, 2);
	ir_mid = IRDistance_Convert(midVal, 3);
		ST7735_Message(0,0,"Left: ", ir_left);
		ST7735_Message(0,1,"Mid: ", ir_mid);
		ST7735_Message(0,2,"Right: ", ir_right);
	}
}

void button(void){
	Clock_Delay1ms(10);
	while((PF0!=0x01)||(PF4!=0x10)){};
	startflag = 1;
}

int sensormain(void){
	OS_Init();           // initialize, disable interrupts
  PortD_Init();
	CAN0_Open(RCV_ID,XMT_ID);    
	initiallize_control();

    
  // create initial foreground threads
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&Idle,128,4); 
  NumCreated += OS_AddThread(&PID,128,2);
	NumCreated += OS_AddPeriodicThread(&sensorloop, TIME_1MS >> 1, 2);
	NumCreated += OS_AddSW1Task(&button, 1);
	//NumCreated += OS_AddThread(&controlupdatethread,128,2); 

 
  OS_Launch(TIME_1MS*10); // doesn't return, interrupts enabled in here
  return 0;               // this never executes
}



//*******************Trampoline for selecting main to execute**********
int main(void) { 			// main
  //realmain();
	//Testmain1();
	sensormain();
}
