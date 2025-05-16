// *************Interpreter.c**************
// Students implement this as part of EE445M/EE380L.12 Lab 1,2,3,4 
// High-level OS user interface
// 
// Runs on LM4F120/TM4C123
// Jonathan W. Valvano 1/18/20, valvano@mail.utexas.edu
#include <stdint.h>
#include <string.h> 
#include <stdio.h>
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../inc/ADCT0ATrigger.h"
#include "../inc/ADCSWTrigger.h"
#include "../RTOS_Labs_common/UART0int.h"
//#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../inc/LPF.h"
#include "../RTOS_Lab6_Networking/loader.h"
#include "../RTOS_Labs_common/ADC.h"
#include "../RTOS_Labs_common/esp8266.h"
#include "../RTOS_Labs_common/can0.h"
#include "../inc/IRDistance.h"

#define CMD_MAX_LENGTH 		 	80
#define CMD_MAX_ARG_LENGTH 	20
#define	CMD_MAX_ARGS   		 	5

//Jitter Stuff
#define FS 400              // producer/consumer sampling
#define RUNLENGTH (20*FS)   // display results and quit when NumSamples==RUNLENGTH
#define PERIOD TIME_500US   // DAS 2kHz sampling period in system time units
int32_t MaxJitter;

//Lab Util
// extern uint32_t CPUUtil;
extern uint32_t IdleCount;

// GLOBALS
char command[CMD_MAX_LENGTH];
char command_args[CMD_MAX_ARGS][CMD_MAX_ARG_LENGTH];
int stop;
uint8_t ThreadCount;
char read[160];
static const ELFSymbol_t symtab[] = {
 { "ST7735_Message", ST7735_Message }, // address of ST7735_Message
 { "UART_OutString", UART_OutString }
}; 

ELFEnv_t env = {symtab, 1};

// 
int32_t strToNum(char *num){
	int32_t sum;
	int neg = 0;
	if(*num == '-'){
		neg = 1;
		num++;
	}
	while(*num){
		sum = sum*10 + (*num - 48);
		num++;
	}
	return 0;
}

// Execute commands
void executeCommands(void){
	// ADD COMMANDS HERE
	if(strcmp(command_args[0], "lcd_top") == 0){
		ST7735_MessageNoVal(0, (*command_args[1] - 48), command_args[2]);
	}
	else if(strcmp(command_args[0], "lcd_bottom") == 0){
		ST7735_MessageNoVal(1, (*command_args[1] - 48), command_args[2]);
	}
	else if(strcmp(command_args[0], "ir_value") == 0){
		ST7735_Message((*command_args[1] - 48), ((*command_args[2] - 48)), "adc_in: ", IRDistance_Convert(ADC_In(), 0));
	}
	else if(strcmp(command_args[0], "os_time") == 0){
		ST7735_Message((*command_args[1] - 48), ((*command_args[2] - 48)), "os_time: ", OS_MsTime());
	}
	else if(strcmp(command_args[0], "os_clear") == 0){
		OS_ClearMsTime();
	}
	else if(strcmp(command_args[0], "jitter") == 0){
		ST7735_Message((*command_args[1] - 48), (*command_args[2] - 48), "jitter: ", MaxJitter);
	}
	else if(strcmp(command_args[0], "thread_count") == 0){
		ST7735_Message((*command_args[1] - 48), (*command_args[2] - 48), "thread_count: ", ThreadCount);
	}
//	else if(strcmp(command_args[0], "cpu_util") == 0){
//		ST7735_Message((*command_args[1] - 48), (*command_args[2] - 48), "cpu_util(%): ", CPUUtil);
//	}
	else if(strcmp(command_args[0], "idle_count") == 0){
		ST7735_Message((*command_args[1] - 48), (*command_args[2] - 48), "idle_count(ms): ", IdleCount*10);
	}
	//init SD
	else if(strcmp(command_args[0], "disk_init") == 0){
		eFile_Init();
		UART_OutString("Disk initialized!\n\r");
	}
	//format SD
	else if(strcmp(command_args[0], "disk_format") == 0){
		eFile_Format();
		UART_OutString("Disk formatted!\n\r");
	}
	//mount filesystem
	else if(strcmp(command_args[0], "disk_mount") == 0){
		eFile_Mount();
		UART_OutString("Disk mounted!\n\r");
	}
	//create file
	else if(strcmp(command_args[0], "file_create") == 0){
		if(eFile_Create(command_args[1])){
		UART_OutString("Out of space!\n\r");
		}
		else{
			UART_OutString("File created!\n\r");
		}
	}
	//write to file
	else if(strcmp(command_args[0], "file_write") == 0){
		eFile_WOpen(command_args[1]);
		int i = 0;
		while(command_args[2][i] != '\0'){
			eFile_Write(command_args[2][i]);
			i++;
		}
		eFile_WClose();
		UART_OutString("Wrote to file!\n\r");
	}
	//list directory
	else if(strcmp(command_args[0], "ls") == 0){
		eFile_DOpen("");
//		unsigned long size;
//		char* name;
		unsigned long size;
		char* name;
		int i = 0;
		while(!eFile_DirNext(&name, &size)){
			UART_OutString(name);
			UART_OutString(": ");
			UART_OutUDec(size);
			UART_OutString("\n\r");
			i++;
		}
		eFile_DClose();
	}
	//read file
	else if(strcmp(command_args[0], "file_read") == 0){
		eFile_ROpen(command_args[1]);
		char read;
		while(!eFile_ReadNext(&read)){
			UART_OutChar(read);
		}
		UART_OutString("\n\r");
		eFile_RClose();
	}
	//delete file
	else if(strcmp(command_args[0], "file_delete") == 0){
		eFile_Delete(command_args[1]);
		UART_OutString("File deleted!\n\r");
	}
	//unmount filesystem
	else if(strcmp(command_args[0], "disk_unmount") == 0){
		if(eFile_Unmount()){
			UART_OutString("Disk unmount failed!\n\r");
		}
		else{
			UART_OutString("Disk unmounted!\n\r");
		}
	}
	//write to file
	else if(strcmp(command_args[0], "large_file_write") == 0){
		eFile_WOpen(command_args[1]);
		int i = 0;
		for(i=0;i<1500;i++){
			eFile_Write('a'+i%26);
			if(i%52==51){
				eFile_Write('\n');
				eFile_Write('\r');
			}
		}
		eFile_WClose();
	}
	else if(strcmp(command_args[0], "process_run") == 0){
		exec_elf(command_args[1], &env); 
	}
}

// Print jitter histogram
void Jitter(int32_t MaxJitter, uint32_t const JitterSize, uint32_t JitterHistogram[]){
  // write this for Lab 3 (the latest)
	uint32_t input;  
  unsigned static long LastTime;  // time at previous ADC sample
  uint32_t thisTime;              // time at current ADC sample
  long jitter;                    // time between measured and expected, in us
    //PD0 ^= 0x01;

	uint32_t diff = OS_TimeDifference(LastTime,thisTime);
	if(diff>PERIOD){
		jitter = (diff-PERIOD+4)/8;  // in 0.1 usec
	}else{
		jitter = (PERIOD-diff+4)/8;  // in 0.1 usec
	}
	if(jitter > MaxJitter){
		MaxJitter = jitter; // in usec
	}       // jitter should be 0
	if(jitter >= JitterSize){
		jitter = JitterSize-1;
	}
	JitterHistogram[jitter]++; 
	//PD0 ^= 0x01;
	
//	UART_OutString("jitter: ");
//	UART_OutUDec(MaxJitter);
	ST7735_Message(0, 0, "jitter: ", MaxJitter);
}

// *********** Command line interpreter (shell) ************
void Interpreter(void){ 
  // write this 	
	OS_setID(100);
	while(1){
		int args = 0;
		int len = 0;
		char letter = UART_InChar();
		while(letter != 0){ // Checks that letter isn't nothing
			if(letter == ' '){ // Moves onto next word
				command_args[args][len] = '\0';
				args++;
				len = -1;
			}
			else if (letter == '\n' || letter == '\r'){ // String is done
				command_args[args][len] = '\0';
				executeCommands();
				args = 0;
				len = -1;
			}
			else{
				command_args[args][len] = letter;
			}
			len++;
			letter = UART_InChar();
		}
	}
}


extern const char formBody[];
extern const char statusBody[];
// *********** Command line interpreter for ESP ************
extern Sema4Type WebServerSema;

extern int HTTP_ServePage(const char* body);


void ESP_Interpreter(void){ 
  // write this 	
	OS_setID(100);
	while(1){
		int args = 0;
		int len = 0;

	// Receive request
  if(!ESP8266_Receive(command, 64)){
    ST7735_DrawString(0,3,"No request",ST7735_YELLOW); 
    ESP8266_CloseTCPConnection();
    OS_Signal(&WebServerSema);
    OS_Kill();
  }
	 if(strncmp(command, "GET", 3) == 0) {
    char* messagePtr = strstr(command, "?message=");
    if(messagePtr) {
      // Clear any previous message
      ST7735_DrawString(0,8,"                    ",ST7735_YELLOW); 
      // Process form reply
      if(HTTP_ServePage(statusBody)) {
        ST7735_DrawString(0,3,"Served status",ST7735_YELLOW); 
        
      } else {
        ST7735_DrawString(0,3,"Error serving status",ST7735_YELLOW); 
      }
			
			char* messageEnd = strchr(messagePtr, ' ');
      if(messageEnd) *messageEnd = 0;  // terminate with null character
      // Print message on terminal
			
		  char* letter = strchr(messagePtr, '=')+1; 	
		  int i = 0;
		  while(letter[i] != 0){ // Checks that letter isn't nothing
				if(letter[i] == '+'){ // Moves onto next word
					command_args[args][len] = '\0';
					args++;
					len = -1;
				}
//				else if (letter[i] == '\n' || letter[i] == '\r' || letter[i] == '%'){ // String is done
//					command_args[args][len] = '\0';S
//					executeCommands();
//					args = 0;
//					len = -1;
//				}
				else{
					command_args[args][len] = letter[i];
				}
				len++;
				i++;
	    }
			command_args[args][len] = '\0';
			executeCommands();
      ST7735_DrawString(0,8,messagePtr + 9,ST7735_YELLOW);
    } 
		else {
      // Serve web page
      if(HTTP_ServePage(formBody)) {
        ST7735_DrawString(0,3,"Served form",ST7735_YELLOW); 
      } 
			else {
        ST7735_DrawString(0,3,"Error serving form",ST7735_YELLOW); 
      }
    }        
  } 
	else {
    // handle data that may be sent via means other than HTTP GET
    ST7735_DrawString(0,3,"Not a GET request",ST7735_YELLOW); 
  }
	ESP8266_CloseTCPConnection();
  OS_Signal(&WebServerSema);
	OS_Kill();

	}
}