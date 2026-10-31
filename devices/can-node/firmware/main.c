/* The robot sensor node: every 10 ms (TIM6 at 100 Hz) the four pressure inputs (ADC1 IN1-IN4 on
 * PA0-PA3 through their dividers, converted in one sequence into memory by DMA) and the
 * ICM-42688-P's accelerations and rotation rates (I2C1 at 400 kHz) go out on CAN at 500 kbit/s:
 *   0x100  pressures: four 16-bit values, millivolts at the 0-5 V inputs (little-endian)
 *   0x101  acceleration X, Y, Z in mg, then the IMU's temperature in 0.01 °C (16-bit each)
 *   0x102  rotation X, Y, Z in 0.1 °/s (16-bit each)
 * A frame 0x200 with one byte sets the status LED's mode (0 off, 1 on, 2 blinking). The node's
 * console (USART1, 115200 baud on the debug header) reports what it does.
 * ST's HAL on the STM32G431CBU6 at 170 MHz from the 8 MHz crystal. */
#include <stdio.h>
#include <string.h>
#include <pcbkit/stm32.h>
#include "stm32g4xx_hal.h"

/* The kit's console: USART1, printf. */
const struct pcbkit_board pcbkit_board = {.uart_baud = 115200};

static UART_HandleTypeDef uart;
static ADC_HandleTypeDef adc;
static DMA_HandleTypeDef adc_dma;
static I2C_HandleTypeDef i2c;
static TIM_HandleTypeDef tick;
static FDCAN_HandleTypeDef can;
static volatile uint16_t raw[5];
static volatile int due, led_mode = 2;
static int imu_ok;

#define IMU 0x68
/* Each input's divider (the design's Divider: Rt 11 kohm over Rb 20 kohm, 5 V -> 3.23 V), against
 * VDDA as VREFINT measures it (its factory calibration at 3.0 V): the regulator's +-2 % and its
 * drift do not reach the reported pressures. */
#define INPUT_MV(code, vdda) ((uint32_t)(code) * (vdda) * 31u / (4095u * 20u))

void SysTick_Handler(void) { HAL_IncTick(); }
void TIM6_DAC_IRQHandler(void) { HAL_TIM_IRQHandler(&tick); }
void DMA1_Channel1_IRQHandler(void) { HAL_DMA_IRQHandler(&adc_dma); }
void FDCAN1_IT0_IRQHandler(void) { HAL_FDCAN_IRQHandler(&can); }
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *h) { if (h == &tick) due = 1; }

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *h, uint32_t its) {
    FDCAN_RxHeaderTypeDef header;
    uint8_t data[8];
    if (!(its & FDCAN_IT_RX_FIFO0_NEW_MESSAGE)) return;
    while (HAL_FDCAN_GetRxMessage(h, FDCAN_RX_FIFO0, &header, data) == HAL_OK)
        if (header.Identifier == 0x200 && header.DataLength >= 1) led_mode = data[0];
}

static void fail(const char *what) { printf("%s failed\n", what); for (;;) {} }

static void clock(void) {
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = RCC_PLLM_DIV2;
    osc.PLL.PLLN = 85;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = RCC_PLLQ_DIV2;
    osc.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) for (;;) {}
    RCC_ClkInitTypeDef clk = {0};
    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK) for (;;) {}
    RCC_PeriphCLKInitTypeDef periph = {0};
    periph.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
    periph.FdcanClockSelection = RCC_FDCANCLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK) for (;;) {}
}

static void pins(GPIO_TypeDef *port, uint32_t which, uint32_t mode, uint32_t af) {
    GPIO_InitTypeDef pin = {0};
    pin.Pin = which;
    pin.Mode = mode;
    pin.Pull = GPIO_NOPULL;
    pin.Speed = GPIO_SPEED_FREQ_HIGH;
    pin.Alternate = af;
    HAL_GPIO_Init(port, &pin);
}

static void reg_write(uint8_t reg, uint8_t value) { HAL_I2C_Mem_Write(&i2c, IMU << 1, reg, I2C_MEMADD_SIZE_8BIT, &value, 1, 5); }

static void imu_start(void) {
    uint8_t who = 0;
    imu_ok = HAL_I2C_Mem_Read(&i2c, IMU << 1, 0x75, I2C_MEMADD_SIZE_8BIT, &who, 1, 5) == HAL_OK && who == 0x47;
    if (!imu_ok) { printf("imu missing (WHO_AM_I %02x)\n", who); return; }
    reg_write(0x4e, 0x0f);         /* PWR_MGMT0: accelerometer and gyroscope in low-noise mode */
    reg_write(0x50, 0x46);         /* ACCEL_CONFIG0: +-4 g, 1 kHz */
    reg_write(0x4f, 0x26);         /* GYRO_CONFIG0: +-1000 dps, 1 kHz */
    HAL_Delay(1);
    printf("imu ICM-42688-P\n");
}

static void send(uint32_t id, const int16_t *v, int n) {
    FDCAN_TxHeaderTypeDef h = {0};
    h.Identifier = id;
    h.IdType = FDCAN_STANDARD_ID;
    h.TxFrameType = FDCAN_DATA_FRAME;
    h.DataLength = (uint32_t)(2 * n);
    h.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    h.BitRateSwitch = FDCAN_BRS_OFF;
    h.FDFormat = FDCAN_CLASSIC_CAN;
    h.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    uint8_t data[8];
    memcpy(data, v, (size_t)(2 * n));
    if (HAL_FDCAN_GetTxFifoFreeLevel(&can) > 0) HAL_FDCAN_AddMessageToTxFifoQ(&can, &h, data);
}

int main(void) {
    HAL_Init();
    clock();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_DMAMUX1_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_ADC12_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_TIM6_CLK_ENABLE();
    __HAL_RCC_FDCAN_CLK_ENABLE();

    pins(GPIOA, GPIO_PIN_9 | GPIO_PIN_10, GPIO_MODE_AF_PP, GPIO_AF7_USART1);
    uart.Instance = USART1;
    uart.Init.BaudRate = 115200;
    uart.Init.WordLength = UART_WORDLENGTH_8B;
    uart.Init.StopBits = UART_STOPBITS_1;
    uart.Init.Parity = UART_PARITY_NONE;
    uart.Init.Mode = UART_MODE_TX_RX;
    uart.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&uart);   /* the console again, at the new clock */
    printf("can-node %lu Hz\n", (unsigned long)HAL_RCC_GetSysClockFreq());

    /* Status LED on PB0. */
    pins(GPIOB, GPIO_PIN_0, GPIO_MODE_OUTPUT_PP, 0);

    /* The four inputs: one sequence, into memory by DMA (DMAMUX request ADC1). */
    pins(GPIOA, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_MODE_ANALOG, 0);
    adc_dma.Instance = DMA1_Channel1;
    adc_dma.Init.Request = DMA_REQUEST_ADC1;
    adc_dma.Init.Direction = DMA_PERIPH_TO_MEMORY;
    adc_dma.Init.PeriphInc = DMA_PINC_DISABLE;
    adc_dma.Init.MemInc = DMA_MINC_ENABLE;
    adc_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    adc_dma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    adc_dma.Init.Mode = DMA_NORMAL;
    adc_dma.Init.Priority = DMA_PRIORITY_HIGH;
    if (HAL_DMA_Init(&adc_dma) != HAL_OK) fail("dma");
    __HAL_LINKDMA(&adc, DMA_Handle, adc_dma);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    adc.Instance = ADC1;
    adc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    adc.Init.Resolution = ADC_RESOLUTION_12B;
    adc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    adc.Init.ScanConvMode = ADC_SCAN_ENABLE;
    adc.Init.EOCSelection = ADC_EOC_SEQ_CONV;
    adc.Init.NbrOfConversion = 5;
    adc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    adc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    if (HAL_ADC_Init(&adc) != HAL_OK) fail("adc");
    /* The four inputs, then VREFINT (its sampling time at least the 4 us the data sheet asks). */
    static const uint32_t channels[5] = {ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3, ADC_CHANNEL_4, ADC_CHANNEL_VREFINT};
    static const uint32_t ranks[5] = {ADC_REGULAR_RANK_1, ADC_REGULAR_RANK_2, ADC_REGULAR_RANK_3, ADC_REGULAR_RANK_4, ADC_REGULAR_RANK_5};
    for (int k = 0; k < 5; k++) {
        ADC_ChannelConfTypeDef ch = {0};
        ch.Channel = channels[k];
        ch.Rank = ranks[k];
        ch.SamplingTime = k == 4 ? ADC_SAMPLETIME_640CYCLES_5 : ADC_SAMPLETIME_92CYCLES_5;
        ch.SingleDiff = ADC_SINGLE_ENDED;
        ch.OffsetNumber = ADC_OFFSET_NONE;
        if (HAL_ADC_ConfigChannel(&adc, &ch) != HAL_OK) fail("adc channel");
    }
    if (HAL_ADCEx_Calibration_Start(&adc, ADC_SINGLE_ENDED) != HAL_OK) fail("adc calibration");

    /* The IMU on I2C1: PA15 SCL, PB7 SDA, 400 kHz at 170 MHz. */
    pins(GPIOA, GPIO_PIN_15, GPIO_MODE_AF_OD, GPIO_AF4_I2C1);
    pins(GPIOB, GPIO_PIN_7, GPIO_MODE_AF_OD, GPIO_AF4_I2C1);
    i2c.Instance = I2C1;
    i2c.Init.Timing = 0x10802D9B;
    i2c.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    if (HAL_I2C_Init(&i2c) != HAL_OK) fail("i2c");
    imu_start();

    /* FDCAN1 on PA11 (RX) / PA12 (TX): classic CAN at 500 kbit/s from PCLK1 (170 MHz / 17, 20 tq);
     * frame 0x200 to FIFO 0, the rest rejected. */
    pins(GPIOA, GPIO_PIN_11 | GPIO_PIN_12, GPIO_MODE_AF_PP, GPIO_AF9_FDCAN1);
    can.Instance = FDCAN1;
    can.Init.ClockDivider = FDCAN_CLOCK_DIV1;
    can.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
    can.Init.Mode = FDCAN_MODE_NORMAL;
    can.Init.AutoRetransmission = ENABLE;
    can.Init.NominalPrescaler = 17;
    can.Init.NominalSyncJumpWidth = 1;
    can.Init.NominalTimeSeg1 = 15;
    can.Init.NominalTimeSeg2 = 4;
    can.Init.DataPrescaler = 1;
    can.Init.DataSyncJumpWidth = 1;
    can.Init.DataTimeSeg1 = 1;
    can.Init.DataTimeSeg2 = 1;
    can.Init.StdFiltersNbr = 1;
    can.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
    if (HAL_FDCAN_Init(&can) != HAL_OK) fail("fdcan");
    FDCAN_FilterTypeDef filter = {0};
    filter.IdType = FDCAN_STANDARD_ID;
    filter.FilterType = FDCAN_FILTER_DUAL;
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1 = 0x200;
    filter.FilterID2 = 0x200;
    HAL_FDCAN_ConfigFilter(&can, &filter);
    HAL_FDCAN_ConfigGlobalFilter(&can, FDCAN_REJECT, FDCAN_REJECT, FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);
    HAL_FDCAN_ActivateNotification(&can, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
    HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
    if (HAL_FDCAN_Start(&can) != HAL_OK) fail("fdcan start");

    /* TIM6: 100 Hz (170 MHz / 1700 / 1000). */
    tick.Instance = TIM6;
    tick.Init.Prescaler = 1699;
    tick.Init.Period = 999;
    HAL_TIM_Base_Init(&tick);
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
    HAL_TIM_Base_Start_IT(&tick);
    printf("sending at 100 Hz\n");

    unsigned frames = 0;
    for (;;) {
        while (!due) __WFI();
        due = 0;
        HAL_ADC_Start_DMA(&adc, (uint32_t *)raw, 5);
        while (HAL_DMA_GetState(&adc_dma) == HAL_DMA_STATE_BUSY) {}
        int16_t pressures[4];
        uint32_t vdda = raw[4] ? __HAL_ADC_CALC_VREFANALOG_VOLTAGE(raw[4], ADC_RESOLUTION_12B) : 3300u;
        for (int k = 0; k < 4; k++) pressures[k] = (int16_t)INPUT_MV(raw[k], vdda);
        send(0x100, pressures, 4);
        if (imu_ok) {
            uint8_t b[14];
            if (HAL_I2C_Mem_Read(&i2c, IMU << 1, 0x1d, I2C_MEMADD_SIZE_8BIT, b, 14, 5) == HAL_OK) {
                int16_t v[7];
                for (int k = 0; k < 7; k++) v[k] = (int16_t)(b[2 * k] << 8 | b[2 * k + 1]);
                /* +-4 g: 8192 LSB/g; +-1000 dps: 32.8 LSB/dps; T = raw / 132.48 + 25. */
                int16_t accel[4] = {(int16_t)(v[1] * 1000 / 8192), (int16_t)(v[2] * 1000 / 8192), (int16_t)(v[3] * 1000 / 8192), (int16_t)(v[0] * 100 / 132 + 2500)};
                int16_t gyro[3] = {(int16_t)(v[4] * 10 / 33), (int16_t)(v[5] * 10 / 33), (int16_t)(v[6] * 10 / 33)};
                send(0x101, accel, 4);
                send(0x102, gyro, 3);
            }
        }
        frames++;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, led_mode == 1 || (led_mode == 2 && (frames / 50) % 2) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        if (frames % 100 == 1) printf("p %d %d %d %d mV\n", pressures[0], pressures[1], pressures[2], pressures[3]);
    }
}
