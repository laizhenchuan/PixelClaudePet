#ifndef __PET_CORE_H
#define __PET_CORE_H

#include "stm32f4xx.h"

/* --- Pet evolution stages --- */
typedef enum {
    STAGE_BABY = 0,
    STAGE_GROWING = 1,
    STAGE_MATURE = 2,
    STAGE_SECRET = 3
} PetStage;

/* --- Pet mood types --- */
typedef enum {
    MOOD_HAPPY = 0,
    MOOD_SAD = 1,
    MOOD_HUNGRY = 2,
    MOOD_SLEEPY = 3,
    MOOD_ANGRY = 4,
    MOOD_SICK = 5
} PetMood;

/* --- Pet data structure --- */
typedef struct {
    char name[12];
    uint8_t hunger;       /* 0-100 */
    uint8_t energy;       /* 0-100 */
    uint8_t mood;         /* 0-100 */
    uint8_t hygiene;      /* 0-100 */
    uint32_t experience;
    PetStage stage;
    PetMood  current_mood;
    uint32_t age_seconds;
    uint32_t tick_count;
    uint8_t is_sick;
    uint16_t sick_duration;
    uint8_t  is_pc_mode;    /* 1 = PC monitor mode, 0 = pet game mode */
    uint32_t secret_timer; /* seconds with all stats > 80 */
} PetData;

extern PetData g_pet;

/* --- Public API --- */
void Pet_Init(void);
void Pet_Tick100ms(void);     /* Call every 100ms from main loop */
void Pet_Feed(void);
void Pet_Play(void);
void Pet_Clean(void);
void Pet_Sleep(void);
void Pet_GetStatusString(char *buf, uint16_t buf_size);

/* --- Helper macros --- */
#define PET_EXP_PER_FEED    8
#define PET_EXP_PER_PLAY    6
#define PET_EXP_PER_CLEAN   5
#define PET_EXP_PER_SLEEP   4
#define PET_FEED_AMOUNT     25
#define PET_PLAY_MOOD_AMT   20
#define PET_PLAY_ENERGY_COST 5
#define PET_CLEAN_AMOUNT    25
#define PET_SLEEP_ENERGY_AMT 35

/* Decay per tick (100ms): 1 point per N ticks */
#define DECAY_HUNGER_TICKS   60   /* 1 hunger point per 6 seconds */
#define DECAY_ENERGY_TICKS   90   /* 1 energy point per 9 seconds */
#define DECAY_MOOD_TICKS     75   /* 1 mood point per 7.5 seconds */
#define DECAY_HYGIENE_TICKS  80   /* 1 hygiene point per 8 seconds */

/* Stage thresholds */
#define EXP_BABY_MAX         200
#define EXP_GROWING_MAX      600
#define EXP_MATURE_MAX       1200

#endif
