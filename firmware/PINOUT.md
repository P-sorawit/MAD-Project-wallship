# ผังขา Nucleo-F767ZI (v4 — ทุกขาอยู่บน header Zio)

อุปกรณ์: แผงสวิตช์ 4 ชุด, ไฟ WS2812 4 เส้น, จอสัมผัส TFT 2 จอ, LCD1602 2 จอ, และต่อฝั่ง: สวิตช์ยอมแพ้, สวิตช์ปิดเสียง, buzzer
รวม **64 ขา** — ตั้ง **User Label** ใน CubeMX ให้ตรงคอลัมน์ "Label" เพราะโค้ดอ้างชื่อนี้

> **ตำแหน่งบนบอร์ด** = `CNx-n` คือ header Zio ตัวไหน ขาที่เท่าไร (เลขในวงเล็บ = ชื่อ D/A ที่พิมพ์บนบอร์ด)
> อ้างอิง **UM1974 Table 13** (NUCLEO-F767ZI) · header morpho CN11/CN12 **ไม่ได้บัดกรีมาจากโรงงาน** จึงไม่ใช้
> แผนภาพแบบกดดูทีละอุปกรณ์: [docs/wiring.html](../docs/wiring.html) · ข้อมูลต้นทาง + ตัวตรวจ: [tools/pinmap.py](tools/pinmap.py)

## เปลี่ยนจาก v3 (ย้ายออกจากขาที่มีแค่บน morpho)

| สัญญาณ | v3 | **v4** | ตำแหน่ง |
|---|---|---|---|
| ROW6 | PF6 | **PE15** | CN10-30 |
| ROW11 | PF11 | **PB10** | CN10-32 |
| ROW20 | PG4 | **PB11** | CN10-34 |
| ROW21 | PG5 | **PD11** | CN10-23 |
| ROW22 | PG8 | **PD12** | CN10-21 |
| ROW24 | PG10 | **PD13** | CN10-19 |
| ROW25 | PG12 | **PB2** | CN10-15 |
| ROW27 | PG15 | **PB6** | CN10-13 |
| MUTE2 | PE1 | **PB1** (EXTI1 เหมือนเดิม) | CN10-7 |

โค้ดไม่ต้องแก้ (อ้างชื่อ Label) — แก้แค่ใน CubeMX

## สรุปตาม header

| Header | อยู่ตรงไหน | ใช้ทำ |
|---|---|---|
| **CN8** | ซ้ายบน | จอ TFT CS/DC/RST ทั้งสองจอ (PC8–PC12, PD2), แถวสวิตช์ PG2/PG3 |
| **CN9** | ซ้ายล่าง | คอลัมน์สวิตช์ PD3–PD7, PD0/PD1, แถว PF0–PF5/PF7–PF10/PG0/PG1, touch PE2–PE6 |
| **CN7** | ขวาบน | SPI1 (PA5/PA6/PB5), I2C1 LCD (PB8/PB9), buzzer (PC6/PC7), COL2 (PD14), แถว PF12 |
| **CN10** | ขวาล่าง | ไฟ WS2812 (PE9/11/13/14), touch PE7/PE8, ยอมแพ้ PE10/PE12, ปิดเสียง PE0/PB1, แถว ที่เหลือ |

## 1. ไฟ WS2812 — TIM1 + DMA2 (4 ขา)

| ขา | ตำแหน่งบนบอร์ด | Label | โหมด | ไปที่ |
|---|---|---|---|---|
| PE9 | **CN10-4** (D6) | – | TIM1_CH1 (AF) | 74AHCT125 1A (pin 2) -> panel 0 P1 fleet |
| PE11 | **CN10-6** (D5) | – | TIM1_CH2 (AF) | 74AHCT125 2A (pin 5) -> panel 1 P1 attack |
| PE13 | **CN10-10** (D3) | – | TIM1_CH3 (AF) | 74AHCT125 3A (pin 9) -> panel 2 P2 fleet |
| PE14 | **CN10-28** (D38) | – | TIM1_CH4 (AF) | 74AHCT125 4A (pin 12) -> panel 3 P2 attack |

## 2. สวิตช์ Matrix — TIM6 สแกน (36 ขา)

**คอลัมน์** — GPIO_Output, **Open Drain**, Output level High, No pull, ใช้ร่วมทั้ง 4 แผง

| คอลัมน์ | A | B | C | D | E | F | G | H |
|---|---|---|---|---|---|---|---|---|
| ขา | PD0 | PD1 | PD14 | PD3 | PD4 | PD5 | PD6 | PD7 |
| Label | `COL0` | `COL1` | `COL2` | `COL3` | `COL4` | `COL5` | `COL6` | `COL7` |
| ตำแหน่ง | CN9-25 | CN9-27 | CN7-16 | CN9-10 | CN9-8 | CN9-6 | CN9-4 | CN9-2 |

**แถว** — GPIO_Input, pull-up 10k ภายนอกไป **3.3V** (หรือ Pull-up ภายใน)

| แผง | แถว 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| 0 เรือ P1 | PF0 `ROW0`<br>CN9-21 | PF1 `ROW1`<br>CN9-19 | PF2 `ROW2`<br>CN9-17 | PF3 `ROW3`<br>CN9-7 | PF4 `ROW4`<br>CN10-11 | PF5 `ROW5`<br>CN9-9 | PE15 `ROW6`<br>CN10-30 |
| 1 โจมตี P1 | PF7 `ROW7`<br>CN9-26 | PF8 `ROW8`<br>CN9-24 | PF9 `ROW9`<br>CN9-28 | PF10 `ROW10`<br>CN9-11 | PB10 `ROW11`<br>CN10-32 | PF12 `ROW12`<br>CN7-20 | PF13 `ROW13`<br>CN10-2 |
| 2 เรือ P2 | PF14 `ROW14`<br>CN10-8 | PF15 `ROW15`<br>CN10-12 | PG0 `ROW16`<br>CN9-29 | PG1 `ROW17`<br>CN9-30 | PG2 `ROW18`<br>CN8-14 | PG3 `ROW19`<br>CN8-16 | PB11 `ROW20`<br>CN10-34 |
| 3 โจมตี P2 | PD11 `ROW21`<br>CN10-23 | PD12 `ROW22`<br>CN10-21 | PG9 `ROW23`<br>CN10-16 | PD13 `ROW24`<br>CN10-19 | PB2 `ROW25`<br>CN10-15 | PG14 `ROW26`<br>CN10-14 | PB6 `ROW27`<br>CN10-13 |

## 3. จอ TFT ILI9341 x2 — SPI1 ใช้ร่วม (9 ขา)

| ขา | ตำแหน่งบนบอร์ด | Label | โหมด | ไปที่ |
|---|---|---|---|---|
| PA5 | **CN7-10** (D13) | – | SPI1_SCK | SCK of both TFTs |
| PA6 | **CN7-12** (D12) | – | SPI1_MISO | SDO(MISO) of both TFTs (optional) |
| PB5 | **CN7-13** (D22) | – | SPI1_MOSI | SDI(MOSI) of both TFTs |
| PC8 | **CN8-2** (D43) | `CS` | Output, High | TFT P1 CS |
| PC9 | **CN8-4** (D44) | `DC` | Output | TFT P1 DC |
| PC10 | **CN8-6** (D45) | `RST` | Output, High | TFT P1 RESET |
| PC11 | **CN8-8** (D46) | `CS2` | Output, High | TFT P2 CS |
| PC12 | **CN8-10** (D47) | `DC2` | Output | TFT P2 DC |
| PD2 | **CN8-12** (D48) | `RST2` | Output, High | TFT P2 RESET |

SPI1: Transmit Only Master, 8 bit, CPOL Low, CPHA 1 Edge, Prescaler 4 (27 Mbit/s) · DMA2 Stream 5

## 4. จอสัมผัส XPT2046 x2 — bit-bang (7 ขา)

T_CLK/T_DIN/T_DO ใช้ร่วมสองจอ แยก T_CS และ T_IRQ (XPT2046 ปล่อย DOUT เป็น High-Z เมื่อ CS = High)

| ขา | ตำแหน่งบนบอร์ด | Label | โหมด | ไปที่ |
|---|---|---|---|---|
| PE3 | **CN9-22** (D60) | `T_CLK` | Output, Very High | T_CLK of both panels |
| PE5 | **CN9-18** (D58) | `T_MOSI` | Output, Very High | T_DIN of both panels |
| PE4 | **CN9-16** (D57) | `T_MISO` | Input | T_DO of both panels |
| PE6 | **CN9-20** (D59) | `T_CS` | Output, High | T_CS P1 |
| PE7 | **CN10-20** (D41) | `T_CS2` | Output, High | T_CS P2 |
| PE2 | **CN9-14** (D56) | `T_IRQ` | EXTI2 falling, pull-up | T_IRQ P1 |
| PE8 | **CN10-18** (D42) | `T_IRQ2` | EXTI8 falling, pull-up | T_IRQ P2 |

⚠ PE2 มีสองตำแหน่ง (CN9-14 และ CN10-25 เป็นขาเดียวกัน) — ต่อที่ CN9-14 และ**ห้ามต่ออะไรที่ CN10-25**

## 5. LCD1602 x2 — I2C1 ใช้ร่วม (2 ขา)

| ขา | ตำแหน่งบนบอร์ด | Label | โหมด | ไปที่ |
|---|---|---|---|---|
| PB8 | **CN7-2** (D15) | – | I2C1_SCL | SCL of both LCD backpacks |
| PB9 | **CN7-4** (D14) | – | I2C1_SDA | SDA of both LCD backpacks |

I2C1 Standard 100 kHz · จอ P1 address **0x27**, จอ P2 **0x26** (บัดกรีจัมเปอร์ A0 บนโมดูลตัวที่สอง)

## 6. เสียง + สวิตช์ประจำฝั่ง — TIM3 PWM + EXTI (6 ขา)

| ขา | ตำแหน่งบนบอร์ด | Label | โหมด | ไปที่ |
|---|---|---|---|---|
| PC6 | **CN7-1** (D16) | – | TIM3_CH1 (AF) | 1k -> 2N2222 base -> buzzer P1 |
| PC7 | **CN7-11** (D21) | – | TIM3_CH2 (AF) | 1k -> 2N2222 base -> buzzer P2 |
| PE0 | **CN10-33** (D34) | `MUTE1` | EXTI0 rising+falling, pull-up | mute slide switch P1 (other leg GND) |
| PB1 | **CN10-7** (A6) | `MUTE2` | EXTI1 rising+falling, pull-up | mute slide switch P2 (other leg GND) |
| PE10 | **CN10-24** (D40) | `SURR1` | EXTI10 falling, pull-up | surrender button P1 (other leg GND) |
| PE12 | **CN10-26** (D39) | `SURR2` | EXTI12 falling, pull-up | surrender button P2 (other leg GND) |

ยอมแพ้: **EXTI ISR** เช็กว่ายังกดอยู่ + debounce 300 ms → จบเกมทันทีในอินเทอร์รัปต์ (มีผลเฉพาะ S3/S4)
ปิดเสียง: **EXTI ISR** ทั้งขอบขึ้นและลง → ตั้ง CCR ของ buzzer ฝั่งนั้นเป็น 0 ทันที

EXTI ที่ใช้: 0 (PE0), 1 (PB1), 2 (PE2), 8 (PE8), 10 (PE10), 12 (PE12) — ไม่ซ้ำกัน
NVIC: เปิด EXTI0, EXTI1, EXTI2, EXTI9_5, EXTI15_10

## 7. Peripheral ที่ไม่มีขา

| Peripheral | ตั้งค่า |
|---|---|
| TIM6 | PSC 107, ARR 199 (200 µs), NVIC on |
| DMA2 | TIM1_CH1..CH4: **Memory→Peripheral**, Normal, Half Word/Half Word — Stream 1, 2, 6, 4 |
| DMA2 | SPI1_TX: Memory→Peripheral, Normal, **Byte/Byte** — **Stream 5** (Channel 3), NVIC on |
| Clock | SYSCLK 216 MHz, APB2 Timer 216 MHz, APB1 Timer 108 MHz |
| SYS | Debug: **Serial Wire** |

## 8. ขาไฟเลี้ยงบน Zio

| Header | ขา | |
|---|---|---|
| CN8 | 7 | +3.3V (จอ TFT, pull-up แถวสวิตช์) |
| CN8 | 9 | +5V (ออกจากบอร์ด) |
| CN8 | 11, 13 · CN9 23 · CN7 8 · CN10 5, 17, 27 | GND |

| จาก | ไป |
|---|---|
| PSU +5V | แถบไฟทั้ง 4 แผง, Vcc 74AHCT125, buzzer (ผ่าน 100Ω), LCD1602 ×2 |
| Nucleo 3.3V (CN8-7) | จอ TFT ×2 (VCC, LED), pull-up 10k ของแถวสวิตช์ 28 ตัว |
| PSU GND | GND ทุกอย่าง + GND Nucleo (CN8-11) |
| Nucleo | ระหว่างพัฒนา: USB · ใช้ PSU เลี้ยงบอร์ด: E5V อยู่บน morpho CN11-6 ซึ่งไม่ได้บัดกรีมา ต้องบัดกรีขาเพิ่ม (JP3 = E5V) |

> ⚠ pull-up ของแถวสวิตช์ต่อ **3.3V** · LCD1602 I2C มี pull-up ไป 5V บน SDA/SCL — PB8/PB9 ทน 5V (FT)

## 9. ขาที่ห้ามใช้ (บอร์ดจองไว้)

PA1 PA2 PA7 PB13 PC1 PC4 PC5 PG11 PG13 (Ethernet) · PA8–PA12 PG6 PG7 (USB) ·
PA13 PA14 PB3 (SWD/SWO) · PD8 PD9 (ST-LINK UART) · PB0 PB7 PB14 (LED) · PC13 (ปุ่ม User) ·
PH0 PH1 PC14 PC15 (clock)

## 10. ขาว่างบน Zio (สำรอง)

PA0 (CN10-29) · PA3 (CN9-1) · PA4 (CN7-17) · PA15 (CN7-9) · PB4 (CN7-19) · PB12 (CN7-7) · PB15 (CN7-3) ·
PC0 (CN9-3) · PC2 (CN10-9) · PC3 (CN9-5) · PD15 (CN7-18)
