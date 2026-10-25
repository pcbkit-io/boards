/* A 24 V AC wall thermostat on an ESP32-WROOM-32E: an SHT40 reads the room every two seconds, a
 * hysteresis controller (0.5 °C either side of the set point) closes W (heat) or Y (cool) and G
 * (the fan, with either of them), the 0.96" SSD1306 shows the temperature large, the set point,
 * the mode, the humidity and which calls are on; MODE cycles OFF / HEAT / COOL / AUTO, UP and DOWN
 * move the set point by 0.5 °C (10-30 °C). On ESP-IDF's register headers through the kit
 * (<pcbkit/esp32.h>); I2C0 at 400 kHz on GPIO21 (SDA) and GPIO22 (SCL); the buttons on GPIO32,
 * GPIO33 and GPIO25 to ground (pull-ups); the relays' drivers on GPIO26 (W), GPIO27 (Y) and GPIO14
 * (G). Wi-Fi comes later: the simulator does not run the radio, so this program is the
 * thermostat without it. */
#include <stdio.h>
#include <string.h>
#include <pcbkit/esp32.h>
#include "soc/gpio_sig_map.h"
#include "soc/i2c_reg.h"

const struct pcbkit_board pcbkit_board = {.uart_baud = 115200};

#define R(a) (*(volatile uint32_t *)(a))
enum { SDA = 21, SCL = 22, UP = 32, DOWN = 33, MODE = 25, W = 26, Y = 27, G = 14 };
enum { OLED = 0x3c, SHT40 = 0x44 };

/* ---- I2C0: the controller's command list (RSTART, WRITE, READ, STOP, END) ------------------- */
static void i2c_reset_fifo(void) {
    R(I2C_FIFO_CONF_REG(0)) |= 1u << 12 | 1u << 13;
    R(I2C_FIFO_CONF_REG(0)) &= ~(1u << 12 | 1u << 13);
}

static int i2c_run(int n_commands) {
    (void)n_commands;
    R(I2C_INT_CLR_REG(0)) = 0x1fff;
    R(I2C_CTR_REG(0)) |= 1u << 5;
    uint64_t deadline = pcbkit_time_us() + 20000;
    while (!(R(I2C_INT_RAW_REG(0)) & (1u << 7 | 1u << 10))) if (pcbkit_time_us() > deadline) return -2;
    return (R(I2C_INT_RAW_REG(0)) & 1u << 10) ? -1 : 0;
}

/* Write `n` bytes (at most 31) to `address`. */
static int i2c_write(uint8_t address, const uint8_t *data, int n) {
    i2c_reset_fifo();
    R(I2C_DATA_APB_REG(0)) = address << 1;
    for (int i = 0; i < n; i++) R(I2C_DATA_APB_REG(0)) = data[i];
    R(I2C_COMD0_REG(0)) = 0u << 11;
    R(I2C_COMD1_REG(0)) = 1u << 11 | 1u << 8 | (uint32_t)(n + 1);
    R(I2C_COMD2_REG(0)) = 3u << 11;
    return i2c_run(3);
}

/* Read `n` bytes (2-32) from `address`. */
static int i2c_read(uint8_t address, uint8_t *out, int n) {
    i2c_reset_fifo();
    R(I2C_DATA_APB_REG(0)) = address << 1 | 1;
    R(I2C_COMD0_REG(0)) = 0u << 11;
    R(I2C_COMD1_REG(0)) = 1u << 11 | 1u << 8 | 1;
    R(I2C_COMD2_REG(0)) = 2u << 11 | (uint32_t)(n - 1);
    R(I2C_COMD3_REG(0)) = 2u << 11 | 1u << 10 | 1;
    R(I2C_COMD4_REG(0)) = 3u << 11;
    int st = i2c_run(5);
    if (st == 0) for (int i = 0; i < n; i++) out[i] = (uint8_t)R(I2C_DATA_APB_REG(0));
    return st;
}

static void i2c_start(void) {
    R(DPORT_PERIP_CLK_EN_REG) |= DPORT_I2C_EXT0_CLK_EN;
    R(DPORT_PERIP_RST_EN_REG) &= ~DPORT_I2C_EXT0_RST;
    pcbkit_pin_signal(SDA, I2CEXT0_SDA_OUT_IDX, I2CEXT0_SDA_IN_IDX);
    pcbkit_pin_signal(SCL, I2CEXT0_SCL_OUT_IDX, I2CEXT0_SCL_IN_IDX);
    R(GPIO_PIN21_REG) |= 1u << 2; /* open drain */
    R(GPIO_PIN22_REG) |= 1u << 2;
    R(I2C_CTR_REG(0)) = 1u << 4 | 1u << 8 | 3u;   /* master, clock always on, open drain */
    R(I2C_SCL_LOW_PERIOD_REG(0)) = 100;            /* 80 MHz / 200: 400 kHz */
    R(I2C_SCL_HIGH_PERIOD_REG(0)) = 100;
}

/* ---- the SHT40 --------------------------------------------------------------------------------- */
static uint8_t crc8(const uint8_t *b, int n) {
    uint8_t c = 0xff;
    for (int i = 0; i < n; i++) {
        c ^= b[i];
        for (int k = 0; k < 8; k++) c = (uint8_t)(c & 0x80 ? (c << 1) ^ 0x31 : c << 1);
    }
    return c;
}

static int sht40(int *centi_celsius, int *permille_rh) {
    uint8_t cmd = 0xfd, b[6];
    if (i2c_write(SHT40, &cmd, 1)) return -1;
    pcbkit_sleep_until(pcbkit_time_us() + 10000);
    if (i2c_read(SHT40, b, 6) || crc8(b, 2) != b[2] || crc8(b + 3, 2) != b[5]) return -1;
    uint32_t t = (uint32_t)b[0] << 8 | b[1], h = (uint32_t)b[3] << 8 | b[4];
    *centi_celsius = (int)(t * 17500u / 65535u) - 4500;
    int rh = (int)(h * 1250u / 65535u) - 60;
    *permille_rh = rh < 0 ? 0 : rh > 1000 ? 1000 : rh;
    return 0;
}

/* ---- the SSD1306 (128 x 64): a frame buffer sent page by page ----------------------------------- */
static uint8_t frame[8][128];

static void oled_commands(const uint8_t *c, int n) {
    for (int i = 0; i < n; i++) {
        uint8_t b[2] = {0x00, c[i]};
        i2c_write(OLED, b, 2);
    }
}

static void oled_start(void) {
    static const uint8_t init[] = {0xae, 0xd5, 0x80, 0xa8, 0x3f, 0xd3, 0x00, 0x40, 0x8d, 0x14, 0x20, 0x00, 0xa1, 0xc8, 0xda, 0x12, 0x81, 0xcf, 0xd9, 0xf1, 0xdb, 0x40, 0xa4, 0xa6, 0xaf};
    oled_commands(init, sizeof init);
}

static void oled_show(void) {
    static const uint8_t window[] = {0x21, 0, 127, 0x22, 0, 7};
    oled_commands(window, sizeof window);
    for (int page = 0; page < 8; page++)
        for (int x = 0; x < 128; x += 16) {
            uint8_t b[17];
            b[0] = 0x40;
            memcpy(b + 1, &frame[page][x], 16);
            i2c_write(OLED, b, 17);
        }
}

/* A 5 x 7 font from space to Z (columns, bit 0 at the top). */
static const uint8_t font[][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5f,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7f,0x14,0x7f,0x14},
    {0x24,0x2a,0x7f,0x2a,0x12},{0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1c,0x22,0x41,0x00},{0x00,0x41,0x22,0x1c,0x00},{0x08,0x2a,0x1c,0x2a,0x08},{0x08,0x08,0x3e,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},{0x20,0x10,0x08,0x04,0x02},
    {0x3e,0x51,0x49,0x45,0x3e},{0x00,0x42,0x7f,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},
    {0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3c,0x4a,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1e},{0x00,0x36,0x36,0x00,0x00},{0x00,0x56,0x36,0x00,0x00},
    {0x00,0x08,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},{0x41,0x22,0x14,0x08,0x00},{0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3e},{0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},{0x3e,0x41,0x41,0x41,0x22},
    {0x7f,0x41,0x41,0x22,0x1c},{0x7f,0x49,0x49,0x49,0x41},{0x7f,0x09,0x09,0x01,0x01},{0x3e,0x41,0x41,0x51,0x32},
    {0x7f,0x08,0x08,0x08,0x7f},{0x00,0x41,0x7f,0x41,0x00},{0x20,0x40,0x41,0x3f,0x01},{0x7f,0x08,0x14,0x22,0x41},
    {0x7f,0x40,0x40,0x40,0x40},{0x7f,0x02,0x04,0x02,0x7f},{0x7f,0x04,0x08,0x10,0x7f},{0x3e,0x41,0x41,0x41,0x3e},
    {0x7f,0x09,0x09,0x09,0x06},{0x3e,0x41,0x51,0x21,0x5e},{0x7f,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7f,0x01,0x01},{0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},{0x7f,0x20,0x18,0x20,0x7f},
    {0x63,0x14,0x08,0x14,0x63},{0x03,0x04,0x78,0x04,0x03},{0x61,0x51,0x49,0x45,0x43},
};

/* Text at column x, row y (pixels), scaled by s (1: 6 x 8 cells). */
static void text(int x, int y, const char *s, int scale) {
    for (; *s; s++, x += 6 * scale) {
        char c = *s >= 'a' && *s <= 'z' ? *s - 32 : *s;
        const uint8_t *g = c >= ' ' && c <= 'Z' ? font[c - ' '] : font[0];
        for (int col = 0; col < 5; col++)
            for (int row = 0; row < 7; row++)
                if (g[col] >> row & 1)
                    for (int dx = 0; dx < scale; dx++)
                        for (int dy = 0; dy < scale; dy++) {
                            int px = x + col * scale + dx, py = y + row * scale + dy;
                            if (px >= 0 && px < 128 && py >= 0 && py < 64) frame[py / 8][px] |= (uint8_t)(1u << (py % 8));
                        }
    }
}

/* ---- the controller ----------------------------------------------------------------------------- */
enum Mode { OFF, HEAT, COOL, AUTO };
static const char *mode_names[] = {"OFF", "HEAT", "COOL", "AUTO"};

int main(void) {
    printf("thermostat\n");
    for (int pin = 0; pin < 3; pin++) {
        static const int out[3] = {W, Y, G};
        pcbkit_pin_output((unsigned)out[pin], 0);
        static const int in[3] = {UP, DOWN, MODE};
        pcbkit_pin_input((unsigned)in[pin], 1);
    }
    i2c_start();
    oled_start();

    int set = 2100, mode = AUTO, temp = 0, rh = 0, ok = 0;
    int heat = 0, cool = 0, last[3] = {1, 1, 1};
    uint64_t next_read = 0;
    for (;;) {
        uint64_t now = pcbkit_time_us();
        /* The buttons, every 20 ms, on their falling edges. */
        static const int keys[3] = {UP, DOWN, MODE};
        for (int k = 0; k < 3; k++) {
            int level = pcbkit_pin_get((unsigned)keys[k]);
            if (last[k] && !level) {
                if (k == 0 && set < 3000) set += 50;
                if (k == 1 && set > 1000) set -= 50;
                if (k == 2) mode = (mode + 1) % 4;
                printf("set %d.%d %s\n", set / 100, set % 100 / 10, mode_names[mode]);
            }
            last[k] = level;
        }
        if (now >= next_read) {
            next_read = now + 2000000;
            ok = sht40(&temp, &rh) == 0;
            if (!ok) printf("sensor not answering\n");
        }
        /* Hysteresis: 0.5 °C either side; AUTO heats below and cools above. */
        if (ok) {
            int want_heat = mode == HEAT || mode == AUTO, want_cool = mode == COOL || mode == AUTO;
            if (!want_heat || temp > set + 50) heat = 0; else if (temp < set - 50) heat = 1;
            if (!want_cool || temp < set - 50) cool = 0; else if (temp > set + 50) cool = 1;
        } else {
            heat = cool = 0;
        }
        pcbkit_pin_put(W, heat);
        pcbkit_pin_put(Y, cool);
        pcbkit_pin_put(G, heat || cool);

        memset(frame, 0, sizeof frame);
        char line[24];
        if (ok) snprintf(line, sizeof line, "%d.%d", temp / 100, (temp < 0 ? -temp : temp) % 100 / 10);
        else snprintf(line, sizeof line, "--.-");
        text(0, 0, line, 4);
        text(104, 0, "C", 2);
        snprintf(line, sizeof line, "SET %d.%d", set / 100, set % 100 / 10);
        text(0, 36, line, 1);
        snprintf(line, sizeof line, "RH %d%%", rh / 10);
        text(80, 36, line, 1);
        text(0, 48, mode_names[mode], 2);
        snprintf(line, sizeof line, "%s%s%s", heat ? "W " : "", cool ? "Y " : "", heat || cool ? "G" : "");
        text(64, 52, line, 1);
        oled_show();
        pcbkit_sleep_until(now + 20000);
    }
}
