# แผนการต่อและเดินสายในตู้

ใช้คู่กับ [PINOUT.md](PINOUT.md) (ขาบนบอร์ด), [BRINGUP.md](BRINGUP.md) (วงจรของแต่ละอุปกรณ์ + ลำดับทดสอบ)
และภาพ [docs/wiring.html](../docs/wiring.html) (หัวข้อ "เดินสายในตู้" มีภาพตัดด้านข้างและผังพื้นเสากลาง)

> ⚠ ขนาดตู้อ่านจาก `3d_print/parts/*.stl` (แผ่นหนา 3 mm) **ตำแหน่งอุปกรณ์บนตู้เป็นข้อเสนอ**
> วัดของจริงก่อนเจาะและก่อนตัดสาย ถ้าย้ายอุปกรณ์ แก้ `HARNESS` ใน `tools/pinmap.py` แล้วรันใหม่
>
> ตารางในหัวข้อ 7 สร้างจาก `tools/pinmap.py` อย่าแก้ในไฟล์นี้ตรง ๆ (รันแล้วจะถูกเขียนทับ)

---

## 1. ตู้และตำแหน่งอุปกรณ์

| ชิ้น (STL) | ขนาด mm | ใช้ติด |
|---|---|---|
| `base` | 489 × 325 | พื้นรวม |
| `top` ×2 | 325 × 190 | ฝากล่องผู้เล่น: **แผงเรือ** + **จอ TFT** วางคู่กันตามแนวกว้าง |
| `side_front` ×2 | 325 × 92 | หน้ากล่อง หันหาผู้เล่น: **ปุ่มยอมแพ้**, **สวิตช์ปิดเสียง**, **buzzer** |
| `side_left/right` ×2 | 190 × 92 | ข้างกล่อง |
| `partition` ×2 | 325 × 279 | หน้าเสากลาง: **แผงโจมตี** + **LCD 16x2** |
| `partition_side` ×2 | 115 × 282 | ข้างเสา: H7 ไฟเข้า / H8 USB |
| `partition_top` | 115 × 325 | หลังคาเสา ทำเป็นฝาเปิดซ่อม |
| `separate*` | 187 × 23, 164 × 23 | ตะแกรงช่อง 8 × 7 (ช่องละ ~23.4 mm) → แผงสวิตช์ 1 แผง ≈ 187 × 164 |

**ภาพตัดด้านข้าง** (ผู้เล่นนั่งซ้าย-ขวา)

```
                              partition_top (เปิดซ่อม)
                             ┌───────────────┐
                    LCD P1 ▐ │               │ ▌ LCD P2
                             │               │
              แผงโจมตี P1 ██ │               │ ██ แผงโจมตี P2
                         ██ │   (โล่ง)      │ ██
                         ██ │               │ ██
          แผงเรือ P1 + TFT P1│               │แผงเรือ P2 + TFT P2
 P1 ► ┌═════════════════════┤               ├═════════════════════┐ ◄ P2
 ปุ่ม │ (ด้านหลังแผงเรือ)      H1             H2  (ด้านหลังแผงเรือ)      │ ปุ่ม
 buzz │                      ║ ▓ บอร์ดทั้งหมด ▓ ║                      │ buzz
      └──────────────────────┴───────────────┴──────────────────────┘
          กล่อง P1  190            เสากลาง 115           กล่อง P2  190
```

- แผงโจมตีและ LCD **ฝังในหน้าเสา** ด้านหลังของมันจึงอยู่ในเสาอยู่แล้ว ไม่ต้องเจาะรูเพิ่ม
- สายไปกล่องผู้เล่นออกทาง **H1 / H2** ช่องต่ำที่หน้าเสา ซึ่งอยู่ในกล่องผู้เล่น มองจากข้างนอกไม่เห็น
- ฝั่ง P2 วางแบบหมุน 180° ของฝั่ง P1 ผู้เล่นทั้งสองเห็นจอ TFT อยู่ด้านเดียวกันของมือ

## 2. บอร์ดบนพื้นเสากลาง

```
                       หน้าเสาฝั่ง P1   (H1 ช่วง x 150–210)
   ┌──────────────────────────────────────────────────────────────────────┐
   │                                              ┌─DSP─┐                  │
 H8│ ┌─────────────────────────┐  ┌───HUB───┐     └─────┘  ┌DRV┐  ┌BUS┐   │H7
USB│ │ NUCLEO-F767ZI            │  │ 28×10k   │              │   │  │5V │   │ไฟเข้า
   │ │ USB ST-LINK หันเข้า H8     │  │ IDC ×4   │     ┌─AUD─┐  └───┘  └───┘   │
   │ └─────────────────────────┘  └──────────┘     └─────┘                  │
   └──────────────────────────────────────────────────────────────────────┘
                       หน้าเสาฝั่ง P2   (H2 ช่วง x 150–210)
   x: 0      6 ─────────── 139   148 ──── 208   216–256   264–294 300–320   325
```

- **ฝั่งซ้าย = สัญญาณ** (Nucleo, HUB, DSP) · **ฝั่งขวา = กระแสสูง** (BUS, DRV) — กระแสไฟ LED หลายแอมป์ไม่ไหลผ่านแถวสัญญาณ
- ยึดบอร์ดบน standoff สูง 8–10 mm กันบัดกรีใต้บอร์ดแตะพื้น
- พื้นที่ระหว่างบอร์ดเว้นเป็น **ช่องเดินสาย**: ริมหน้าเสาฝั่ง P1 (สัญญาณ) และริมหน้าเสาฝั่ง P2 (ไฟเลี้ยง)

## 3. บอร์ดย่อย (perfboard)

บอร์ดเล็กคั่นกลางมีไว้ให้ **ถอดสายเป็นชุด** ได้ และให้ตัวต้านทาน/ชิปอยู่ในที่ที่เปลี่ยนได้ ไม่ต้องบัดกรีบนสายแพ

### HUB — ศูนย์รวมสวิตช์ matrix
- ตัวต้านทาน 10k จาก **ROW0–27 ไป 3.3V** 28 ตัว ([BRINGUP 1.3](BRINGUP.md))
- COL0–7 ต่อขนานไปทั้ง 4 ซ็อกเก็ต
- box header IDC 2×8 จำนวน 4 ตัว: **J0** แผงเรือ P1 · **J1** แผงโจมตี P1 · **J2** แผงเรือ P2 · **J3** แผงโจมตี P2
- ขาของทุกซ็อกเก็ต (สายแพเส้นที่ n = ขา n, ขีดแดง = ขา 1)

```
   box header มองจากด้านบน
   ┌──────────┤ร่องกุญแจ├──────────┐
   │ 1▪  3   5   7   9  11  13  15 │     1–8  = COL A–H
   │ 2   4   6   8  10  12  14  16 │     9–15 = ROW 1–7,  16 = GND
   └────────────────────────────────┘     ▪ = ขา 1 (ขีดแดงของสายแพ)
```

| ขา IDC | 1–8 | 9–15 | 16 |
|---|---|---|---|
| สัญญาณ | COL A–H (`COL0–7`) | ROW 1–7 ของแผงนั้น | GND |
| J0 | PD0 … PD7 | `ROW0–6` (PF0–PF5, PE15) | |
| J1 | (ขนาน) | `ROW7–13` (PF7–PF10, PB10, PF12, PF13) | |
| J2 | (ขนาน) | `ROW14–20` (PF14, PF15, PG0–PG3, PB11) | |
| J3 | (ขนาน) | `ROW21–27` (PD11, PD12, PG9, PD13, PB2, PG14, PB6) | |

- ไดโอด 1N4148 ต่อปุ่มอยู่บน**แผงสวิตช์** ไม่ได้อยู่บน HUB
- ส่ง 3.3V ต่อให้ DSP ผ่านสาย J1

### DRV — ไดรเวอร์ไฟ LED
- 74AHCT125 (มีซ็อกเก็ต) + 100nF ที่ขา 14, ขา OE ทั้ง 4 ลง GND ([BRINGUP 1.2](BRINGUP.md))
- ขาออก 1Y–4Y ผ่าน **330Ω** → หัว JST-XH 3 ขา (5V · DIN · GND) 4 หัว = L1–L4
- 5V เข้าทางขั้วสกรูจาก BUS (สาย P2, AWG18) แยกออก 4 ทาง
- **1000µF 16V** ใส่ที่ต้นแผงไฟแต่ละแผง (ปลายสาย L) ไม่ใช่บน DRV

### DSP — ศูนย์รวมจอ TFT
- รับ N3 (17 เส้นจาก Nucleo) + J1 (3.3V, GND)
- หัว 1×14 ตัวผู้ 2 หัว เรียงขา**ตรงกับโมดูลจอ**: `VCC GND CS RESET DC SDI SCK LED SDO T_CLK T_CS T_DIN T_DO T_IRQ` สายจึงต่อตรง 1:1
- SCK, SDI, SDO, T_CLK, T_DIN, T_DO ต่อขนานไปทั้ง 2 หัว · CS/DC/RESET/T_CS/T_IRQ แยกหัวละชุด
- ใส่ **33Ω อนุกรม** ที่ SCK และ SDI ใกล้ขาเข้า ลดสัญญาณสะท้อนบนสายยาว
- VCC และ LED ของจอต่อ 3.3V (จอละ ~60–80 mA, regulator 3.3V ของ Nucleo จ่ายได้)

### AUD — เสียง สวิตช์ข้าง และ LCD
- วงจร buzzer 2 ชุด ([BRINGUP 1.4](BRINGUP.md)): PC6/PC7 → 1k → 2N2222 → 100Ω → BUZ− · BUZ+ = 5V · **1N4148 คร่อม BUZ+/BUZ−** บนบอร์ดนี้
- MUTE1/SURR1/MUTE2/SURR2 ผ่านตรงไปหัว S1/S2 (ใช้ pull-up ในชิป) · ถ้ามีกดหลอกจากสายยาว ใส่ 100nF จากขาไป GND
- PB8/PB9 + 5V + GND ออกหัว JST-XH 4 ขา (สาย I1) ไป LCD ทั้งสองจอ
- 5V เข้าจาก BUS (สาย P3)

### BUS — ไฟ 5V
- H7 → สวิตช์เปิดปิด → **ฟิวส์ 10A** → terminal block แถว +5V 4 ช่อง / GND 4 ช่อง
- PSU: แนะนำ **adapter 5V 10A แบบอยู่นอกตู้** + ขั้ว XT60 หรือ GX16-2 ที่ H7 จะไม่มีไฟบ้านในตู้
  (ถ้าใช้ PSU กล่องโลหะในตู้ ส่วนไฟบ้านต้องมีฝาครอบขั้วและให้คนที่ทำเป็นต่อ)
- ไฟเข้า Nucleo: ระหว่างพัฒนาใช้ USB ทาง H8 · ประกอบจริงใช้สาย **P5 → E5V** (ต้องบัดกรีขาที่ morpho CN11-6 และย้าย JP3 ไป E5V) — **ห้ามต่อ P5 ตอนเสียบ USB**

## 4. รหัสสีสาย

| สี | ใช้กับ |
|---|---|
| แดง | +5V |
| ดำ | GND |
| ส้ม | +3.3V |
| เหลือง | DIN ไฟ LED (PE9/11/13/14) |
| ขาว | คอลัมน์สวิตช์ `COL`, SCL |
| เทา | แถวสวิตช์ `ROW` |
| น้ำเงิน | SPI จอ (SCK, MOSI, MISO) |
| เขียว | CS/DC/RST จอ, SDA |
| ม่วง | touch, PWM buzzer, BUZ− |
| น้ำตาล | สวิตช์ยอมแพ้ / ปิดเสียง |
| สายแพ | ขีดแดง = เส้นที่ 1 |

## 5. หลักการเดินสาย

1. **แยกสายไฟออกจากสายสัญญาณ** สาย L (ไฟ LED) และ P (ไฟเลี้ยง) เดินริมผนังฝั่ง P2/ฝั่ง BUS · สายแพ M, D เดินริมฝั่ง P1 ถ้าต้องข้ามกันให้ข้ามตั้งฉาก
2. **ทุกสายมี GND ของตัวเอง** (มีในตารางแล้ว) กระแสกลับจะได้ไม่ไปวิ่งในสายของอุปกรณ์อื่น
3. **SPI จอยาวรวม ~50 cm** (N3 15 + D1 35) ที่ 27 Mbit/s ถ้าภาพมีจุดเพี้ยน ตั้ง SPI1 Prescaler เป็น 8 (13.5 Mbit/s) ใน CubeMX
   ยังเร็วพอ: เต็มจอ ~91 ms, แบบส่งเฉพาะแถบที่เปลี่ยน ~25 ms
4. **สายแพ** พับมุม 45° แทนการบิด ยึดทุก 8–10 cm · เผื่อ **ห่วงสาย 5–10 cm** ตรงที่ต้องเปิดฝา (ฝากล่อง `top`, หลังคาเสา)
5. **ขอบรู H1/H2** พันเทปหรือใส่ยางกันบาด ขอบงานพิมพ์ 3D คม
6. **ทุกสายถอดได้ที่หัวต่อ** ยกฝากล่องหรือถอดแผงได้โดยไม่ต้องตัดสาย
7. **ติดป้ายปลายสายทั้งสองข้าง** เช่น `M2 · HUB-J1 → แผงโจมตี P1`
8. **ความยาวในตารางเป็นค่าประมาณ** (ระยะในตู้ + เผื่อ) วัดเส้นทางจริงด้วยเชือกก่อนตัด

## 6. ลำดับการประกอบ

ประกอบทีละชุด ทดสอบผ่านแล้วค่อยต่อชุดถัดไป (โค้ดทดสอบอยู่ใน [hwtests/](hwtests/README.md))

| ขั้น | ประกอบ | ทดสอบ |
|---|---|---|
| 1 | ยึด Nucleo, BUS · เจาะ H7/H8 · ต่อ P1, P4 (**ยังไม่ต่อ P5**) | มัลติมิเตอร์: BUS 5.0–5.2 V, GND ของ BUS ↔ CN8-11 บี๊บ |
| 2 | DRV + N2 + P2 + L1 → แผงเรือ P1 | `test_led.c` (`WS2812_ACTIVE_MASK 0x01`) |
| 3 | L2, L3, L4 | `test_led.c` (`0x0F`) |
| 4 | ก่อนติดตั้ง: ทดสอบตารางทีละแผงบนโต๊ะ · แล้วต่อ HUB + N1 + M1–M4 | `test_table.c` ทีละตาราง 7×8 → `test_switch.c` ตรวจรวม 224 ปุ่ม |
| 5 | AUD + N5 + P3 + S1, S2 | `test_buzzer.c`, `test_surrender.c` |
| 6 | I1 → LCD P1 → LCD P2 | `test_lcd.c` ต้องเจอ 0x27 และ 0x26 |
| 7 | DSP + J1 + N3 + D1, D2 | `test_tft.c` แล้ว `test_touch.c` (คาลิเบรต) |
| 8 | ปิดฝา · ถอด USB · ต่อ P5 + JP3 → E5V | `APP_HW_TEST 1` ทดสอบรวม แล้วเล่นจริง |

## 7. ช่องเจาะและตารางสาย

ชื่อปลายสาย: `NUC` Nucleo · `HUB` `DRV` `DSP` `AUD` `BUS` บอร์ดย่อย · `PANEL0–3` แผงเรือ P1, แผงโจมตี P1, แผงเรือ P2, แผงโจมตี P2 ·
`TFT1/2` `LCD1/2` จอ · `CTRL1/2` ปุ่มยอมแพ้ + สวิตช์ปิดเสียง + buzzer ของแต่ละฝั่ง

<!-- BEGIN generated by tools/pinmap.py -->
### ช่องเจาะ
| ช่อง | ตำแหน่ง | สายที่ผ่าน |
|---|---|---|
| **H1** | tower face toward P1, low (hidden inside the P1 box), slot 60 x 25 mm, x 150-210 / z 8-33 | M1, L1, D1, S1 |
| **H2** | tower face toward P2, low (hidden inside the P2 box), slot 60 x 25 mm, x 150-210 / z 8-33 | M3, L3, D2, S2 |
| **H3** | cut-out of the P1 attack panel in the tower face; the panel back is inside the tower, no extra hole | M2, L2 |
| **H4** | cut-out of the P2 attack panel in the tower face; the panel back is inside the tower, no extra hole | M4, L4 |
| **H5** | cut-out of LCD P1 in the tower face (window 72 x 25 mm); pins end inside the tower | I1 |
| **H6** | cut-out of LCD P2 in the tower face (window 72 x 25 mm); pins end inside the tower | I1 |
| **H7** | tower side wall at the power end: 5 V inlet (XT60 / GX16-2) + power switch | P1 |
| **H8** | tower side wall at the Nucleo end: ST-LINK USB access ~14 x 10 mm, z 8-20 | – |

### รายการสาย
| สาย | จาก → ไป | ยาว cm | เส้น | หัวต่อ ต้นทาง / ปลายทาง | ผ่าน |
|---|---|---|---|---|---|
| **N1** | NUC → HUB<br>Nucleo -> matrix hub | 15 | 38 × AWG26 | Dupont male x38 / header on HUB | – |
| **M1** | HUB → PANEL0<br>Hub -> P1 fleet panel switches | 30 | 16 × ribbon 16 | IDC 2x8 / IDC 2x8 | H1 |
| **M2** | HUB → PANEL1<br>Hub -> P1 attack panel switches | 30 | 16 × ribbon 16 | IDC 2x8 / IDC 2x8 | H3 |
| **M3** | HUB → PANEL2<br>Hub -> P2 fleet panel switches | 30 | 16 × ribbon 16 | IDC 2x8 / IDC 2x8 | H2 |
| **M4** | HUB → PANEL3<br>Hub -> P2 attack panel switches | 30 | 16 × ribbon 16 | IDC 2x8 / IDC 2x8 | H4 |
| **N2** | NUC → DRV<br>Nucleo -> LED driver | 15 | 5 × AWG26 | Dupont male x5 / JST-XH 5 | – |
| **L1** | DRV → PANEL0<br>LED driver -> P1 fleet LEDs | 35 | 3 × AWG20 (DIN AWG26) | JST-XH 3 / JST-XH 3 | H1 |
| **L2** | DRV → PANEL1<br>LED driver -> P1 attack LEDs | 35 | 3 × AWG20 (DIN AWG26) | JST-XH 3 / JST-XH 3 | H3 |
| **L3** | DRV → PANEL2<br>LED driver -> P2 fleet LEDs | 35 | 3 × AWG20 (DIN AWG26) | JST-XH 3 / JST-XH 3 | H2 |
| **L4** | DRV → PANEL3<br>LED driver -> P2 attack LEDs | 35 | 3 × AWG20 (DIN AWG26) | JST-XH 3 / JST-XH 3 | H4 |
| **N3** | NUC → DSP<br>Nucleo -> display hub | 15 | 17 × AWG26 | Dupont male x17 / header on DSP | – |
| **J1** | HUB → DSP<br>Hub -> display hub (3.3 V share) | 10 | 2 × AWG24 | JST-XH 2 / JST-XH 2 | – |
| **D1** | DSP → TFT1<br>Display hub -> TFT P1 | 35 | 14 × ribbon 14 | header 1x14 / Dupont female 1x14 (module order) | H1 |
| **D2** | DSP → TFT2<br>Display hub -> TFT P2 | 35 | 14 × ribbon 14 | header 1x14 / Dupont female 1x14 (module order) | H2 |
| **N5** | NUC → AUD<br>Nucleo -> audio/switch/LCD board | 15 | 9 × AWG26 | Dupont male x9 / header on AUD | – |
| **I1** | AUD → LCD1 > LCD2<br>AUD -> LCD P1 -> LCD P2 (I2C daisy chain, 45 + 30 cm) | 75 | 4 × AWG24 | JST-XH 4 / Dupont female 1x4 at each LCD | H5, H6 |
| **S1** | AUD → CTRL1<br>Audio board -> P1 controls (buzzer, mute, surrender) | 40 | 5 × AWG24 | JST-XH 5 / solder + heatshrink | H1 |
| **S2** | AUD → CTRL2<br>Audio board -> P2 controls | 40 | 5 × AWG24 | JST-XH 5 / solder + heatshrink | H2 |
| **P1** | PSU → BUS<br>PSU -> 5 V bus | 15 | 2 × AWG18 | XT60 / GX16-2 / terminal block | H7 |
| **P2** | BUS → DRV<br>5 V bus -> LED driver (feeds L1-L4) | 10 | 2 × AWG18 | terminal block / screw terminal | – |
| **P3** | BUS → AUD<br>5 V bus -> audio board + LCDs | 10 | 2 × AWG22 | terminal block / JST-XH 2 | – |
| **P4** | BUS → NUC<br>Bus GND -> Nucleo GND (star ground) | 10 | 1 × AWG22 | terminal block / Dupont male | – |
| **P5** | BUS → NUC<br>5 V bus -> Nucleo E5V (final build only, JP3 = E5V) | 25 | 1 × AWG22 | terminal block / pin soldered into morpho CN11-6 | – |

#### N1 · Nucleo -> matrix hub
| # | สัญญาณ | Nucleo | สี |
|---|---|---|---|
| 1 | COL0 | **CN9-25** | white |
| 2 | COL1 | **CN9-27** | white |
| 3 | COL2 | **CN7-16** | white |
| 4 | COL3 | **CN9-10** | white |
| 5 | COL4 | **CN9-8** | white |
| 6 | COL5 | **CN9-6** | white |
| 7 | COL6 | **CN9-4** | white |
| 8 | COL7 | **CN9-2** | white |
| 9 | ROW0 | **CN9-21** | grey |
| 10 | ROW1 | **CN9-19** | grey |
| 11 | ROW2 | **CN9-17** | grey |
| 12 | ROW3 | **CN9-7** | grey |
| 13 | ROW4 | **CN10-11** | grey |
| 14 | ROW5 | **CN9-9** | grey |
| 15 | ROW6 | **CN10-30** | grey |
| 16 | ROW7 | **CN9-26** | grey |
| 17 | ROW8 | **CN9-24** | grey |
| 18 | ROW9 | **CN9-28** | grey |
| 19 | ROW10 | **CN9-11** | grey |
| 20 | ROW11 | **CN10-32** | grey |
| 21 | ROW12 | **CN7-20** | grey |
| 22 | ROW13 | **CN10-2** | grey |
| 23 | ROW14 | **CN10-8** | grey |
| 24 | ROW15 | **CN10-12** | grey |
| 25 | ROW16 | **CN9-29** | grey |
| 26 | ROW17 | **CN9-30** | grey |
| 27 | ROW18 | **CN8-14** | grey |
| 28 | ROW19 | **CN8-16** | grey |
| 29 | ROW20 | **CN10-34** | grey |
| 30 | ROW21 | **CN10-23** | grey |
| 31 | ROW22 | **CN10-21** | grey |
| 32 | ROW23 | **CN10-16** | grey |
| 33 | ROW24 | **CN10-19** | grey |
| 34 | ROW25 | **CN10-15** | grey |
| 35 | ROW26 | **CN10-14** | grey |
| 36 | ROW27 | **CN10-13** | grey |
| 37 | 3V3 | **CN8-7** | orange |
| 38 | GND | **CN9-12** | black |

#### M1 · Hub -> P1 fleet panel switches
| # | สัญญาณ | สี |
|---|---|---|
| 1 | COL A | ribbon |
| 2 | COL B | ribbon |
| 3 | COL C | ribbon |
| 4 | COL D | ribbon |
| 5 | COL E | ribbon |
| 6 | COL F | ribbon |
| 7 | COL G | ribbon |
| 8 | COL H | ribbon |
| 9 | ROW 1 | ribbon |
| 10 | ROW 2 | ribbon |
| 11 | ROW 3 | ribbon |
| 12 | ROW 4 | ribbon |
| 13 | ROW 5 | ribbon |
| 14 | ROW 6 | ribbon |
| 15 | ROW 7 | ribbon |
| 16 | GND | ribbon |

#### M2 · Hub -> P1 attack panel switches
ลำดับสายเหมือน M1

#### M3 · Hub -> P2 fleet panel switches
ลำดับสายเหมือน M1

#### M4 · Hub -> P2 attack panel switches
ลำดับสายเหมือน M1

#### N2 · Nucleo -> LED driver
| # | สัญญาณ | Nucleo | สี |
|---|---|---|---|
| 1 | PE9 | **CN10-4** | yellow |
| 2 | PE11 | **CN10-6** | yellow |
| 3 | PE13 | **CN10-10** | yellow |
| 4 | PE14 | **CN10-28** | yellow |
| 5 | GND | **CN10-22** | black |

#### N3 · Nucleo -> display hub
| # | สัญญาณ | Nucleo | สี |
|---|---|---|---|
| 1 | PA5 | **CN7-10** | blue |
| 2 | PB5 | **CN7-13** | blue |
| 3 | PA6 | **CN7-12** | blue |
| 4 | CS | **CN8-2** | green |
| 5 | DC | **CN8-4** | green |
| 6 | RST | **CN8-6** | green |
| 7 | CS2 | **CN8-8** | green |
| 8 | DC2 | **CN8-10** | green |
| 9 | RST2 | **CN8-12** | green |
| 10 | T_CLK | **CN9-22** | purple |
| 11 | T_MOSI | **CN9-18** | purple |
| 12 | T_MISO | **CN9-16** | purple |
| 13 | T_CS | **CN9-20** | purple |
| 14 | T_CS2 | **CN10-20** | purple |
| 15 | T_IRQ | **CN9-14** | purple |
| 16 | T_IRQ2 | **CN10-18** | purple |
| 17 | GND | **CN7-8** | black |

#### D1 · Display hub -> TFT P1
| # | สัญญาณ | สี |
|---|---|---|
| 1 | VCC 3V3 | ribbon |
| 2 | GND | ribbon |
| 3 | CS | ribbon |
| 4 | RESET | ribbon |
| 5 | DC | ribbon |
| 6 | SDI(MOSI) | ribbon |
| 7 | SCK | ribbon |
| 8 | LED 3V3 | ribbon |
| 9 | SDO(MISO) | ribbon |
| 10 | T_CLK | ribbon |
| 11 | T_CS | ribbon |
| 12 | T_DIN | ribbon |
| 13 | T_DO | ribbon |
| 14 | T_IRQ | ribbon |

#### D2 · Display hub -> TFT P2
ลำดับสายเหมือน D1

#### N5 · Nucleo -> audio/switch/LCD board
| # | สัญญาณ | Nucleo | สี |
|---|---|---|---|
| 1 | PC6 | **CN7-1** | purple |
| 2 | PC7 | **CN7-11** | purple |
| 3 | MUTE1 | **CN10-33** | brown |
| 4 | SURR1 | **CN10-24** | brown |
| 5 | MUTE2 | **CN10-7** | brown |
| 6 | SURR2 | **CN10-26** | brown |
| 7 | PB8 | **CN7-2** | white |
| 8 | PB9 | **CN7-4** | green |
| 9 | GND | **CN10-27** | black |

#### I1 · AUD -> LCD P1 -> LCD P2 (I2C daisy chain, 45 + 30 cm)
| # | สัญญาณ | สี |
|---|---|---|
| 1 | GND | black |
| 2 | 5V | red |
| 3 | SDA | green |
| 4 | SCL | white |

#### S1 · Audio board -> P1 controls (buzzer, mute, surrender)
| # | สัญญาณ | สี |
|---|---|---|
| 1 | BUZ+ 5V | red |
| 2 | BUZ- (2N2222 C) | purple |
| 3 | MUTE1 | brown |
| 4 | SURR1 | brown |
| 5 | GND | black |

#### S2 · Audio board -> P2 controls
| # | สัญญาณ | สี |
|---|---|---|
| 1 | BUZ+ 5V | red |
| 2 | BUZ- (2N2222 C) | purple |
| 3 | MUTE2 | brown |
| 4 | SURR2 | brown |
| 5 | GND | black |
<!-- END generated -->

## 8. ของที่ต้องใช้เดินสาย (ประมาณ)

| ของ | จำนวน |
|---|---|
| สายแพ 16 เส้น + หัว IDC 2×8 ตัวเมีย | ~1.4 m, 8 หัว |
| box header IDC 2×8 | 8 (4 บน HUB, 4 บนแผงสวิตช์) |
| สายแพ 14 เส้น + housing Dupont 1×14 | ~0.8 m, 2 หัว (ฝั่งจอ) + header 1×14 ตัวผู้ 2 (ฝั่ง DSP) |
| หัว Dupont ตัวผู้ (เสียบ header ตัวเมียของ Nucleo) | ~71 (N1 38, N2 5, N3 17, N5 9, P4 1 + สำรอง) |
| JST-XH คู่ผู้-เมีย | 3 ขา ×8, 5 ขา ×3, 4 ขา ×1, 2 ขา ×2 |
| สาย AWG18 แดง/ดำ | ~0.5 m ต่อสี |
| สาย AWG20 แดง/ดำ | ~1.5 m ต่อสี (ไฟ LED) |
| สาย AWG24–26 สีตามหัวข้อ 4 | ~2 m ต่อสี |
| terminal block 2×4, กล่องฟิวส์ + ฟิวส์ 10A, สวิตช์, ขั้ว XT60/GX16 | 1 ชุด |
| ยางกันบาด/เทปขอบรู, เคเบิลไทร์, ป้ายสาย, ท่อหด, standoff M3 | ตามใช้ |
| อุปกรณ์บนบอร์ดย่อย | 10k ×28, 74AHCT125 + ซ็อกเก็ต, 330Ω ×4, 33Ω ×2, 100nF ×3, 1000µF 16V ×4, 2N2222 ×2, 1k ×2, 100Ω ×2, 1N4148 ×2, perfboard ×4 |
