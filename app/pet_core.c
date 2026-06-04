#include "pet_core.h"
#include <string.h>
#include <stdio.h>

PetData g_pet;

/* --- Internal helpers --- */
static void Pet_RecalcMood(void)
{
    uint8_t avg = (g_pet.hunger + g_pet.energy + g_pet.mood + g_pet.hygiene) / 4;

    if (g_pet.is_sick) {
        g_pet.current_mood = MOOD_SICK;
        return;
    }
    if (avg >= 70) {
        g_pet.current_mood = MOOD_HAPPY;
    } else if (avg >= 45) {
        /* Find the worst attribute */
        uint8_t min = g_pet.hunger;
        if (g_pet.energy < min) min = g_pet.energy;
        if (g_pet.mood < min)   min = g_pet.mood;
        if (g_pet.hygiene < min) min = g_pet.hygiene;

        if (min == g_pet.hunger)
            g_pet.current_mood = MOOD_HUNGRY;
        else if (min == g_pet.energy)
            g_pet.current_mood = MOOD_SLEEPY;
        else
            g_pet.current_mood = MOOD_SAD;
    } else {
        g_pet.current_mood = MOOD_ANGRY;
    }
}

static void Pet_RecalcStage(void)
{
    uint32_t exp = g_pet.experience;
    if (exp >= EXP_MATURE_MAX && g_pet.secret_timer >= 300) {
        g_pet.stage = STAGE_SECRET;
    } else if (exp >= EXP_GROWING_MAX) {
        g_pet.stage = STAGE_MATURE;
    } else if (exp >= EXP_BABY_MAX) {
        g_pet.stage = STAGE_GROWING;
    } else {
        g_pet.stage = STAGE_BABY;
    }
}

/* --- Public API --- */
void Pet_Init(void)
{
    memset(&g_pet, 0, sizeof(PetData));
    strcpy(g_pet.name, "Mimi");
    g_pet.hunger = 80;
    g_pet.energy = 80;
    g_pet.mood = 80;
    g_pet.hygiene = 80;
    g_pet.experience = 0;
    g_pet.stage = STAGE_BABY;
    g_pet.current_mood = MOOD_HAPPY;
    g_pet.is_pc_mode = 1;  /* Start in PC monitor mode by default */
}

void Pet_Tick100ms(void)
{
    g_pet.tick_count++;

    /* Age tracking: ~10 ticks per second */
    if (g_pet.tick_count % 10 == 0) {
        g_pet.age_seconds++;
    }

    /* Attribute decay */
    if (g_pet.tick_count % DECAY_HUNGER_TICKS == 0 && g_pet.hunger > 0) {
        g_pet.hunger--;
    }
    if (g_pet.tick_count % DECAY_ENERGY_TICKS == 0 && g_pet.energy > 0) {
        g_pet.energy--;
    }
    if (g_pet.tick_count % DECAY_MOOD_TICKS == 0 && g_pet.mood > 0) {
        g_pet.mood--;
    }
    if (g_pet.tick_count % DECAY_HYGIENE_TICKS == 0 && g_pet.hygiene > 0) {
        g_pet.hygiene--;
    }

    /* Sickness check: any stat at 0 for too long */
    if (g_pet.hunger == 0 || g_pet.energy == 0 || g_pet.mood == 0 || g_pet.hygiene == 0) {
        g_pet.sick_duration++;
        if (g_pet.sick_duration > 300) { /* ~30 seconds */
            g_pet.is_sick = 1;
        }
    } else {
        g_pet.sick_duration = 0;
        if (g_pet.is_sick && g_pet.hunger > 30 && g_pet.energy > 30 && g_pet.mood > 30 && g_pet.hygiene > 30) {
            g_pet.is_sick = 0;
        }
    }

    /* Death: sick for > 5 minutes -> reset */
    if (g_pet.is_sick && g_pet.sick_duration > 3000) {
        Pet_Init();
        return;
    }

    /* Secret evolution timer */
    if (g_pet.hunger > 80 && g_pet.energy > 80 && g_pet.mood > 80 && g_pet.hygiene > 80) {
        g_pet.secret_timer++;
    } else {
        g_pet.secret_timer = 0;
    }

    /* Mood and stage recalc every 50 ticks (5s) */
    if (g_pet.tick_count % 50 == 0) {
        Pet_RecalcMood();
        Pet_RecalcStage();
    }
}

void Pet_Feed(void)
{
    g_pet.hunger += PET_FEED_AMOUNT;
    if (g_pet.hunger > 100) g_pet.hunger = 100;
    g_pet.experience += PET_EXP_PER_FEED;
}

void Pet_Play(void)
{
    g_pet.mood += PET_PLAY_MOOD_AMT;
    if (g_pet.mood > 100) g_pet.mood = 100;
    if (g_pet.energy >= PET_PLAY_ENERGY_COST)
        g_pet.energy -= PET_PLAY_ENERGY_COST;
    else
        g_pet.energy = 0;
    g_pet.experience += PET_EXP_PER_PLAY;
}

void Pet_Clean(void)
{
    g_pet.hygiene += PET_CLEAN_AMOUNT;
    if (g_pet.hygiene > 100) g_pet.hygiene = 100;
    g_pet.experience += PET_EXP_PER_CLEAN;
}

void Pet_Sleep(void)
{
    g_pet.energy += PET_SLEEP_ENERGY_AMT;
    if (g_pet.energy > 100) g_pet.energy = 100;
    g_pet.experience += PET_EXP_PER_SLEEP;
    /* Fast-forward 30 seconds of decay */
    g_pet.age_seconds += 30;
}

void Pet_GetStatusString(char *buf, uint16_t buf_size)
{
    const char *stage_names[] = {"Kitten", "Cat", "King Cat", "Space Cat"};
    const char *mood_names[] = {"Happy", "Sad", "Hungry", "Sleepy", "Angry", "Sick"};

    snprintf(buf, buf_size,
        "\r\n================================\r\n"
        "  Name:    %s\r\n"
        "  Stage:   %s  Lv.%lu\r\n"
        "  Mood:    %s\r\n"
        "  Hunger:  %d/100\r\n"
        "  Energy:  %d/100\r\n"
        "  Mood:    %d/100\r\n"
        "  Hygiene: %d/100\r\n"
        "  Age:     %lus\r\n"
        "  EXP:     %lu\r\n"
        "================================\r\n",
        g_pet.name,
        stage_names[g_pet.stage],
        (unsigned long)(g_pet.experience / 10),
        mood_names[g_pet.current_mood],
        g_pet.hunger,
        g_pet.energy,
        g_pet.mood,
        g_pet.hygiene,
        (unsigned long)g_pet.age_seconds,
        (unsigned long)g_pet.experience);
}
