// *************Interpreter.c**************
// Students implement this as part of EE445M/EE380L.12 Lab 1,2,3,4 
// High-level OS user interface
// 
// Runs on LM4F120/TM4C123
// Jonathan W. Valvano 1/18/20, valvano@mail.utexas.edu
#include <stdint.h>
#include <stdlib.h>
#include <string.h> 
#include <stdio.h>
#include <ctype.h> 
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../inc/ADCT0ATrigger.h"
#include "../inc/ADCSWTrigger.h"
#include "../RTOS_Labs_common/ADC.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"

	
#define MAX_COMMAND_LINE_LEN 60 //max length of a command line
#define MAX_COMMAND_LEN 10 //max length a command can be 
#define MAX_ARG_LEN 20 //max length of an argument in a command
#define NUM_COMMANDS 5 //total number of commands/operations
#define MAX_DESC_LEN 60
#define NUM_ARGS 4


int writeToLCD(int commandIndex, char* args0, char* arg1, char* arg2, char* arg3);
int printADC(int commandIndex, char* args0, char* arg1, char* arg2, char* arg3);
int getTime(int commandIndex, char* args0, char* arg1, char* arg2, char* arg3);
int help(int commandIndex, char* args0, char* arg1, char* arg2, char* arg3);
int clear(int commandIndex, char* args0, char* arg1, char* arg2, char* arg3);
int next_command(int commandIndex, char* args0, char* arg1, char* arg2, char* arg3);

typedef struct{
	char commandName[MAX_COMMAND_LEN +1];
	int numArgs;
	int (*operation)(int commandIndex, char* args0, char* arg1, char* arg2, char* arg3); //the function pointer may not need all 4 arguments depending	on the function
	char description[MAX_DESC_LEN+1];
}Command;



/**
 * @brief Command table for mapping command names to their corresponding functions.
 *
 * This array contains a list of commands with their associated names, argument counts, and 
 * the function that should be executed when the command is called. It allows easy mapping 
 * from command names to their respective actions in the system.
 *
 * Each entry in the table consists of:
 * 1. A command name (string) that is used to identify the command.
 * 2. The number of arguments that the command expects (integer).
 * 3. A function pointer to the function that should be executed when the command is invoked. Not all the functions need to be used for the function to run properly
 * 
 * The table is indexed by the command name and can be used to look up and execute commands dynamically.
 */
Command CommandTable[NUM_COMMANDS] = {
  {"write", 4, writeToLCD, "Writes to LCD\nParam:\ntop/bott(0,1)\nrow (0-7)\nstring\nend_Val"},
  {"adc_in", 0, printADC, "Displays ADC value in PUTTY"},
  {"time", 0, getTime, "Prints time\nat bottom of screen"},
  {"help", 0, help, "Helps\nYou are in this right now"},
  {"clear", 0, clear, "Clears the screen"},
};

// Print jitter histogram
void Jitter(int32_t MaxJitter, uint32_t const JitterSize, uint32_t JitterHistogram[]){
  // write this for Lab 3 (the latest)
	
}

int isCommand(char *token){
  for (int i = 0; i < NUM_COMMANDS; i++){
    if (strcmp(token, CommandTable[i].commandName) == 0){
      return i; // true
     }
   }
  return -1; // false
  }

// *********** Command line interpreter (shell) ************
/**
 * @brief The interpreter function that processes user commands from the UART interface.
 *
 * This function continuously waits for user input, parses the command entered, and executes the corresponding
 * function from the command table. It also handles input processing, including case normalization and argument extraction.
 * 
 * The function performs the following:
 * 1. Initializes the system components.
 * 2. Reads the user input from UART.
 * 3. Processes the input and identifies the command.
 * 4. Extracts arguments for the command if required.
 * 5. Executes the corresponding command function.
 * 6. Handles invalid commands by notifying the user.
 */
void Interpreter(void){ 
  // write this  

  OS_ClearMsTime();
  ADC_Init(1);
  char commandPrompt[MAX_COMMAND_LINE_LEN] = {0}; //holds the user input command
  char args[4][MAX_ARG_LEN]; //array that will hold the arguments needed for a certain command
	
  UART_OutString("type in a command and hit enter\n ");
  UART_OutChar(CR);
  UART_OutChar(LF);

	
	while(1){
    UART_InString(commandPrompt,MAX_COMMAND_LINE_LEN); //takes in prompt
    //UART_OutString(" OutString="); //prints out prompt. Shows that prompt was recieved
    //UART_OutString(commandPrompt);
    UART_OutChar(CR);
    UART_OutChar(LF);

    for (int i = 0; i < strlen(commandPrompt); i++){ //converts input command to lowercase
      commandPrompt[i] = tolower(commandPrompt[i]);
		}
		
    char* lPtr;
		
	  if (!(lPtr = strtok(commandPrompt, "\t\n ,"))){ //Lptr gets the first word/token in the command prompt
     continue ; 
    }
    else{
      char str[10];
		int commandIndex = isCommand(lPtr); //gets the corersponding index for the inputted command
		if(commandIndex != -1){
				for (int i = 0; i < CommandTable[commandIndex].numArgs; i++){ //reads arguments based on how many are needed for command 
				lPtr = strtok(NULL, "\t\n ,");
				if (lPtr){
					strcpy(args[i], lPtr);
				}
				else{
         break;
				}
			}
			int ret = CommandTable[commandIndex].operation(commandIndex, args[0], args[1], args[2], args[3]); //runs 
		}
		else{
			UART_OutString("invalid command. Type help for a list of commands and their functions"); //for invalid comments
			UART_OutChar(CR);
		  UART_OutChar(LF);
		}

	}
 }
}



/**
 * @brief Writes a word and a number to a specific screen and row of an LCD, and draws a horizontal line.
 *
 * This function takes the screen number, row number, word, and a number as input arguments and displays 
 * the word and number on the specified screen and row of the LCD. Additionally, it draws a horizontal line 
 * across the middle of the screen.
 *
 * @param commandIndex Not used in this function (reserved for future use).
 * @param arg0 String representing the screen number to write to (converted to uint32_t).
 * @param arg1 String representing the row number to write to on the LCD (converted to uint32_t).
 * @param arg2 Pointer to a string containing the word to be displayed on the LCD.
 * @param arg3 String representing the numeric value to display (converted to uint32_t).
 * 
 * @return Always returns 0.
 * 
 * @note The ST7735_Message function is used to write text and numbers to the LCD screen. The 
 *       ST7735_DrawFastHLine function is used to draw a horizontal line across the middle of the screen 
 *       with a width of 128 pixels and color 0xFFFF (white).
 */
int writeToLCD(int commandIndex, char* arg0, char* arg1, char* arg2, char* arg3){
	
	uint32_t screenNum = atoi(arg0); //converts arg0 to uint_32.. arg0 contains which screen to write to  
	uint32_t rowNum = atoi(arg1); //converts arg1 to uint32_t... arg1 contains which row to write to on lcd
	uint32_t value = atoi(arg3); //number to write to lcd
	
	ST7735_Message(screenNum, rowNum, arg2, value);  //writes to screen
	ST7735_DrawFastHLine(0, 78, 128, 0xFFFF); //draws line accross the middle
	return 0;
}

/**
 * @brief Prints the elapsed time of the OS timer.
 *
 * This function retrieves the current time from the OS timer and displays it on the screen at a 
 * specific location (screen 1, row 7). The time is displayed as a 3-digit number.
 *
 * @param commandIndex Not used in this function (reserved for future use).
 * @param arg0 Not used in this function.
 * @param arg1 Not used in this function.
 * @param arg2 Not used in this function.
 * @param arg3 Not used in this function.
 * 
 * @return Always returns 0.
 * 
 * @note The function uses OS_MsTime() to get the current time in milliseconds and the 
 *       ST7735_MessageNum function to display the time on the screen.
 */
int getTime(int commandIndex, char* arg0, char* arg1, char* arg2, char* arg3){
  int time = OS_MsTime(); //gets OS timer
  ST7735_MessageNum(1,7, time, 3); //prints time
  return 0;
}


/*
 * @brief Reads an ADC value and sends it to the UART for display.
 *
 * This function reads an analog value from the ADC and outputs the value to the UART serial port 
 * for display. The ADC value is preceded by the string "ADC Value: ", and a new line is added after 
 * the value is printed.
 *
 * @param commandIndex Not used in this function (reserved for future use).
 * @param arg0 Not used in this function.
 * @param arg1 Not used in this function.
 * @param arg2 Not used in this function.
 * @param arg3 Not used in this function.
 * 
 * @return Always returns 0.
 * 
 * @note The ADC_In function is used to read the ADC value, and the UART_Out functions are used 
 *       to send the result to the UART. The CR and LF characters are used to insert a newline 
 *       after the ADC value is printed.
 */
int printADC(int commandIndex,char* arg0, char* arg1, char* arg2, char* arg3){
  uint32_t ret = ADC_In(); //ret holds ADC value
  UART_OutString("ADC Value: "); //prints ADC value to serial UART
  UART_OutUDec(ret);
  UART_OutChar(CR);
  UART_OutChar(LF); //new line
  return 0;
		
}




/**
 * @brief Displays a list of available commands on the LCD with a brief help message.
 *
 * This function displays a help message on the screen followed by a list of available commands 
 * from the CommandTable. Each command name is printed on a new row of the screen, with a delay 
 * between each display.
 *
 * @param commandIndex Not used in this function (reserved for future use).
 * @param args0 Not used in this function.
 * @param arg1 Not used in this function.
 * @param arg2 Not used in this function.
 * @param arg3 Not used in this function.
 * 
 * @return Always returns 0.
 * 
 * @note The function uses ST7735_MessageDelay to display the help message and each command name. 
 *       The delay is set to 50 milliseconds between each message.
 */
int help(int commandIndex, char* args0, char* arg1, char* arg2, char* arg3){
  clear(commandIndex, args0, arg1, arg2, arg3);
	for(int i = 0; i < NUM_COMMANDS; i++){
    uint32_t current_time = OS_MsTime();	ST7735_MessageDelay(0,0,"COMMANDS:", 50);
		ST7735_MessageDelay(0,5,CommandTable[i].commandName, 50);
		ST7735_MessageDelay(1,1,CommandTable[i].description, 50);
		while(OS_MsTime() - current_time < 5200){
		  if(OS_MsTime()%300 == 0){
			  ST7735_MessageNum(1, 7, 5-(OS_MsTime() - current_time) / 1000 , 0);
			}
		}
		clear(commandIndex, args0, arg1, arg2, arg3);
	}
  return 0;
}


/**
 * @brief Clears the screen and redraws a horizontal line.
 *
 * This function clears the entire LCD screen and then draws a white horizontal line across 
 * the middle of the screen.
 *
 * @param commandIndex Not used in this function (reserved for future use).
 * @param args0 Not used in this function.
 * @param arg1 Not used in this function.
 * @param arg2 Not used in this function.
 * @param arg3 Not used in this function.
 * 
 * @return Always returns 0.
 * 
 * @note The function uses ST7735_FillScreen to clear the screen and ST7735_DrawFastHLine 
 *       to draw the horizontal line at y-coordinate 78 with a width of 128 pixels and a 
 *       white color (0xFFFF).
 */
int clear(int commandIndex, char* args0, char* arg1, char* arg2, char* arg3){
  ST7735_FillScreen(0);
	ST7735_DrawFastHLine(0, 78, 128, 0xFFFF);
	OS_ClearMsTime();
	return 0;
}
