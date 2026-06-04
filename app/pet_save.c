#include "pet_save.h"
#include "pet_core.h"
#include "bsp_24C02.h"
#include <string.h>

/* Data layout in EEPROM (64 bytes total):
 * Offset  Size  Field
 * 0       4     Magic "PET!"
 * 4       12    Name
 * 16      1     Hunger
 * 17      1     Energy
 * 18      1     Mood (stat)
 * 19      1     Hygiene
 * 20      4     Experience
 * 24      4     Age seconds
 * 28      1     Stage
 * 29      1     Is sick
 * 30      4     Secret timer
 * 34      1     CRC8
 * 35-63   -     Reserved
 */
#define SAVE_MAGIC    0x54455021  /* "!PET" */
#define SAVE_ADDR     0x00

static uint8_t CRC8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0x00;
    uint8_t poly = 0x07;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ poly;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

uint8_t Pet_Save_Init(void)
{
    return AT24C02_Init();
}

uint8_t Pet_Save(void)
{
    uint8_t buf[40];
    uint32_t magic = SAVE_MAGIC;
    uint8_t crc;

    memset(buf, 0, sizeof(buf));

    /* Pack data */
    memcpy(buf + 0, &magic, 4);
    memcpy(buf + 4, g_pet.name, 12);
    buf[16] = g_pet.hunger;
    buf[17] = g_pet.energy;
    buf[18] = g_pet.mood;
    buf[19] = g_pet.hygiene;
    memcpy(buf + 20, &g_pet.experience, 4);
    memcpy(buf + 24, &g_pet.age_seconds, 4);
    buf[28] = (uint8_t)g_pet.stage;
    buf[29] = g_pet.is_sick;
    memcpy(buf + 30, &g_pet.secret_timer, 4);
    crc = CRC8(buf, 34);
    buf[34] = crc;

    /* Write to EEPROM in 8-byte pages */
    for (uint8_t page = 0; page < 5; page++) {
        uint8_t addr = page * 8;
        if (AT24C02_WriteBytes(SAVE_ADDR + addr, buf + addr, 8) == 0) {
            return 0;
        }
    }

    return 1;
}

uint8_t Pet_Load(void)
{
    uint8_t buf[40];
    uint32_t magic;
    uint8_t crc, calc_crc;

    memset(buf, 0, sizeof(buf));

    /* Read all data from EEPROM */
    if (AT24C02_ReadBytes(SAVE_ADDR, buf, 40) == 0) {
        return 0;
    }

    /* Check magic */
    memcpy(&magic, buf, 4);
    if (magic != SAVE_MAGIC) {
        return 0;
    }

    /* Verify CRC */
    crc = buf[34];
    calc_crc = CRC8(buf, 34);
    if (crc != calc_crc) {
        return 0;
    }

    /* Unpack data */
    memcpy(g_pet.name, buf + 4, 12);
    g_pet.name[11] = '\0';
    g_pet.hunger = buf[16];
    g_pet.energy = buf[17];
    g_pet.mood = buf[18];
    g_pet.hygiene = buf[19];
    memcpy(&g_pet.experience, buf + 20, 4);
    memcpy(&g_pet.age_seconds, buf + 24, 4);
    g_pet.stage = (PetStage)buf[28];
    g_pet.is_sick = buf[29];
    memcpy(&g_pet.secret_timer, buf + 30, 4);

    return 1;
}
