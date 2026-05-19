#include "mlx90640_i2c.h"

#include <furi.h>
#include <furi_hal.h>
#include <string.h>

#define MLX90640_I2C_TIMEOUT_MS 100U
#define MLX90640_READ_RETRIES   3U

static paramsMLX90640 mlx90640_params;
static bool mlx90640_params_ready = false;
static float mlx90640_emissivity = 0.95f;

static uint16_t mlx90640_bswap16(uint16_t value) {
    return (uint16_t)((value << 8) | (value >> 8));
}

void MLX90640_I2CInit(void) {
}

int MLX90640_I2CGeneralReset(void) {
    const uint8_t reset_cmd = 0x06;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    const bool ok =
        furi_hal_i2c_tx(&furi_hal_i2c_handle_external, 0x00, &reset_cmd, 1, MLX90640_I2C_TIMEOUT_MS);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    return ok ? 0 : -1;
}

int MLX90640_I2CRead(
    uint8_t slaveAddr,
    uint16_t startAddress,
    uint16_t nMemAddressRead,
    uint16_t* data) {
    uint8_t cmd[2] = {
        (uint8_t)(startAddress >> 8),
        (uint8_t)(startAddress & 0xFFU),
    };

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool ok = furi_hal_i2c_trx(
        &furi_hal_i2c_handle_external,
        (uint8_t)(slaveAddr << 1),
        cmd,
        sizeof(cmd),
        (uint8_t*)data,
        nMemAddressRead * sizeof(uint16_t),
        MLX90640_I2C_TIMEOUT_MS);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    if(ok) {
        for(uint16_t i = 0; i < nMemAddressRead; i++) {
            data[i] = mlx90640_bswap16(data[i]);
        }
    }

    return ok ? 0 : -1;
}

int MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data) {
    const uint8_t cmd[4] = {
        (uint8_t)(writeAddress >> 8),
        (uint8_t)(writeAddress & 0xFFU),
        (uint8_t)(data >> 8),
        (uint8_t)(data & 0xFFU),
    };

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    const bool ok = furi_hal_i2c_tx(
        &furi_hal_i2c_handle_external,
        (uint8_t)(slaveAddr << 1),
        cmd,
        sizeof(cmd),
        MLX90640_I2C_TIMEOUT_MS);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    return ok ? 0 : -1;
}

void MLX90640_I2CFreqSet(int freq) {
    UNUSED(freq);
}

bool mlx90640_read_eeprom(uint16_t eeprom[MLX90640_EEPROM_DUMP_NUM]) {
    return MLX90640_DumpEE(MLX90640_I2C_ADDRESS, eeprom) == MLX90640_NO_ERROR;
}

bool mlx90640_extract_params(uint16_t* eeprom, paramsMLX90640* params) {
    if(MLX90640_ExtractParameters(eeprom, params) != MLX90640_NO_ERROR) {
        return false;
    }

    memcpy(&mlx90640_params, params, sizeof(mlx90640_params));
    mlx90640_params_ready = true;
    return true;
}

bool mlx90640_configure(uint8_t refresh_rate) {
    if(MLX90640_SetRefreshRate(MLX90640_I2C_ADDRESS, refresh_rate) != MLX90640_NO_ERROR) {
        return false;
    }

    return MLX90640_SetChessMode(MLX90640_I2C_ADDRESS) == MLX90640_NO_ERROR;
}

void mlx90640_set_emissivity(float emissivity) {
    mlx90640_emissivity = emissivity;
}

bool mlx90640_read_frame(float frame[MLX90640_PIXEL_COUNT]) {
    if(!mlx90640_params_ready) {
        return false;
    }

    uint16_t frame_data[834];
    bool got_frame = false;

    for(uint8_t subpage = 0; subpage < 2; subpage++) {
        int status = MLX90640_FRAME_DATA_ERROR;
        for(uint8_t attempt = 0; attempt < MLX90640_READ_RETRIES; attempt++) {
            status = MLX90640_GetFrameData(MLX90640_I2C_ADDRESS, frame_data);
            if(status >= 0) {
                break;
            }
        }

        if(status < 0) {
            return false;
        }

        const float ta = MLX90640_GetTa(frame_data, &mlx90640_params);
        MLX90640_CalculateTo(frame_data, &mlx90640_params, mlx90640_emissivity, ta - 8.0f, frame);
        got_frame = true;
    }

    MLX90640_BadPixelsCorrection(
        mlx90640_params.brokenPixels, frame, 1, &mlx90640_params);
    MLX90640_BadPixelsCorrection(
        mlx90640_params.outlierPixels, frame, 1, &mlx90640_params);

    return got_frame;
}
