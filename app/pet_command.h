#ifndef __PET_COMMAND_H
#define __PET_COMMAND_H

#include "stm32f4xx.h"

void Cmd_Init(void);
void Cmd_Process(void);          /* Call from main loop to check for serial input */
uint8_t Cmd_IsCooldown(void);    /* Returns 1 if any command is on cooldown */
uint16_t Cmd_GetCooldownSec(void);

#endif
