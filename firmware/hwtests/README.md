# โค้ดทดสอบทีละอุปกรณ์

แต่ละไฟล์ทดสอบอุปกรณ์เดียว ใช้แค่ไดรเวอร์ของอุปกรณ์นั้น → ตั้ง CubeMX เฉพาะส่วนนั้นก็ build ได้
ผลดูได้ 2 ทาง: **บนตัวอุปกรณ์เอง** และ **ตัวแปร `test_*` ใน Debug → Live Expressions**

| # | ไฟล์ | ทดสอบ | ต้องใช้ไฟล์ไดรเวอร์ |
|---|---|---|---|
| 1 | `test_led.c` | ไฟ WS2812 ×4 | `2_led/ws2812.c`, `ws2812_port_stm32.c`, `ws2812_test.c` |
| 2 | `test_switch.c` | สวิตช์ matrix 224 ปุ่ม | `1_switch/keypad.c`, `keypad_core.c` (+ไฟ ถ้าเปิด `TEST_SWITCH_WITH_LED`) |
| 3 | `test_buzzer.c` | buzzer ×2 + สวิตช์ปิดเสียง ×2 | `sound/buzzer.c` |
| 4 | `test_surrender.c` | ปุ่มยอมแพ้ ×2 | – |
| 5 | `test_lcd.c` | LCD1602 ×2 | `5_lcd/lcd1602.c` |
| 6 | `test_tft.c` | จอ TFT ×2 (ภาพ, DMA, ความเร็ว) | `4_tft/tft.c`, `gfx.c`, `font5x7.c` |
| 7 | `test_touch.c` | จอสัมผัส ×2 + คาลิเบรต | `4_tft/touch.c`, `tft.c`, `gfx.c`, `font5x7.c` |

## ขั้นตอนเหมือนกันทุกตัว

1. CubeMX: ตั้ง clock 216 MHz + peripheral ของอุปกรณ์นั้น (ตารางในแต่ละหัวข้อ, ชื่อขาตาม [PINOUT.md](../PINOUT.md))
2. เพิ่มไฟล์เข้า build: **ไฟล์ test เพียงไฟล์เดียว** + ไฟล์ไดรเวอร์ในตาราง
   (ห้ามใส่ `app/app.c` หรือ test ไฟล์อื่น เพราะ callback ของ HAL จะซ้ำกัน)
3. Include paths: `hwtests`, `app`, `1_switch`, `2_led`, `4_tft`, `5_lcd`, `sound`, `../core`
4. `main.c` (เฉพาะช่อง USER CODE):
   ```c
   /* USER CODE BEGIN Includes */
   #include "hwtests.h"
   /* USER CODE BEGIN 2 */
   TestLed_Init();          // เปลี่ยนเป็นของ test ที่ใช้
   /* USER CODE BEGIN 3 (ใน while 1) */
   TestLed_Loop();
   ```
5. Build → **Debug** (ไอคอนแมลง) → Resume (F8) → เปิด **Window → Show View → Live Expressions** แล้วพิมพ์ชื่อตัวแปร `test_...`

---

## 1. ไฟ WS2812 — `test_led.c`

| CubeMX | ค่า |
|---|---|
| TIM1 | CH1–CH4 PWM Generation, Prescaler 0, Period 269 |
| DMA2 | TIM1_CH1..CH4 **Memory to Peripheral**, Normal, Half Word / Half Word |
| ขา | PE9, PE11, PE13, PE14 |
| `app_config.h` | `WS2812_ACTIVE_MASK` = channel ที่ต่อแล้ว (เส้นเดียว = `0x01U`) |

**วิธีทดสอบ**
1. ต่อไฟทีละเส้นตาม BRINGUP.md ข้อ 1.2
2. รัน แล้วดูไฟ

**ผลที่ต้องได้**
- ทุกดวง: แดง → เขียว → น้ำเงิน → ขาว (1.5 วินาที/สี)
- สีประจำ channel: แผง 0 แดง, 1 เขียว, 2 น้ำเงิน, 3 เหลือง
- ไฟวิ่งทีละดวง A1 → H1 → A2 … H7
- Live: `test_led_pattern` วน 1→6, `test_led_frames` เพิ่มขึ้นเรื่อย ๆ

| ถ้า | สาเหตุ / แก้ |
|---|---|
| `test_led_frames` = 0 | interrupt ของ DMA ไม่ทำงาน → NVIC DMA2 stream, ไม่มี callback |
| ไม่ติดเลย แต่ frames เพิ่ม | ฮาร์ดแวร์: GND ร่วม, ขา OE ของ 74AHCT125 ลง GND, เข้า DIN |
| ติดดวงเดียว | ดวงแรกเปลี่ยนสีถูก = สายระหว่างดวง; ไม่ถูก = DMA Direction/Data width |
| แดงกับเขียวสลับ | ไฟใช้ลำดับ RGB → สลับ `c.g`/`c.r` ใน `ws2812.c` |
| ไฟวิ่งผิดลำดับ / สีผิดแผง | `WS2812_SERPENTINE`, `WS2812_FLIP_X/Y`, `WS2812_CHANNEL_TO_PANEL` |

---

## 2. สวิตช์ Matrix — `test_switch.c`

| CubeMX | ค่า |
|---|---|
| TIM6 | Prescaler 107, Period 199 (200 µs), **NVIC TIM6 on** |
| `COL0..COL7` | PD0, PD1, **PD14**, PD3–PD7 GPIO_Output **Open Drain**, level High |
| `ROW0..ROW27` | GPIO_Input (pull-up 10k ภายนอก หรือ Pull-up ภายใน) |
| ไฟ (ไม่บังคับ) | ถ้าต่อไฟแล้ว: ตั้งแบบ test 1 + เพิ่ม `TEST_SWITCH_WITH_LED=1` ใน Preprocessor defines |

**วิธีทดสอบ**
1. ต่อแผงทีละแผง (คอลัมน์ 8 + แถว 7 ของแผงนั้น)
2. Live Expressions: `test_key_count`, `test_last_panel`, `test_last_x`, `test_last_y`, `test_seen`, `test_seen_map`, `keypad_debug_raw`
3. กดทุกปุ่ม 56 ปุ่มทีละปุ่ม

**ผลที่ต้องได้**
- กด 1 ครั้ง `test_key_count` +1, `test_last_*` ตรงกับปุ่มที่กด (x 0 = A, y 0 = แถว 1)
- กดครบ → `test_seen[แผง]` = **56** และ `test_seen_map[แผง][0..6]` = **0xFF** ทุกแถว
- (ถ้าเปิดไฟ) ไฟช่องเดียวกับปุ่มกลายเป็นขาว, ล่าสุดแดง, แต่ละแผงพื้นสีต่างกัน
- ไม่กดอะไร: `keypad_debug_raw[0..7]` = 0 ทุกตัว

| ถ้า | สาเหตุ / แก้ |
|---|---|
| `test_seen_map` ขาดบิตเดียว | ปุ่ม/ไดโอดตัวนั้น (ไดโอดกลับทิศ) |
| ขาดทั้งแถว (ค่า 0x00 ทั้งแถว) | สายแถวนั้นหลุด หรือชื่อขา ROW ผิด |
| ขาดบิตเดียวกันทุกแถว | สายคอลัมน์นั้น |
| `keypad_debug_raw` ไม่เป็น 0 ทั้งที่ไม่กด | แถวไม่มี pull-up / ลัดลง GND |
| x หรือ y กลับด้าน | `KEYPAD_FLIP_X/Y` · แผงผิด → `KEYPAD_GROUP_TO_PANEL` |
| กดทีเดียวนับ 2 | เพิ่ม `KEYPAD_DEBOUNCE_SCANS` |

ทดสอบ ghosting: กดค้าง 3 ปุ่มเป็นรูปตัว L → `test_key_count` ต้องเพิ่มแค่ 3

---

## 3. Buzzer + สวิตช์ปิดเสียง — `test_buzzer.c`

| CubeMX | ค่า |
|---|---|
| TIM3 | CH1, CH2 PWM Generation, Prescaler 107 (1 MHz), Period 999 · ขา PC6, PC7 |
| `MUTE1`, `MUTE2` | PE0 (EXTI0), **PB1** (EXTI1) GPIO_EXTI **Rising/Falling**, Pull-up · NVIC EXTI0, EXTI1 on |

**วิธีทดสอบ**
1. รัน แล้วฟัง: เสียงทั้ง 7 แบบ ห่างกัน 1.5 วิ วนเป็นชุด **P1 อย่างเดียว → P2 อย่างเดียว → ทั้งคู่**
2. ระหว่างเสียงดัง เลื่อนสวิตช์ปิดเสียง P1
3. Live: `test_sound`, `test_side` (1 = P1, 2 = P2, 3 = ทั้งคู่), `test_muted`, `test_mute_edges`

**ผลที่ต้องได้**
- รอบ `test_side = 1` ดังเฉพาะ buzzer P1, รอบ 2 เฉพาะ P2
- เสียงแต่ละแบบต่างกันชัด (คลิก, บึ้บ ๆ, ตกลง, ขึ้น, ไล่ลง 4 โน้ต, เริ่มเกม, จบเกม)
- เลื่อนปิดเสียง → ฝั่งนั้น**เงียบทันที**กลางเสียง, อีกฝั่งดังต่อ, `test_muted[0]` = 1, `test_mute_edges[0]` +1 ต่อการเลื่อน 1 ครั้ง

| ถ้า | สาเหตุ / แก้ |
|---|---|
| ไม่ดังเลย | ขา 2N2222 (E-B-C), buzzer ต้องเป็นแบบ passive, PWM ขา PC6/PC7 |
| ดังผิดฝั่ง | สลับสาย PC6/PC7 |
| `test_mute_edges` ไม่เพิ่ม | NVIC EXTI ไม่เปิด / ตั้งเป็น GPIO_Input แทน EXTI |
| ON/OFF กลับกัน | สลับขาข้างของสวิตช์ (ขาที่ลง GND) |

---

## 4. ปุ่มยอมแพ้ — `test_surrender.c`

| CubeMX | ค่า |
|---|---|
| `SURR1`, `SURR2` | PE10, PE12 GPIO_EXTI **Falling**, Pull-up · NVIC **EXTI15_10** on |

**วิธีทดสอบ**
1. Live: `test_surr_level`, `test_surr_edges`, `test_surr_presses`, `test_surr_bounce_max`
2. กดปุ่ม P1 ช้า ๆ 10 ครั้ง แล้วรัว ๆ 10 ครั้ง ทำซ้ำกับ P2

**ผลที่ต้องได้**
- ปล่อย: `test_surr_level` = 1, กดค้าง = 0
- `test_surr_presses` เพิ่ม **ทีละ 1 ต่อการกด 1 ครั้งเสมอ** (ตัวกรองแบบเดียวกับในเกม)
- `test_surr_edges` อาจเพิ่มมากกว่า (= หน้าสัมผัสเด้ง เป็นเรื่องปกติ) — `test_surr_bounce_max` บอกว่าเด้งมากสุดกี่ครั้ง

| ถ้า | สาเหตุ / แก้ |
|---|---|
| ไม่เพิ่มเลย | NVIC EXTI15_10 ปิด, ปุ่มไม่ได้ลง GND |
| presses +2 ต่อการกด | เด้งนานเกิน 300 ms (หายาก) → เพิ่ม `DEBOUNCE_MS` / `SURRENDER_DEBOUNCE_MS` |
| level = 0 ตลอด | ปุ่มค้าง หรือสายลัดลง GND |

---

## 5. LCD1602 — `test_lcd.c`

| CubeMX | ค่า |
|---|---|
| I2C1 | Standard Mode 100 kHz, PB8 (SCL), PB9 (SDA) |

**วิธีทดสอบ**
1. รัน, Live: `test_i2c_found`, `test_i2c_count`, `test_lcd_ok`
2. ดูจอทั้งสอง

**ผลที่ต้องได้**
- `test_i2c_found` มี **0x27 และ 0x26**, `test_lcd_ok` = {1, 1}
- LCD P1 บรรทัดบน `LCD P1 ADDR 27`, P2 `LCD P2 ADDR 26`
- บรรทัดล่างสลับทุก 2 วิ: `COUNT n` (นับวินาที) กับ `ABCDEFGHIJKLMNOP` (ต้องเห็นครบ 16 ตัว)

| ถ้า | สาเหตุ / แก้ |
|---|---|
| ไฟจอติด ไม่มีตัวอักษร | หมุน contrast หลังโมดูล |
| `test_i2c_count` = 0 | SDA/SCL สลับ, ไม่มี GND/5V, ขา I2C ใน CubeMX ผิด |
| เจอ 0x3F/0x3E แทน | ชิป PCF8574A → แก้ `LCD_I2C_ADDR` ใน `app_config.h` |
| เจอแค่ 0x27 | ยังไม่บัดกรี A0 ที่จอ P2 (สองจอ address ชนกัน) |
| ตัวอักษรเพี้ยน | สาย I2C ยาว/หลวม |

---

## 6. จอ TFT — `test_tft.c`

| CubeMX | ค่า |
|---|---|
| SPI1 | Transmit Only Master, 8-bit, CPOL Low, CPHA 1 Edge, Prescaler 4 · PA5, PA6, PB5 |
| DMA2 | SPI1_TX **Stream 5**, Memory to Peripheral, Normal, Byte/Byte · NVIC DMA2 stream5 on |
| GPIO Output (High) | `CS` PC8, `DC` PC9, `RST` PC10, `CS2` PC11, `DC2` PC12, `RST2` PD2 |

**วิธีทดสอบ**
1. รัน, ดูทั้งสองจอ · Live: `test_tft_full_ms`, `test_tft_band_ms`, `test_tft_updates`

**ผลที่ต้องได้**
- บนสุด: แถบสี **R G B W** เรียงจากซ้าย
- `TOP LEFT` อยู่มุมซ้ายบน, `BOTTOM RIGHT` มุมขวาล่าง, เห็นกรอบขาว 1 px ครบ 4 ด้าน
- ตัวใหญ่ `TFT 1` บนจอ P1, `TFT 2` บนจอ P2, แถบเทาไล่ 16 ระดับ
- `UPDATE n` เพิ่มขึ้นทุก 250 ms สลับจอ (ทดสอบส่งเฉพาะแถบผ่าน DMA)
- เวลา (SPI 27 MHz): full ≈ 45 ms, band ≈ 5 ms · ถ้าใช้ prescaler 8: ≈ 91 / 10 ms

| ถ้า | สาเหตุ / แก้ |
|---|---|
| จอขาวล้วน | CS/DC/RST ผิดขา, ไม่มีไฟเลี้ยง, ค้างที่ init |
| R กับ B สลับ | `TFT_MADCTL` ใน `tft.c`: 0x48 ↔ 0x40 |
| กลับด้าน/หมุน (TOP LEFT ไม่อยู่ซ้ายบน) | ลอง `TFT_MADCTL` 0x88, 0x08, 0xC8 |
| ภาพเป็นจุดเพี้ยน | Prescaler 8 หรือ 16, สายให้สั้น |
| `UPDATE` ไม่เพิ่ม / ค้าง | DMA SPI1_TX ไม่ได้ตั้ง / NVIC ปิด (busy ค้าง) |
| ขึ้นผิดจอ | สลับสาย CS กับ CS2 |

---

## 7. จอสัมผัส + คาลิเบรต — `test_touch.c` (ทำหลัง test 6 ผ่าน)

| CubeMX | ค่า |
|---|---|
| ทุกอย่างของ test 6 | |
| GPIO Output (Very High) | `T_CLK` PE3, `T_MOSI` PE5, `T_CS` PE6 (High), `T_CS2` PE7 (High) |
| GPIO Input | `T_MISO` PE4 |
| GPIO_EXTI Falling, Pull-up | `T_IRQ` PE2, `T_IRQ2` PE8 · NVIC EXTI2, EXTI9_5 on |

**วิธีทดสอบ (ทำทีละจอ)**
1. จอแสดงเป้า 4 จุด แตะตามลำดับ **1 ซ้ายบน → 2 ขวาบน → 3 ขวาล่าง → 4 ซ้ายล่าง** (เป้าถัดไปเป็นสีแดง แตะแล้วเป็นเขียว)
2. ครบ 4 จุด จอแสดงค่า **X_MIN X_MAX Y_MIN Y_MAX SWAP_XY** (หรือดู `test_cal[จอ][0..4]`)
3. ใส่ค่าลง `app_config.h` (ตำแหน่ง P1 = ตัวแรก, P2 = ตัวที่สอง ของแต่ละบรรทัด):
   ```c
   #define TOUCH_RAW_X_MIN        { <X_MIN จอ1>, <X_MIN จอ2> }
   #define TOUCH_RAW_X_MAX        { <X_MAX จอ1>, <X_MAX จอ2> }
   #define TOUCH_RAW_Y_MIN        { ... }
   #define TOUCH_RAW_Y_MAX        { ... }
   #define TOUCH_SWAP_XY          { <SWAP จอ1>, <SWAP จอ2> }
   ```
4. Build + รันใหม่ ทำครบ 4 จุดอีกรอบ แล้วแตะที่ไหนก็ได้

**ผลที่ต้องได้**
- แตะ 1 ครั้ง `#n` +1 (ไม่ใช่ +2), ค่า RAW เปลี่ยนตามตำแหน่ง
- หลังใส่ค่าแล้ว: **กากบาทแดงขึ้นใต้นิ้ว** ทุกมุม คลาดไม่เกิน ~5 px
- แตะจอ P1 แล้วจอ P2 ไม่เปลี่ยน (และกลับกัน)

| ถ้า | สาเหตุ / แก้ |
|---|---|
| แตะแล้วไม่มีอะไร | T_IRQ ไม่ลง LOW/EXTI ไม่เปิด, T_CS ผิดจอ |
| RAW เป็น 0 หรือ 4095 ตลอด | T_MISO/T_MOSI สลับ, T_CLK ไม่ต่อ |
| ค่ากระโดดมาก | กดเบาไป (resistive ต้องกดลงไป), สายยาว |
| แตะจอ P1 แต่จอ P2 ตอบ | สลับสาย T_CS/T_CS2 หรือ T_IRQ/T_IRQ2 |

---

## หลังผ่านครบทุกตัว
- ทดสอบรวมทุกอุปกรณ์พร้อมกัน: `APP_HW_TEST 1` ใน `app_config.h` (ใช้ `app/app.c` + `app/hwtest.c` + ไดรเวอร์ทั้งหมด) — ดู BRINGUP.md
- เล่นเกมจริง: `APP_HW_TEST 0` + เช็กลิสต์ BRINGUP.md ขั้นที่ 11
