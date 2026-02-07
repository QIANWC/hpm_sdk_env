/*
 * Copyright (c) 2024 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include "board.h"
#include "ecat_def.h"
#include "ecatappl.h"
#include "ecatslv.h"
#include "applInterface.h"
#include "digital_io.h"
#include "hpm_ecat_hw.h"
#include "hpm_l1c_drv.h"
#include "monitor.h"
#include "hpm_spi_drv.h"
//#include "hpm_can_drv.h"
#include "application/ads1220.h"

/*
 * Copyright (c) 2021 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "board.h"
#include "hpm_debug_console.h"
#include "hpm_spi_drv.h"
#ifdef HPMSOC_HAS_HPMSDK_DMAV2
#include "hpm_dmav2_drv.h"
#else
#include "hpm_dma_drv.h"
#endif
#include "hpm_dmamux_drv.h"
#include "hpm_l1c_drv.h"

ATTR_PLACE_AT_NONCACHEABLE ADS1220_t RSensor;
uint8_t ads1220_regs[4];

int main(void)
{
    hpm_stat_t stat;
    board_init();
    board_init_ethercat(HPM_ESC); /* init ESC function pins */
    board_init_switch_led();      /* init switch and led for ECAT display */
//    board_init_spi_pins(HPM_SPI1);
//    board_init_spi_pins_with_gpio_as_cs(HPM_SPI1);
    printf("EtherCAT IO sample\n");

    monitor_init();
    
    /* hardware init */
    stat = ecat_hardware_init(HPM_ESC);
    if (stat != status_success) {
        printf("Init ESC peripheral and related devices(EEPROM/PHY) failed!\n");
        return 0;
    }

    MainInit(); /* SSC Initilize the stack */

#if defined(ESC_EEPROM_EMULATION) && ESC_EEPROM_EMULATION
    pAPPL_EEPROM_Read  = ecat_eeprom_emulation_read;
    pAPPL_EEPROM_Write = ecat_eeprom_emulation_write;
    pAPPL_EEPROM_Reload = ecat_eeprom_emulation_reload;
    pAPPL_EEPROM_Store  = ecat_eeprom_emulation_store;
#endif

    /* Create basic mapping */
    APPL_GenerateMapping(&nPdInputSize, &nPdOutputSize);

    /* Set stack run flag */
    bRunApplication = TRUE;
    /* Execute the stack */

    ads1220_init(&RSensor);
    RSensor.wbuf[1] = 0x30; // 0x30=AIN1-AIN2, 0xE0=AVDD/2-AVDD/2
    RSensor.wbuf[2] = 0x04; //CM=1,TS=0
    RSensor.wbuf[3] = 0xD0; //VREF=AVCC, 50/60Hz rejection
    RSensor.wbuf[4] = 0x00;
    ads1220_write_registers(&RSensor);
    ads1220_read_registers(&RSensor);

    while (bRunApplication == TRUE) {
        MainLoop();
        monitor_handle();

        ads1220_read_data(&RSensor);
        board_delay_ms(100);
    }

    /* hardware deinit */

    return 0;
}

