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
#define PLACE_BUFF_AT_CACHEABLE 1
#endif

#define TEST_TRANSFER_DATA_IN_BYTE (5U)
#if PLACE_BUFF_AT_CACHEABLE
ATTR_ALIGN(HPM_L1C_CACHELINE_SIZE) uint8_t sent_buff[TEST_TRANSFER_DATA_IN_BYTE];
ATTR_ALIGN(HPM_L1C_CACHELINE_SIZE) uint8_t receive_buff[TEST_TRANSFER_DATA_IN_BYTE];
#else
ATTR_PLACE_AT_NONCACHEABLE uint8_t sent_buff[TEST_TRANSFER_DATA_IN_BYTE];
ATTR_PLACE_AT_NONCACHEABLE uint8_t receive_buff[TEST_TRANSFER_DATA_IN_BYTE];
#endif

void prepare_transfer_data(void)
{
    sent_buff[0] = 0x23;
    //    for (uint32_t i = 0; i < TEST_TRANSFER_DATA_IN_BYTE; i++) {
    //        sent_buff[i] = i % 0xFF;
    //    }
}

void spi_master_check_transfer_data(SPI_Type *ptr)
{
    uint32_t i = 0U, error_count = 0U;

    /* Wait for the spi master transfer to complete */
    while (spi_is_active(ptr)) {
    }
    /* disable spi dma before starting next dma transaction */
    spi_disable_tx_dma(ptr);
    spi_disable_rx_dma(ptr);

    printf("The sent data are:");
    for (i = 0; i < TEST_TRANSFER_DATA_IN_BYTE; i++) {
        if ((i & 0x0FU) == 0U) {
            printf("\r\n");
        }
        printf("0x%02X ", sent_buff[i]);
    }
    printf("\n");
    printf("The received data are:");
    for (i = 0; i < TEST_TRANSFER_DATA_IN_BYTE; i++) {
        if ((i & 0x0FU) == 0U) {
            printf("\n");
        }
        printf("0x%02X ", receive_buff[i]);
        if (sent_buff[i] != receive_buff[i]) {
            error_count++;
        }
    }
    printf("\n");
    if (error_count == 0) {
        printf("SPI transfer all data matched!\n");
    } else {
        printf("Error occurred in SPI transfer!\n");
    }
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

    hpm_stat_t stat;
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

ATTR_PLACE_AT_NONCACHEABLE int toggle;
hpm_stat_t ads1220_test(ADS1220_t *dev)
{
    hpm_stat_t stat;
    uint8_t cmd = 0x1a;
    uint32_t addr = 0x10;
    uint32_t spi_tx_trans_count, spi_rx_trans_count;

    spi_tx_trans_count = sizeof(sent_buff) / TEST_SPI_DATA_LEN_IN_BYTE;
    spi_rx_trans_count = sizeof(receive_buff) / TEST_SPI_DATA_LEN_IN_BYTE;
    prepare_transfer_data();

    if (toggle) {
        sent_buff[0] = 0x23; //rreg
        sent_buff[1] = 0;
        sent_buff[2] = 0;
        toggle = 0;
    } else {
        sent_buff[0] = 0x43; //wreg
        sent_buff[1] = 0x2;
        sent_buff[2] = 0x2;
        toggle = 1;
    }

#if 1
    /* setup spi tx trigger dma transfer*/
#if PLACE_BUFF_AT_CACHEABLE
    if (l1c_dc_is_enabled()) {
        /* cache writeback for sent buff */
        uint32_t aligned_start = HPM_L1C_CACHELINE_ALIGN_DOWN((uint32_t)sent_buff);
        uint32_t aligned_end = HPM_L1C_CACHELINE_ALIGN_UP((uint32_t)sent_buff + sizeof(sent_buff));
        uint32_t aligned_size = aligned_end - aligned_start;
        l1c_dc_writeback(aligned_start, aligned_size);
    }
#endif
    dmamux_config(TEST_SPI_DMAMUX, TEST_SPI_TX_DMAMUX_CH, TEST_SPI_TX_DMA_REQ, true);
    stat = spi_tx_trigger_dma(TEST_SPI_DMA, TEST_SPI_TX_DMA_CH, TEST_SPI, core_local_mem_to_sys_address(BOARD_RUNNING_CORE, (uint32_t)sent_buff),
                              TEST_SPI_DMA_TRANS_DATA_WIDTH, sizeof(sent_buff));
    if (stat != status_success) {
        printf("spi tx trigger dma failed\n");
        while (1) {
        }
    }

    /* setup spi rx trigger dma transfer*/
    dmamux_config(TEST_SPI_DMAMUX, TEST_SPI_RX_DMAMUX_CH, TEST_SPI_RX_DMA_REQ, true);
    stat = spi_rx_trigger_dma(TEST_SPI_DMA, TEST_SPI_RX_DMA_CH, TEST_SPI, core_local_mem_to_sys_address(BOARD_RUNNING_CORE, (uint32_t)receive_buff),
                              TEST_SPI_DMA_TRANS_DATA_WIDTH, sizeof(receive_buff));
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
        stat = spi_setup_dma_transfer(TEST_SPI, &dev->control_config, &cmd, &addr, spi_tx_trans_count, spi_rx_trans_count);
    //    toggle = !toggle;
        if (stat != status_success) {
            printf("spi setup dma transfer failed\n");
            while (1) {
            }
        }
    
        spi_master_check_transfer_data(TEST_SPI);

//    ads1220_write_registers(TEST_SPI, 0, 4, sent_buff);
//    ads1220_read_registers(TEST_SPI, 0, 4, receive_buff);
//    while (1) {
//    }
#endif

    return 0;
}
/* Build a register read command and perform SPI transfer using spi_write_read_data.
 * ADS1220 protocol: RREG (0x20 | addr), count = n-1
 */
hpm_stat_t ads1220_read_registers(SPI_Type *ptr, uint8_t reg_addr, uint8_t num_regs, uint8_t *out)
{
    if (out == NULL || num_regs == 0 || num_regs > ADS1220_MAX_REGS) {
        return status_invalid_argument;
    }

    if (reg_addr >= ADS1220_MAX_REGS) {
        return status_invalid_argument;
    }

    uint8_t cmd;
    cmd = ADS1220_CMD_RREG | ((reg_addr & 0x03) << 2) | ((num_regs - 1) & 0x03);

    /* For ADS1220 we need to send the command and then read num_regs bytes.
     * Use spi_write_read_data which takes write buffer, write count, read buffer, read count.
     * We'll send the 2-byte command (wbuff) and then read num_regs bytes (rbuff).
     * The function expects data_len_in_bytes; ADS1220 is 8-bit registers so data_len_in_bytes = 1.
     */

    uint8_t wtmp[1 + ADS1220_MAX_REGS];
    uint8_t rtmp[1 + ADS1220_MAX_REGS];

    /* copy command to wtmp */
    wtmp[0] = cmd;

    hpm_stat_t stat = spi_write_read_data(ptr, 1, wtmp, 1 + num_regs, rtmp, 1 + num_regs);
    if (stat != status_success) {
        return stat;
    }

    /* copy received data to out */
    for (uint8_t i = 0; i < num_regs; i++) {
        out[i] = rtmp[i];
    }

    return status_success;
}

hpm_stat_t ads1220_write_registers(SPI_Type *ptr, uint8_t reg_addr, uint8_t num_regs, const uint8_t *in)
{
    if (in == NULL || num_regs == 0 || num_regs > ADS1220_MAX_REGS) {
        return status_invalid_argument;
    }

    if (reg_addr >= ADS1220_MAX_REGS) {
        return status_invalid_argument;
    }

    uint8_t cmd;
    cmd = ADS1220_CMD_WREG | ((reg_addr & 0x03) << 2) | ((num_regs - 1) & 0x03);

    /* Build write buffer: cmd[0], cmd[1], reg bytes... */
    uint8_t wbuf[1 + ADS1220_MAX_REGS];
    wbuf[0] = cmd;
    for (uint8_t i = 0; i < num_regs; i++)
        wbuf[1 + i] = in[i];

    /* write only transaction */
    return spi_write_read_data(ptr, 1, wbuf, 1 + num_regs, NULL, 0);
}

hpm_stat_t ads1220_read_register(SPI_Type *ptr, uint8_t reg_addr, uint8_t *out)
{
    return ads1220_read_registers(ptr, reg_addr, 1, out);
}

hpm_stat_t ads1220_write_register(SPI_Type *ptr, uint8_t reg_addr, uint8_t value)
{
    return ads1220_write_registers(ptr, reg_addr, 1, &value);
}
