#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"

void WideTimer1A_Init(void(*task)(void), uint32_t period, uint32_t priority);

void WideTimer1A_Handler(void);

void WideTimer1_Stop(void);