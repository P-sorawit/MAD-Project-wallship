# Firmware โครงสร้างโมดูล (STM32 Nucleo-144 F767ZI)

โครงไฟล์สำหรับเกม Battle Ship บนบอร์ด ใช้ AI/กติกาจาก `../core/` (`wallship_ai.h`, `wallship_game_rules.h`) โดยตรง
หลักการ: **ISR แค่เก็บอินพุต → `App_Loop()` เรียกตรรกะเกม → ไฟอ่านสถานะเกมไปแสดง**
ไฟล์ที่ไม่ใช้ HAL ทดสอบบนคอมได้ (`host/`) ไฟล์ที่ใช้ HAL ต้องคอมไพล์ใน STM32CubeIDE

```
ISR: TIM6 ──► keypad (คิวปุ่ม)                       EXTI mute ──► buzzer
                    │
App_Loop ─► Game_OnKey / App_OnButton / Game_Tick   (game.c = FSM S1..S6)
                    │  (dirty)
                    └─► led_view ─► ws2812 ─► DMA2/TIM1 CH1-4
```

## ไฟล์แต่ละโมดูล

```
firmware/
├─ app/        app.c app.h app_config.h     ตัวเชื่อม + ค่าตั้งฮาร์ดแวร์ทั้งหมด
├─ 1_switch/   keypad.c keypad_core.c keypad.h
├─ 2_led/      ws2812.c/.h ws2812_port_stm32.c led_view.c/.h ws2812_test.c/.h
├─ 3_game/     game.c/.h placement.c/.h
├─ 6_ai/       ai_player.c/.h               (+ ../core/wallship_ai.h)
├─ sound/      buzzer.c/.h
└─ host/       เทสต์บนคอม (ไม่ต้องใส่ใน CubeIDE)
```

| # | โมดูล | HAL-free (เทสต์บนคอมได้) | ใช้ HAL |
|---|---|---|---|
| 1 | รับค่าสวิตช์ → `{panel, x, y}` | `keypad_core.c` (debounce + คิว) | `keypad.c` (สแกนคอลัมน์ใน TIM6) |
| 2 | ควบคุมไฟ | `ws2812.c` (buffer สี + เข้ารหัส CCR 86/173), `led_view.c` (สถานะ → สี) | `ws2812_port_stm32.c` (DMA) |
| 3 | ตัวเกมหลัก (state) | `game.c` (S1–S6), `placement.c` (กติกาวางเรือ) | – |
| 6 | AI | `ai_player.c` (ห่อ `wallship_ai.h`) | – |
| – | เสียง | – | `buzzer.c` (TIM3 CH1/CH2) |
| – | ตัวเชื่อม | – | `app.c` (loop + HAL callbacks) |

ค่าที่ต้องปรับตามสายจริงอยู่ที่เดียวคือ [app/app_config.h](app/app_config.h) (มี `TODO` กำกับ)

## API หลัก

```c
/* 1 สวิตช์ */  bool Keypad_GetEvent(KeyEvent *e);           // e->panel 0..3, x 0..7 (A-H), y 0..6 (1-7)
/* 2 ไฟ */     Ws2812_SetPixel(panel, x, y, rgb); Ws2812_SetPanel(panel, rgb); Ws2812_SetAll(rgb);
              bool Ws2812_Show(void);                      // reload: เข้ารหัส + ส่ง DMA (false = ยังส่งเฟรมก่อนไม่เสร็จ)
/* 3 เกม */    Game_OnKey(g, panel, x, y); Game_OnButton(g, player, BTN_...); Game_Tick(g, ms);
/* ปุ่ม */     App_OnButton(player, BTN_...);               // อินพุตปุ่มคำสั่งจากอุปกรณ์ใดก็ได้
/* 6 AI */     AiPlayer_Setup(agent, seed, profile); AiPlayer_TakeShot(...)
```

แผง: `0` เรือ P1, `1` โจมตี P1, `2` เรือ P2, `3` โจมตี P2

## ⚠ ปุ่มคำสั่งยังไม่มีอุปกรณ์รับ

ตัดโมดูล TFT/LCD ออกแล้ว แต่เกมยังต้องการปุ่มคำสั่งเหล่านี้ (`GameButton` ใน `game.h`):

| ปุ่ม | ใช้ตอน |
|---|---|
| `BTN_MODE_1P` / `BTN_MODE_2P` / `BTN_CONFIRM` | S1 เลือกโหมดและเริ่ม (P1 เท่านั้น) |
| `BTN_SHIP_1..3`, `BTN_CANCEL`, `BTN_CONFIRM` | S2 เลือกความยาวเรือ, ล้าง, ยืนยัน |
| `BTN_SURRENDER` | S3/S4 ยอมแพ้ |
| `BTN_CONFIRM` | S5 เริ่มเกมใหม่ |

อุปกรณ์ที่จะมาแทน (ปุ่มกดแยก, ใช้บางช่องของแผงสวิตช์ ฯลฯ) แค่เรียก `App_OnButton(player, BTN_...)` จาก main loop ถ้ายังไม่ต่อ เกมจะค้างที่ S1

## State (ตาม proposal 1.3.3)

| State | ไฟ |
|---|---|
| S1 เลือกโหมด | ดับ |
| S2 วางเรือ | แผงเรือ: น้ำเงิน/ขาว, ช่องที่กำลังวาง = ขาวหรี่ |
| S3 เริ่มเกม (2 วิ) | น้ำเงิน + เรือตัวเองขาว |
| S4 ต่อสู้ | พลาด=น้ำเงิน 15%, โดน=ส้ม, จม=แดง |
| S5 แพ้-ชนะ | เปิดเผยเรือทุกลำ |
| S6 รีเซ็ต (1 วิ) | ดับ → S1 |

กติกาวางเรือ: เลือกความยาว → กดช่องทีละช่อง ต้องว่าง ติดกัน และอยู่แนวเดียวกับสองช่องแรก (กันรูปตัว L) ต่อได้ทั้งสองปลาย, CANCEL ล้างทั้งกระดาน, CONFIRM ได้เมื่อครบ 6 ลำ
โหมด 1 คน: AI เป็น P2 วางเรือเองทันที และยิงทุก ~0.9 วินาที

## ตั้งค่า STM32CubeMX

| Peripheral | ค่า | ชื่อ pin (User Label) |
|---|---|---|
| TIM6 | update ทุก 200 µs (PSC 107, ARR 199 ที่ 108 MHz), เปิด NVIC | – |
| GPIO คอลัมน์ | Output **Open Drain**, no pull | `COL0`..`COL7` |
| GPIO แถว | Input (มี pull-up 10k ภายนอก) | `ROW0`..`ROW27` (0–6 = แผง 0, 7–13 = แผง 1, ...) |
| TIM1 | PSC 0, ARR 269, CH1–CH4 PWM | – |
| DMA2 | TIM1_CH1..CH4, Mem→Periph, Normal, **Half Word** ทั้งสองฝั่ง | – |
| TIM3 | counter 1 MHz (PSC 107), CH1/CH2 PWM | – |
| EXTI | rising + falling | `MUTE1` `MUTE2` (เลขขาห้ามซ้ำกัน) |

## นำไปใส่ใน CubeIDE

1. Generate โปรเจกต์จาก `.ioc` ตามตารางข้างบน
2. Include paths: `app`, `1_switch`, `2_led`, `3_game`, `6_ai`, `sound` และ `../core`
3. Source: ลากโฟลเดอร์ `firmware` เข้าโปรเจกต์ (Link/Copy) แล้ว **Exclude from build** โฟลเดอร์ `host/`
4. ใน `main.c` (เฉพาะช่อง USER CODE): `#include "app.h"`, `App_Init();` หลัง MX_..._Init, `App_Loop();` ใน `while (1)`
5. ถ้า HAL timebase เป็น TIMx (ไม่ใช่ SysTick) `main.c` จะมี `HAL_TIM_PeriodElapsedCallback` อยู่แล้ว ให้ย้ายบรรทัด `Keypad_ScanIsr()` ไปใส่ในนั้นแล้วลบตัวใน `app.c`
6. ถ้าเปิด D-Cache ไว้ `ws2812_port_stm32.c` จะ clean cache ก่อนสั่ง DMA ให้เอง

## ทดสอบไฟ WS2812 อย่างเดียวบนบอร์ด

ใช้ `2_led/ws2812_test.c` วนลายทดสอบ (แดง → เขียว → น้ำเงิน → ขาว → สีประจำ channel → ไฟวิ่งทีละดวง)

**วงจร** (เริ่มจากแผงเดียวหรือแถบสั้น ๆ ก่อน)
```
Nucleo TIM1_CH1 (เช่น PE9) ─► 74AHCT125 (ขา OE ต่อ GND) ─► 330Ω ─► DIN ของแถบไฟ
PSU 5V ─► 5V ของแถบไฟ + Vcc ของ 74AHCT125      (1000µF คร่อม 5V-GND ที่แถบ)
GND ของ PSU ─ GND ของแถบ ─ GND ของ Nucleo  ← ต้องต่อร่วมกันเสมอ
```

**CubeMX (โปรเจกต์ทดสอบแยก)**
1. Clock Configuration: ให้ **APB2 Timer clocks = 216 MHz** ถ้าไม่ใช่ ให้คำนวณใหม่: `ARR = f×1.25µs − 1`, `CCR_0 = f×0.40µs`, `CCR_1 = f×0.80µs` แล้วแก้ใน `app_config.h`
2. TIM1 → Channel1 = PWM Generation CH1, Prescaler 0, Counter Period 269, **Output compare preload = Enable**
3. DMA Settings ของ TIM1 → Add `TIM1_CH1`, Memory to Peripheral, Normal, Data width **Half Word / Half Word**
4. ตั้ง `WS2812_ACTIVE_MASK` ใน `app_config.h` เป็น `0x01U` (ใช้แค่ CH1) — channel ที่ไม่ได้ตั้ง DMA ห้ามเปิดใน mask

**ไฟล์ที่ใส่ในโปรเจกต์ทดสอบ:** `2_led/ws2812.c`, `ws2812_port_stm32.c`, `ws2812_test.c`, include path `2_led`, `app`, `../core` (ไม่ต้องใส่ `app.c`)

**main.c**
```c
/* USER CODE BEGIN Includes */
#include "ws2812_test.h"
void Ws2812_Port_PulseFinished(TIM_HandleTypeDef *htim);
/* USER CODE BEGIN 2 */
Ws2812Test_Init();
/* USER CODE BEGIN 3  (ใน while 1) */
Ws2812Test_Loop(HAL_GetTick());
/* USER CODE BEGIN 4 */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    Ws2812_Port_PulseFinished(htim);
}
```

**อ่านผล**

| เห็น | แปลว่า | แก้ |
|---|---|---|
| แดง/เขียวสลับกัน | ไฟล็อตนี้ใช้ลำดับ RGB | สลับ `put_byte(c.g)` กับ `c.r` ใน `ws2812.c` |
| ไม่ติดเลย | ไม่มีสัญญาณ/ไฟเลี้ยง | เช็ก GND ร่วม, OE ของ 74AHCT125 ต่อ GND, ใส่ breakpoint ดูว่า `HAL_TIM_PWM_Start_DMA` คืน `HAL_OK` |
| ติดแค่เฟรมแรกแล้วค้าง | callback ไม่ถูกเรียก (`Ws2812_IsBusy()` ค้าง true) | เช็กว่า DMA stream IRQ เปิดใน NVIC และมี callback ใน main.c |
| สีมั่ว/กระพริบ | timing ผิด | เช็ก clock 216 MHz และ preload; ถ้ามี logic analyzer: คาบ 1.25 µs, high 0.4/0.8 µs |
| ไฟวิ่งผิดลำดับ | ลำดับสายในแผงต่างจากที่คิด | ปรับ `WS2812_SERPENTINE`, `WS2812_FLIP_X/Y` |
| สีประจำ channel ผิดแผง | สาย CH ↔ แผง สลับ | ปรับ `WS2812_CHANNEL_TO_PANEL` |

ผ่านแผงแรกแล้วค่อยเพิ่ม CH2–CH4 (Add DMA ทีละ channel แล้วขยาย mask เป็น `0x0FU`)

## ทดสอบบนคอมพิวเตอร์

```powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
.\firmware\host\run_host_tests.ps1
```

ทดสอบ: กติกาวางเรือ (ตัว L, ทับ, ช่องว่าง, ต่อปลายกลับด้าน, CANCEL), debounce/mapping สวิตช์, การเข้ารหัส WS2812 + เพดานความสว่าง 40%, เกม PvE ครบ S1→S6→S1, PvP (ยิงผิดตา, ยิงซ้ำ, ยอมแพ้)

## ยังต้องทำ/ตรวจกับฮาร์ดแวร์จริง

- อุปกรณ์รับปุ่มคำสั่ง (ดูหัวข้อ ⚠ ข้างบน)
- ลำดับแผงกับสาย (`KEYPAD_GROUP_TO_PANEL`, `WS2812_CHANNEL_TO_PANEL`), การกลับด้าน (`*_FLIP_X/Y`), ไฟเดินแบบงู (`WS2812_SERPENTINE`)
- ขั้วสวิตช์ mute (`buzzer.c`, `app.c` ถือว่า LOW = ปิดเสียง)
- Buzzer สองตัวใช้ TIM3 ตัวเดียว → ARR ร่วมกัน เล่นได้ทีละเพลง ถ้าอยากให้เสียงต่างกันพร้อมกันต้องแยก timer
