/* TEST PANEL ART: ONE 8x7 panel, Keys + RGB WS2812 Pixel Art Test.
 *
 * แสดงภาพ Pixel Art 6 ลายตามที่ต้องการ พร้อมทดสอบปุ่มกด:
 *   - รูปปกติ (เริ่มต้น หรือกดปุ่มขวาล่างสุด): เรือรบกลางทะเล
 *   - ปุ่ม (1,1) หรือปุ่มแถวบนคอลัมน์ 1: ระเบิด (Hit)
 *   - ปุ่ม (2,2) หรือปุ่มแถวบนคอลัมน์ 2: น้ำกระจาย (Splash / Miss)
 *   - ปุ่ม (3,3) หรือปุ่มแถวบนคอลัมน์ 3: เป้าเล็งเรดาร์ (Target)
 *   - ปุ่ม (4,4) หรือปุ่มแถวบนคอลัมน์ 4: ถ้วยรางวัลชนะเลิศ (Trophy)
 *   - ปุ่ม (5,5) หรือปุ่มแถวบนคอลัมน์ 5: หัวกะโหลก Game Over (Skull)
 *   - ปุ่มอื่นๆ: เมื่อกดแล้ว ไฟ LED ในช่องนั้นจะเปลี่ยนสีตามลำดับ (แดง->ส้ม->เหลือง->เขียว->ฟ้า->น้ำเงิน->ม่วง->ขาว)
 *
 * Hardware Wiring (Nucleo-F767ZI v4):
 *   คอลัมน์ A..H -> PD0 CN9-25, PD1 CN9-27, PD14 CN7-16, PD3 CN9-10,
 *                   PD4 CN9-8,  PD5 CN9-6,  PD6 CN9-4,   PD7 CN9-2
 *   แถว 1..7     -> PF0 CN9-21, PF1 CN9-19, PF2 CN9-17, PF3 CN9-7,
 *                   PF4 CN10-11, PF5 CN9-9, PE15 CN10-30
 *   LED DIN      -> PE9 CN10-4 (TIM1 CH1) ผ่านไอซี 74AHCT125 (แปลง 3.3V เป็น 5V)
 *
 * การตั้งค่าใน STM32CubeMX:
 *   TIM1 CH1: PWM Generation CH1
 *   DMA2: TIM1_CH1 Memory To Peripheral, Normal, Data Width: Half Word / Half Word
 *   (ขาสวิตช์ 15 ขา โค้ดจะตั้งค่า GPIO ให้อัตโนมัติ ไม่ต้องตั้งใน CubeMX)
 */

#include "main.h"
#include "hwtests.h"
#include <stdbool.h>
#include <string.h>

/* ---------------- ขนาดและทิศทาง ---------------- */
#define HW_COLS         8U    /* จำนวนขาคอลัมน์สวิตช์บนบอร์ด (A..H) */
#define HW_ROWS         7U    /* จำนวนขาแถวสวิตช์บนบอร์ด (1..7) */
#define ART_W           8U    /* ความกว้างของรูป Pixel Art (8 คอลัมน์ A..H) */
#define ART_H           7U    /* ความสูงของรูป Pixel Art (7 แถว 1..7) */
#define TOTAL_LEDS      56U   /* 8 x 7 = 56 ดวง */

/* ปรับแต่งตามการวางสายไฟ LED บนแผงจริง:
 * LED_SERPENTINE: 1 = สายไฟเดินแบบงูเลื้อยสลับซ้าย-ขวาในแถวคู่
 * LED_FLIP_X:     1 = กลับด้านแนวนอน
 * LED_FLIP_Y:     1 = กลับด้านแนวตั้ง
 */
#define LED_SERPENTINE  0
#define LED_FLIP_X      0
#define LED_FLIP_Y      0

#define LED_BRIGHTNESS  64U   /* 0..255 (64 = ~25% สว่างชัดเจนและปลอดภัยต่อกระแสไฟ) */
#define SCAN_MS         2U    /* สแกนปุ่มทุกๆ 2 ms */
#define DEBOUNCE_SCANS  4U    /* กดค้างเสถียร 4 รอบ (8 ms) */
#define SETTLE_US       20U   /* หน่วงเวลาให้ระดับสัญญาณนิ่ง */

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} Pin;

/* ตำแหน่งขา Pinout Nucleo-F767ZI v4 */
static const Pin col_pins[HW_COLS] = {
    {GPIOD, GPIO_PIN_0},  /* Col A - CN9-25 */
    {GPIOD, GPIO_PIN_1},  /* Col B - CN9-27 */
    {GPIOD, GPIO_PIN_14}, /* Col C - CN7-16 */
    {GPIOD, GPIO_PIN_3},  /* Col D - CN9-10 */
    {GPIOD, GPIO_PIN_4},  /* Col E - CN9-8  */
    {GPIOD, GPIO_PIN_5},  /* Col F - CN9-6  */
    {GPIOD, GPIO_PIN_6},  /* Col G - CN9-4  */
    {GPIOD, GPIO_PIN_7}   /* Col H - CN9-2  */
};

static const Pin row_pins[HW_ROWS] = {
    {GPIOF, GPIO_PIN_0},  /* Row 1 - CN9-21  */
    {GPIOF, GPIO_PIN_1},  /* Row 2 - CN9-19  */
    {GPIOF, GPIO_PIN_2},  /* Row 3 - CN9-17  */
    {GPIOF, GPIO_PIN_3},  /* Row 4 - CN9-7   */
    {GPIOF, GPIO_PIN_4},  /* Row 5 - CN10-11 */
    {GPIOF, GPIO_PIN_5},  /* Row 6 - CN9-9   */
    {GPIOE, GPIO_PIN_15}  /* Row 7 - CN10-30 */
};

extern TIM_HandleTypeDef htim1;

/* ตัวแปรสำหรับตรวจสอบใน STM32CubeIDE -> Live Expressions */
volatile uint8_t  test_art_pattern;      /* 0=Normal, 1=Hit, 2=Splash, 3=Target, 4=Trophy, 5=Skull */
volatile uint16_t test_art_presses;      /* จำนวนครั้งที่กดปุ่ม */
volatile uint8_t  test_art_last_x;       /* พิกัดปุ่มล่าสุด X (0..7) */
volatile uint8_t  test_art_last_y;       /* พิกัดปุ่มล่าสุด Y (0..6) */
volatile uint8_t  test_art_led_ok;       /* 1 = DMA พร้อมทำงาน */
volatile uint8_t  test_art_raw[HW_COLS]; /* ค่าบิตแถวที่กดอยู่ขณะนี้ */

/* ---------------- โครงสร้างสี ---------------- */
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Color;

#define C_BLACK   ((Color){   0,   0,   0 })
#define C_RED     ((Color){ 255,   0,   0 })
#define C_ORANGE  ((Color){ 255,  80,   0 })
#define C_YELLOW  ((Color){ 255, 200,   0 })
#define C_BLUE    ((Color){   0,  60, 255 })
#define C_WHITE   ((Color){ 255, 255, 255 })
#define C_GREEN   ((Color){   0, 255,   0 })
#define C_CYAN    ((Color){   0, 220, 255 })
#define C_PURPLE  ((Color){ 160,   0, 255 })

static const Color step_colors[] = {
    C_RED, C_ORANGE, C_YELLOW, C_GREEN, C_CYAN, C_BLUE, C_PURPLE, C_WHITE
};
#define STEP_COLOR_COUNT (sizeof(step_colors) / sizeof(step_colors[0]))

/* ---------------- ข้อมูลภาพ Pixel Art 6 ลาย (8 Columns x 7 Rows) ---------------- */

/* ลาย 0: ปกติ (เรือรบกลางทะเล) */
static const Color img_normal[ART_H][ART_W] = {
    /* 1 */ { C_BLACK, C_BLACK, C_BLACK, C_RED,   C_BLACK, C_BLACK, C_BLACK, C_BLACK },
    /* 2 */ { C_BLACK, C_BLACK, C_BLACK, C_WHITE, C_BLACK, C_BLACK, C_BLACK, C_BLACK },
    /* 3 */ { C_BLACK, C_BLACK, C_WHITE, C_WHITE, C_WHITE, C_WHITE, C_BLACK, C_BLACK },
    /* 4 */ { C_BLACK, C_RED,   C_RED,   C_RED,   C_RED,   C_RED,   C_RED,   C_BLACK },
    /* 5 */ { C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE  },
    /* 6 */ { C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE  },
    /* 7 */ { C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE  }
};

/* ลาย 1: (1,1) ระเบิด (Hit) */
static const Color img_hit[ART_H][ART_W] = {
    /* 1 */ { C_BLACK, C_RED,    C_BLACK,  C_BLACK,  C_BLACK,  C_BLACK,  C_RED,    C_BLACK },
    /* 2 */ { C_BLACK, C_BLACK,  C_ORANGE, C_RED,    C_RED,    C_ORANGE, C_BLACK,  C_BLACK },
    /* 3 */ { C_RED,   C_ORANGE, C_YELLOW, C_YELLOW, C_YELLOW, C_YELLOW, C_ORANGE, C_RED   },
    /* 4 */ { C_BLACK, C_RED,    C_YELLOW, C_WHITE,  C_WHITE,  C_YELLOW, C_RED,    C_BLACK },
    /* 5 */ { C_BLACK, C_ORANGE, C_YELLOW, C_YELLOW, C_YELLOW, C_YELLOW, C_ORANGE, C_BLACK },
    /* 6 */ { C_BLUE,  C_BLUE,   C_RED,    C_RED,    C_RED,    C_RED,    C_BLUE,   C_BLUE  },
    /* 7 */ { C_BLUE,  C_BLUE,   C_BLUE,   C_BLUE,   C_BLUE,   C_BLUE,   C_BLUE,   C_BLUE  }
};

/* ลาย 2: (2,2) น้ำกระจาย (Splash / Miss) */
static const Color img_splash[ART_H][ART_W] = {
    /* 1 */ { C_BLACK, C_BLUE,  C_BLACK, C_BLACK, C_BLACK, C_BLACK, C_BLUE,  C_BLACK },
    /* 2 */ { C_BLACK, C_BLACK, C_BLUE,  C_BLACK, C_BLACK, C_BLUE,  C_BLACK, C_BLACK },
    /* 3 */ { C_BLUE,  C_BLACK, C_BLUE,  C_BLACK, C_BLACK, C_BLUE,  C_BLACK, C_BLUE  },
    /* 4 */ { C_BLACK, C_BLUE,  C_BLACK, C_BLUE,  C_BLUE,  C_BLACK, C_BLUE,  C_BLACK },
    /* 5 */ { C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE  },
    /* 6 */ { C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE  },
    /* 7 */ { C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE,  C_BLUE  }
};

/* ลาย 3: (3,3) เป้าเล็งเรดาร์ (Target) */
static const Color img_target[ART_H][ART_W] = {
    /* 1 */ { C_BLACK, C_BLACK, C_BLACK, C_RED,    C_RED,    C_BLACK, C_BLACK, C_BLACK },
    /* 2 */ { C_BLACK, C_BLACK, C_RED,   C_BLACK,  C_BLACK,  C_RED,   C_BLACK, C_BLACK },
    /* 3 */ { C_BLACK, C_RED,   C_BLACK, C_RED,    C_RED,    C_BLACK, C_RED,   C_BLACK },
    /* 4 */ { C_RED,   C_BLACK, C_RED,   C_YELLOW, C_YELLOW, C_RED,   C_BLACK, C_RED   },
    /* 5 */ { C_BLACK, C_RED,   C_BLACK, C_RED,    C_RED,    C_BLACK, C_RED,   C_BLACK },
    /* 6 */ { C_BLACK, C_BLACK, C_RED,   C_BLACK,  C_BLACK,  C_RED,   C_BLACK, C_BLACK },
    /* 7 */ { C_BLACK, C_BLACK, C_BLACK, C_RED,    C_RED,    C_BLACK, C_BLACK, C_BLACK }
};

/* ลาย 4: (4,4) ถ้วยรางวัล (Trophy) */
static const Color img_trophy[ART_H][ART_W] = {
    /* 1 */ { C_BLACK,  C_YELLOW, C_YELLOW, C_YELLOW, C_YELLOW, C_YELLOW, C_YELLOW, C_BLACK  },
    /* 2 */ { C_YELLOW, C_YELLOW, C_YELLOW, C_YELLOW, C_YELLOW, C_YELLOW, C_YELLOW, C_YELLOW },
    /* 3 */ { C_YELLOW, C_BLACK,  C_YELLOW, C_YELLOW, C_YELLOW, C_YELLOW, C_BLACK,  C_YELLOW },
    /* 4 */ { C_BLACK,  C_BLACK,  C_YELLOW, C_YELLOW, C_YELLOW, C_YELLOW, C_BLACK,  C_BLACK  },
    /* 5 */ { C_BLACK,  C_BLACK,  C_BLACK,  C_YELLOW, C_YELLOW, C_BLACK,  C_BLACK,  C_BLACK  },
    /* 6 */ { C_BLACK,  C_BLACK,  C_YELLOW, C_YELLOW, C_YELLOW, C_YELLOW, C_BLACK,  C_BLACK  },
    /* 7 */ { C_BLACK,  C_YELLOW, C_YELLOW, C_YELLOW, C_YELLOW, C_YELLOW, C_YELLOW, C_BLACK  }
};

/* ลาย 5: (5,5) หัวกะโหลก (Skull / Game Over) */
static const Color img_skull[ART_H][ART_W] = {
    /* 1 */ { C_BLACK, C_WHITE, C_WHITE, C_WHITE, C_WHITE, C_WHITE, C_WHITE, C_BLACK },
    /* 2 */ { C_WHITE, C_WHITE, C_WHITE, C_WHITE, C_WHITE, C_WHITE, C_WHITE, C_WHITE },
    /* 3 */ { C_WHITE, C_RED,   C_WHITE, C_WHITE, C_WHITE, C_WHITE, C_RED,   C_WHITE },
    /* 4 */ { C_WHITE, C_WHITE, C_WHITE, C_BLACK, C_BLACK, C_WHITE, C_WHITE, C_WHITE },
    /* 5 */ { C_BLACK, C_WHITE, C_WHITE, C_WHITE, C_WHITE, C_WHITE, C_WHITE, C_BLACK },
    /* 6 */ { C_BLACK, C_WHITE, C_BLACK, C_WHITE, C_WHITE, C_BLACK, C_WHITE, C_BLACK },
    /* 7 */ { C_BLACK, C_BLACK, C_WHITE, C_WHITE, C_WHITE, C_WHITE, C_BLACK, C_BLACK }
};

/* ตารางเก็บสี override ของแต่ละช่องเมื่อถูกกดทดสอบ (0 = ใช้สีตามรูปภาพ) */
static uint8_t key_step[TOTAL_LEDS];

/* ตัวแปรจัดการสวิตช์และ LED */
static uint8_t stable_state[HW_COLS];
static uint8_t debounce_count[HW_COLS][HW_ROWS];
static uint32_t last_scan_time;

#define RESET_SLOTS 240U
static uint16_t led_dma[TOTAL_LEDS * 24U + RESET_SLOTS] __attribute__((aligned(32)));
static uint16_t ccr_0, ccr_1;
static volatile bool led_busy;
static bool need_redraw = true;

/* ---------------- ฟังก์ชันควบคุม LED WS2812 ---------------- */

static uint32_t tim1_clock_hz(void)
{
    RCC_ClkInitTypeDef clk;
    uint32_t latency;
    HAL_RCC_GetClockConfig(&clk, &latency);
    return (clk.APB2CLKDivider == RCC_HCLK_DIV1) ? HAL_RCC_GetPCLK2Freq() : (2U * HAL_RCC_GetPCLK2Freq());
}

static uint16_t ns_to_ticks(uint32_t clk_hz, uint32_t ns)
{
    return (uint16_t)(((uint64_t)clk_hz * ns + 500000000ULL) / 1000000000ULL);
}

static void leds_init(void)
{
    const uint32_t clk = tim1_clock_hz();
    __HAL_TIM_SET_AUTORELOAD(&htim1, ns_to_ticks(clk, 1250U) - 1U);   /* 1.25 us ต่อบิต (800 kHz) */
    ccr_0 = ns_to_ticks(clk, 400U);                                  /* บิต 0: High 0.40 us */
    ccr_1 = ns_to_ticks(clk, 800U);                                  /* บิต 1: High 0.80 us */
    test_art_led_ok = (htim1.hdma[TIM_DMA_ID_CC1] != NULL);
    memset(led_dma, 0, sizeof(led_dma));
}

/* คำนวณลำดับ index ของดวงไฟ LED ในสาย (0..55) */
static uint8_t map_led_index(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    if (LED_FLIP_X) { x = (uint8_t)(width - 1U - x); }
    if (LED_FLIP_Y) { y = (uint8_t)(height - 1U - y); }
    if (LED_SERPENTINE && (y & 1U)) { x = (uint8_t)(width - 1U - x); }
    return (uint8_t)(y * width + x);
}

static void put_led(uint8_t index, Color c)
{
    const uint8_t bytes[3] = { c.g, c.r, c.b }; /* WS2812 ลำดับส่ง: G -> R -> B */
    uint16_t *out = &led_dma[index * 24U];
    uint8_t i, bit;
    for (i = 0; i < 3U; ++i) {
        const uint8_t val = (uint8_t)((bytes[i] * LED_BRIGHTNESS + 127U) / 255U);
        for (bit = 0x80U; bit != 0U; bit >>= 1) {
            *out++ = (val & bit) ? ccr_1 : ccr_0;
        }
    }
}

/* นำภาพและค่าสีที่ถูกกดมาแสดงบนไฟ LED */
static void render_display(void)
{
    uint8_t y, x;
    const Color (*img_ptr)[ART_W];

    switch (test_art_pattern) {
        case 1:  img_ptr = img_hit;     break;
        case 2:  img_ptr = img_splash;  break;
        case 3:  img_ptr = img_target;  break;
        case 4:  img_ptr = img_trophy;  break;
        case 5:  img_ptr = img_skull;   break;
        default: img_ptr = img_normal;  break;
    }

    /* แสดงภาพขนาด 8 คอลัมน์ x 7 แถว ตรงตามขนาดกระดาน */
    for (y = 0; y < ART_H; ++y) {
        for (x = 0; x < ART_W; ++x) {
            const uint8_t idx = map_led_index(x, y, ART_W, ART_H);
            Color c = (key_step[idx] > 0U) ? step_colors[key_step[idx] - 1U] : img_ptr[y][x];
            put_led(idx, c);
        }
    }

#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if (SCB->CCR & SCB_CCR_DC_Msk) {
        SCB_CleanDCache_by_Addr((uint32_t *)led_dma, (int32_t)sizeof(led_dma));
    }
#endif

    led_busy = true;
    if (HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_1, (uint32_t *)led_dma, TOTAL_LEDS * 24U + RESET_SLOTS) != HAL_OK) {
        led_busy = false;
        need_redraw = true;
    }
}

/* ---------------- ฟังก์ชันสวิตช์และปุ่มกด ---------------- */

static void delay_us(uint32_t us)
{
    volatile uint32_t n = us * (SystemCoreClock / 1000000U) / 2U;
    while (n-- != 0U) {}
}

static void keys_init(void)
{
    GPIO_InitTypeDef g = {0};
    uint8_t i;
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    /* คอลัมน์ Output Open Drain (High-Z เมื่อปล่อย) */
    for (i = 0; i < HW_COLS; ++i) {
        HAL_GPIO_WritePin(col_pins[i].port, col_pins[i].pin, GPIO_PIN_SET);
        g.Pin = col_pins[i].pin;
        g.Mode = GPIO_MODE_OUTPUT_OD;
        g.Pull = GPIO_NOPULL;
        g.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(col_pins[i].port, &g);
    }
    /* แถว Input Pull-up */
    for (i = 0; i < HW_ROWS; ++i) {
        g.Pin = row_pins[i].pin;
        g.Mode = GPIO_MODE_INPUT;
        g.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(row_pins[i].port, &g);
    }
}

/* เปลี่ยนภาพและล้างค่าสี override */
static void select_pattern(uint8_t pat)
{
    test_art_pattern = pat;
    memset(key_step, 0, sizeof(key_step)); /* เคลียร์สี override เพื่อแสดงรูปเต็ม */
    need_redraw = true;
}

/* ตรวจสอบเมื่อมีการกดปุ่ม (col: 0..7, row: 0..6) */
static void handle_button_press(uint8_t col, uint8_t row)
{
    ++test_art_presses;
    test_art_last_x = col;
    test_art_last_y = row;

    /* 1. ปุ่มขวาล่างสุด (Col สุดท้าย H และ Row สุดท้าย 7) -> ภาพปกติ */
    if (col == HW_COLS - 1U && row == HW_ROWS - 1U) {
        select_pattern(0);
        return;
    }

    /* 2. ปุ่มเปลี่ยนภาพ: รองรับทั้งปุ่มแถวบนสุด (Row 0 / Col 0..4)
     *    และปุ่มแนวทแยง Row == Col (คือ (1,1), (2,2), (3,3), (4,4), (5,5))
     */
    if ((row == 0U && col == 0U) || (col == 0U && row == 0U)) {
        select_pattern(1); /* (1,1) ระเบิด Hit */
        return;
    }
    if ((row == 0U && col == 1U) || (col == 1U && row == 1U)) {
        select_pattern(2); /* (2,2) น้ำกระจาย Splash */
        return;
    }
    if ((row == 0U && col == 2U) || (col == 2U && row == 2U)) {
        select_pattern(3); /* (3,3) เป้าเล็ง Target */
        return;
    }
    if ((row == 0U && col == 3U) || (col == 3U && row == 3U)) {
        select_pattern(4); /* (4,4) ถ้วยรางวัล Trophy */
        return;
    }
    if ((row == 0U && col == 4U) || (col == 4U && row == 4U)) {
        select_pattern(5); /* (5,5) หัวกะโหลก Skull */
        return;
    }

    /* 3. ปุ่มอื่นๆ ทั่วไป: เปลี่ยนสีในช่องนั้นวนรอบ เพื่อทดสอบปุ่มและหลอด LED */
    {
        uint8_t cell_idx = map_led_index(col, row, ART_W, ART_H);
        if (cell_idx < TOTAL_LEDS) {
            key_step[cell_idx] = (uint8_t)(key_step[cell_idx] % STEP_COLOR_COUNT + 1U);
            need_redraw = true;
        }
    }
}

/* สแกนปุ่ม matrix ด้วยสัญญาณ Open Drain ทีละคอลัมน์ */
static void scan_keys(void)
{
    uint8_t c, r;
    for (c = 0; c < HW_COLS; ++c) {
        uint8_t raw = 0;
        HAL_GPIO_WritePin(col_pins[c].port, col_pins[c].pin, GPIO_PIN_RESET);
        delay_us(SETTLE_US);
        for (r = 0; r < HW_ROWS; ++r) {
            if (HAL_GPIO_ReadPin(row_pins[r].port, row_pins[r].pin) == GPIO_PIN_RESET) {
                raw |= (uint8_t)(1U << r);
            }
        }
        HAL_GPIO_WritePin(col_pins[c].port, col_pins[c].pin, GPIO_PIN_SET);
        test_art_raw[c] = raw;

        for (r = 0; r < HW_ROWS; ++r) {
            const uint8_t bit = (uint8_t)(1U << r);
            if ((raw & bit) == (stable_state[c] & bit)) {
                debounce_count[c][r] = 0;
            } else if (++debounce_count[c][r] >= DEBOUNCE_SCANS) {
                debounce_count[c][r] = 0;
                stable_state[c] ^= bit;
                if ((raw & bit) != 0U) {
                    handle_button_press(c, r);
                }
            }
        }
    }
}

/* ---------------- ฟังก์ชันหลักสำหรับเรียกใช้งาน ---------------- */

void TestPanelArt_Init(void)
{
    keys_init();
    leds_init();
    select_pattern(0); /* เริ่มต้นแสดงภาพปกติ */
}

void TestPanelArt_Loop(void)
{
    const uint32_t now = HAL_GetTick();

    if (now - last_scan_time >= SCAN_MS) {
        last_scan_time = now;
        scan_keys();
    }
    if (need_redraw && test_art_led_ok && !led_busy) {
        need_redraw = false;
        render_display();
    }
}

/* DMA Callback เมื่อส่งข้อมูลให้แถบไฟ WS2812 เสร็จสิ้น */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
        HAL_TIM_PWM_Stop_DMA(&htim1, TIM_CHANNEL_1);
        led_busy = false;
    }
}
