#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "mlx90640_api.h"

#define MLX90640_I2C_ADDRESS 0x33U
#define MLX90640_PIXEL_COUNT 768U

void MLX90640_I2CInit(void);
int MLX90640_I2CGeneralReset(void);
int MLX90640_I2CRead(
    uint8_t slaveAddr,
    uint16_t startAddress,
    uint16_t nMemAddressRead,
    uint16_t* data);
int MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data);
void MLX90640_I2CFreqSet(int freq);

bool mlx90640_read_eeprom(uint16_t eeprom[MLX90640_EEPROM_DUMP_NUM]);
bool mlx90640_extract_params(uint16_t* eeprom, paramsMLX90640* params);
bool mlx90640_configure(uint8_t refresh_rate);
void mlx90640_set_emissivity(float emissivity);
bool mlx90640_read_frame(float frame[MLX90640_PIXEL_COUNT]);
