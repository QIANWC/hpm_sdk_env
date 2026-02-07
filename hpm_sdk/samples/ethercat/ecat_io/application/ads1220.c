#include "board.h"
#include "ads1220.h"
#include "hpm_spi_drv.h"
#include "hpm_debug_console.h"
#include "hpm_dmav2_drv.h"
#include "hpm_dmamux_drv.h"
#include "hpm_l1c_drv.h"

#define TEST_SPI              BOARD_APP_SPI_BASE
#define TEST_SPI_SCLK_FREQ    BOARD_APP_SPI_SCLK_FREQ
#define TEST_SPI_DMA          BOARD_APP_HDMA
#define TEST_SPI_DMAMUX       BOARD_APP_DMAMUX
#define TEST_SPI_RX_DMA_REQ   BOARD_APP_SPI_RX_DMA
#define TEST_SPI_TX_DMA_REQ   BOARD_APP_SPI_TX_DMA
#define TEST_SPI_RX_DMA_CH    0
#define TEST_SPI_TX_DMA_CH    1
#define TEST_SPI_RX_DMAMUX_CH DMA_SOC_CHN_TO_DMAMUX_CHN(TEST_SPI_DMA, TEST_SPI_RX_DMA_CH)
#define TEST_SPI_TX_DMAMUX_CH DMA_SOC_CHN_TO_DMAMUX_CHN(TEST_SPI_DMA, TEST_SPI_TX_DMA_CH)

/* data width definition */
#define TEST_SPI_DATA_LEN_IN_BIT      (8U)
#define TEST_SPI_DATA_LEN_IN_BYTE     (1U)
#define TEST_SPI_DMA_TRANS_DATA_WIDTH DMA_TRANSFER_WIDTH_BYTE

#ifndef PLACE_BUFF_AT_CACHEABLE
#define PLACE_BUFF_AT_CACHEABLE 0
#endif

#define TEST_TRANSFER_DATA_IN_BYTE (5U)
#if PLACE_BUFF_AT_CACHEABLE
ATTR_ALIGN(HPM_L1C_CACHELINE_SIZE) uint8_t sent_buff[TEST_TRANSFER_DATA_IN_BYTE];
ATTR_ALIGN(HPM_L1C_CACHELINE_SIZE) uint8_t receive_buff[TEST_TRANSFER_DATA_IN_BYTE];
#else
#endif

void spi_master_check_transfer_data(ADS1220_t *dev)
{
    uint32_t i = 0U, error_count = 0U;

    /* Wait for the spi master transfer to complete */
    while (spi_is_active(dev->spi)) {
    }
    /* disable spi dma before starting next dma transaction */
    spi_disable_tx_dma(dev->spi);
    spi_disable_rx_dma(dev->spi);

//    printf("The sent data are:");
//    for (i = 0; i < TEST_TRANSFER_DATA_IN_BYTE; i++) {
//        if ((i & 0x0FU) == 0U) {
//            printf("\r\n");
//        }
//        printf("0x%02X ", dev->reg_wbuf[i]);
//    }
//    printf("\n");
//    printf("The received data are:");
//    for (i = 0; i < TEST_TRANSFER_DATA_IN_BYTE; i++) {
//        if ((i & 0x0FU) == 0U) {
//            printf("\n");
//        }
//        printf("0x%02X ", dev->regs[i]);
//        if (dev->reg_wbuf[i] != dev->regs[i]) {
//            error_count++;
//        }
//    }
//    printf("\n");
//    if (error_count == 0) {
//        printf("SPI transfer all data matched!\n");
//    } else {
//        printf("Error occurred in SPI transfer!\n");
//    }
}

hpm_stat_t spi_tx_trigger_dma(DMA_Type *dma_ptr, uint8_t ch_num, SPI_Type *spi_ptr, uint32_t src, uint8_t data_width, uint32_t size)
{
    dma_handshake_config_t config;
    /* For TX: Disable TX DMA request first to ensure SPI starts correctly after DMA configuration */
    spi_disable_tx_dma(TEST_SPI);
    dma_default_handshake_config(dma_ptr, &config);
    config.ch_index = ch_num;
    config.dst = (uint32_t)&spi_ptr->DATA;
    config.dst_fixed = true;
    config.src = src;
    config.src_fixed = false;
    config.data_width = data_width;
    config.size_in_byte = size;

    return dma_setup_handshake(dma_ptr, &config, true);
}

hpm_stat_t spi_rx_trigger_dma(DMA_Type *dma_ptr, uint8_t ch_num, SPI_Type *spi_ptr, uint32_t dst, uint8_t data_width, uint32_t size)
{
    dma_handshake_config_t config;

    dma_default_handshake_config(dma_ptr, &config);
    config.ch_index = ch_num;
    config.dst = dst;
    config.dst_fixed = false;
    config.src = (uint32_t)&spi_ptr->DATA;
    config.src_fixed = true;
    config.data_width = data_width;
    config.size_in_byte = size;

    return dma_setup_handshake(dma_ptr, &config, true);
}

hpm_stat_t ads1220_init(ADS1220_t *dev)
{
    dev->spi = TEST_SPI;
    if (dev == NULL || dev->spi == NULL) {
        return status_invalid_argument;
    }

    __unused hpm_stat_t stat;
    uint32_t spi_clcok;

    spi_clcok = board_init_spi_clock(TEST_SPI);
    board_init_spi_pins(TEST_SPI);
    printf("SPI Master DMA Transfer Example\n");

    /* set SPI sclk frequency for master */
    spi_master_get_default_timing_config(&dev->timing_config);
    dev->timing_config.master_config.clk_src_freq_in_hz = spi_clcok;
    dev->timing_config.master_config.sclk_freq_in_hz = TEST_SPI_SCLK_FREQ;
    if (status_success != spi_master_timing_init(TEST_SPI, &dev->timing_config)) {
        printf("SPI master timing init failed\n");
        while (1) {
        }
    }

    /* set SPI format config for master */
    spi_master_get_default_format_config(&dev->format_config);
    dev->format_config.master_config.addr_len_in_bytes = 1U;
    dev->format_config.common_config.data_len_in_bits = TEST_SPI_DATA_LEN_IN_BIT;
    dev->format_config.common_config.data_merge = false;
    dev->format_config.common_config.mosi_bidir = false;
    dev->format_config.common_config.lsb = false;
    dev->format_config.common_config.mode = spi_master_mode;
    dev->format_config.common_config.cpol = spi_sclk_low_idle;
    dev->format_config.common_config.cpha = spi_sclk_sampling_even_clk_edges;
    spi_format_init(TEST_SPI, &dev->format_config);

    /* set SPI control config for master */
    spi_master_get_default_control_config(&dev->control_config);
    dev->control_config.master_config.cmd_enable = false;
    dev->control_config.master_config.addr_enable = false;
    dev->control_config.master_config.addr_phase_fmt = spi_address_phase_format_single_io_mode;
    dev->control_config.common_config.tx_dma_enable = true;
    dev->control_config.common_config.rx_dma_enable = true;
    dev->control_config.common_config.trans_mode = spi_trans_write_read_together;
    dev->control_config.common_config.data_phase_fmt = spi_single_io_mode;
    dev->control_config.common_config.dummy_cnt = spi_dummy_count_1;

    return status_success;
}

hpm_stat_t ads1220_session(ADS1220_t *dev, uint32_t wcnt, uint32_t rcnt)
{
    hpm_stat_t stat;
    uint32_t spi_tx_trans_count, spi_rx_trans_count;

    spi_tx_trans_count = MIN(sizeof(dev->wbuf), wcnt);
    spi_rx_trans_count = MIN(sizeof(dev->rbuf), rcnt);

#if 1
    /* setup spi tx trigger dma transfer*/
#if PLACE_BUFF_AT_CACHEABLE
    if (l1c_dc_is_enabled()) {
        /* cache writeback for sent buff */
        uint32_t aligned_start = HPM_L1C_CACHELINE_ALIGN_DOWN((uint32_t)dev->wbuf);
        uint32_t aligned_end = HPM_L1C_CACHELINE_ALIGN_UP((uint32_t)dev->wbuf + sizeof(dev->wbuf));
        uint32_t aligned_size = aligned_end - aligned_start;
        l1c_dc_writeback(aligned_start, aligned_size);
    }
#endif
    dmamux_config(TEST_SPI_DMAMUX, TEST_SPI_TX_DMAMUX_CH, TEST_SPI_TX_DMA_REQ, true);
    stat = spi_tx_trigger_dma(TEST_SPI_DMA, TEST_SPI_TX_DMA_CH, TEST_SPI, core_local_mem_to_sys_address(BOARD_RUNNING_CORE, (uint32_t)dev->wbuf),
                              TEST_SPI_DMA_TRANS_DATA_WIDTH, spi_tx_trans_count);
    if (stat != status_success) {
        printf("spi tx trigger dma failed\n");
        while (1) {
        }
    }

    /* setup spi rx trigger dma transfer*/
    dmamux_config(TEST_SPI_DMAMUX, TEST_SPI_RX_DMAMUX_CH, TEST_SPI_RX_DMA_REQ, true);
    stat = spi_rx_trigger_dma(TEST_SPI_DMA, TEST_SPI_RX_DMA_CH, TEST_SPI, core_local_mem_to_sys_address(BOARD_RUNNING_CORE, (uint32_t)dev->rbuf),
                              TEST_SPI_DMA_TRANS_DATA_WIDTH, spi_rx_trans_count);
    if (stat != status_success) {
        printf("spi rx trigger dma failed\n");
        while (1) {
        }
    }
#if PLACE_BUFF_AT_CACHEABLE
    if (l1c_dc_is_enabled()) {
        /* cache invalidate for receive buff */
        uint32_t aligned_start = HPM_L1C_CACHELINE_ALIGN_DOWN((uint32_t)receive_buff);
        uint32_t aligned_end = HPM_L1C_CACHELINE_ALIGN_UP((uint32_t)receive_buff + sizeof(receive_buff));
        uint32_t aligned_size = aligned_end - aligned_start;
        l1c_dc_invalidate(aligned_start, aligned_size);
        //    l1c_dc_invalidate_all();
    }
#endif
        stat = spi_setup_dma_transfer(TEST_SPI, &dev->control_config, NULL, NULL, spi_tx_trans_count, spi_rx_trans_count);
    //    toggle = !toggle;
        if (stat != status_success) {
            printf("spi setup dma transfer failed\n");
            while (1) {
            }
        }
    
        spi_master_check_transfer_data(dev);
#endif

    return 0;
}
hpm_stat_t ads1220_read_registers(ADS1220_t *dev)
{
    //write to
    dev->wbuf[0] = ADS1220_CMD_RREG | 0x3;
    memset(dev->wbuf + 1, 0, ADS1220_MAX_REGS);
    ads1220_session(dev, 1 + ADS1220_MAX_REGS, 1 + ADS1220_MAX_REGS);
    for (int k = 0; k < ADS1220_MAX_REGS; ++k) {
        dev->regs[k] = dev->rbuf[1 + k];
    }
    return 0;
}
hpm_stat_t ads1220_write_registers(ADS1220_t *dev)
{
    dev->wbuf[0] = ADS1220_CMD_WREG | 0x3;
    ads1220_session(dev, 1 + ADS1220_MAX_REGS, 1 + ADS1220_MAX_REGS);
    return 0;
}
hpm_stat_t ads1220_read_data(ADS1220_t *dev)
{
    memset(dev->wbuf, 0, 4);
    //    dev->wbuf[0] = ADS1220_CMD_RDATA;
    //    memset(dev->wbuf + 1, 0, 3);
    ads1220_session(dev, 4, 4);
    dev->adc_result = dev->rbuf[0] << 24 | dev->rbuf[1] << 16 | dev->rbuf[2] << 8;
    return 0;
}