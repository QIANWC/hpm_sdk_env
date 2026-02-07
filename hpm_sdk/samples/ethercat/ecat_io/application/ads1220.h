#ifndef ADS1220_H
#define ADS1220_H

#include "hpm_common.h"
#include "hpm_spi_drv.h"

/* ADS1220 command definitions */
#define ADS1220_CMD_START (0x08)
#define ADS1220_CMD_RDATA (0x10)
#define ADS1220_CMD_RREG  (0x20)
#define ADS1220_CMD_WREG  (0x40)
#define ADS1220_MAX_REGS  4

typedef struct {
    SPI_Type *spi;
    spi_timing_config_t timing_config;
    spi_format_config_t format_config;
    spi_control_config_t control_config;
    int32_t adc_result;
    uint8_t regs[ADS1220_MAX_REGS];
    uint8_t wbuf[1 + ADS1220_MAX_REGS];
    uint8_t rbuf[1 + ADS1220_MAX_REGS];
    float tc_coef;//   = 0.039f;              //39mV/degC
    float voltage_vref;
    float voltage_raw;
    float temperature_cold;//    = 25.0f;
    float temperature_real;
} ADS1220_t;

hpm_stat_t ads1220_init(ADS1220_t *dev);

/* Read/Write multiple registers from/to ADS1220 over SPI.
 * - ptr: pointer to SPI register block (already configured)
 * - reg_addr: starting register address (0..3)
 * - num_regs: number of registers to access (1..4)
 * - out/in: data buffer
 */
hpm_stat_t ads1220_read_registers(ADS1220_t *dev);
hpm_stat_t ads1220_write_registers(ADS1220_t *dev);

hpm_stat_t ads1220_read_data(ADS1220_t *dev);

#endif /* ADS1220_H */
