# คู่มืออ่านโค้ด firmware

อ่านไฟล์นี้ก่อนเปิดโค้ด จะรู้ว่าแต่ละไฟล์ทำอะไร อ่านอะไรก่อน และถ้าจะแก้เรื่องหนึ่งต้องไปไฟล์ไหน

## 1. ภาพรวมใน 1 นาที

```
main.c (CubeMX)
  └─ App_Init() / App_Loop()            app/app.c
        │
        ├─ read_inputs()   ── แผงสวิตช์ ──► Game_OnKey()        1_switch → 3_game
        │                  ── แตะจอ ─────► Game_OnButton()     4_tft/touch → 3_game
        ├─ update_game()   ── เวลา ──────► Game_Tick()         (S3→S4, AI ยิง, S6→S1)
        └─ draw_outputs()  ── อ่าน Game ──► ไฟ / LCD / จอ TFT   2_led, 5_lcd, 4_tft

interrupt:  TIM6 → สแกนสวิตช์   ·  DMA → ส่งไฟ/ภาพเสร็จ  ·  EXTI → ยอมแพ้ / ปิดเสียง / แตะจอ
```

หลัก 3 ข้อที่ทั้งโค้ดยึด:
1. **ตัวเกม (`3_game`) ไม่รู้จักฮาร์ดแวร์** รับแค่ "กดช่อง", "กดปุ่ม", "เวลาผ่านไป"
2. **การแสดงผลอ่านสถานะเกมแล้ววาดใหม่** ไม่มีใครสั่ง "เปลี่ยนไฟดวงนี้" ตรง ๆ จากตัวเกม
3. **ไฟล์ที่ไม่มี `#include "main.h"` ไม่แตะ HAL** จึงเทสต์บนคอมได้ (`host/`)

## 2. ลำดับการอ่าน (แนะนำ)

| ลำดับ | ไฟล์ | อ่านเพื่อรู้ |
|---|---|---|
| 1 | `app/app.c` | วงจรหลักทั้งระบบ สั้น อ่านจบในไม่กี่นาที |
| 2 | `3_game/game.h` | state S1–S6, ปุ่มคำสั่ง, ข้อมูลที่เกมเก็บ |
| 3 | `3_game/game.c` | กติกาแต่ละ state ทำอะไรเมื่อกดอะไร |
| 4 | `3_game/placement.c` | กติกาวางเรือ (ติดกัน, ห้ามตัว L) |
| 5 | `../core/wallship_game_rules.h` | ตัดสิน HIT/MISS/SUNK, สลับตา |
| 6 | โมดูลที่ตัวเองรับผิดชอบ | ดูตารางข้อ 3 |

## 3. แต่ละโฟลเดอร์

| โฟลเดอร์ | ไฟล์ | ใช้ HAL | ทำอะไร |
|---|---|---|---|
| `app/` | `app.c` | ✔ | วงจรหลัก + HAL callback ทั้งหมด |
| | `app_config.h` | | **ค่าทุกอย่างที่ปรับตามฮาร์ดแวร์อยู่ที่นี่** (ลำดับแผง, ความสว่าง, address LCD, คาลิเบรต touch) |
| | `hwtest.c` | ✔ | โหมดทดสอบรวม (`APP_HW_TEST 1`) |
| `1_switch/` | `keypad.c` | ✔ | สแกนคอลัมน์ทุก 200 µs |
| | `keypad_core.c` | | กันเด้ง + คิวการกด |
| `2_led/` | `ws2812.c` | | buffer สี → ค่า CCR (บิตละ 86/173) |
| | `ws2812_port_stm32.c` | ✔ | สั่ง DMA ของ TIM1 |
| | `led_view.c` | | สถานะเกม → สีแต่ละช่อง |
| `3_game/` | `game.c`, `placement.c` | | ตัวเกม (ไม่มีฮาร์ดแวร์) |
| `4_tft/` | `tft_ui.c` | | เลือกหน้าจอ, วาด, จุดแตะ |
| | `gfx.c`, `art.c`, `font5x7.c` | | วาดรูป / ภาพ pixel art / ตัวอักษร ลง RAM |
| | `tft.c` | ✔ | ส่งภาพเข้าจอด้วย DMA |
| | `touch.c` | ✔ | อ่านจอสัมผัส |
| `5_lcd/` | `lcd_ui.c` | | ข้อความ 2 บรรทัดของแต่ละ state |
| | `lcd1602.c` | ✔ | ไดรเวอร์ I2C |
| `6_ai/` | `ai_player.c` | | ให้ AI ยิงผ่านกติกาเดียวกับคน |
| `sound/` | `buzzer.c` | ✔ | ทำนองเสียง + ปิดเสียง |
| `hwtests/` | `test_*.c` | ✔ | ทดสอบทีละอุปกรณ์ (ดู README ในโฟลเดอร์) |
| `host/` | | | เทสต์และวาดภาพหน้าจอบนคอม |
| `tools/` | `pinmap.py` | | ผังขาต้นทาง → ตรวจ + สร้าง `docs/wiring.html` |

## 4. อยากแก้เรื่องนี้ ไปที่ไหน

| อยากแก้ | ไปที่ |
|---|---|
| สีช่องบนแผงไฟ | `2_led/led_view.c` (ค่าสี `LED_*` บนสุดของไฟล์) |
| ความสว่างรวม | `app/app_config.h` → `WS2812_BRIGHTNESS_CAP` |
| ไฟเดินผิดลำดับ / แผงสลับ | `app_config.h` → `WS2812_*`, `KEYPAD_*` |
| ข้อความ LCD | `5_lcd/lcd_ui.c` → `LcdUi_Format()` |
| หน้าจอ TFT / ตำแหน่งปุ่ม | `4_tft/tft_ui.c` → `draw_*()` และ `TftUi_HitTest()` (ใช้ค่า layout ชุดเดียวกันด้านบนไฟล์) |
| เพิ่มภาพ pixel art | `4_tft/art.c` แล้วเรียกใน `tft_ui.c` |
| เสียง | `sound/buzzer.c` → ตาราง `sounds[]` (ความถี่ Hz, ระยะ ms) |
| เวลา GAME START / AI ยิง / RESET | `3_game/game.c` → `GAME_*_MS` บนสุดของไฟล์ |
| กติกาวางเรือ | `3_game/placement.c` |
| กติกายิง / ชนะ | `../core/wallship_game_rules.h` |
| ความเก่งของ AI | `app_config.h` → `GAME_AI_PROFILE` หรือ `../core/wallship_heuristic.h` |
| ขาที่ต่อ | แก้ตาราง `PINS` ใน `tools/pinmap.py` แล้วรัน (ตรวจซ้ำ/ชน + สร้าง `docs/wiring.html` ใหม่) จากนั้นแก้ `PINOUT.md` และ CubeMX ให้ตรง |

## 5. ชื่อในโค้ด

| รูปแบบ | ความหมาย | ตัวอย่าง |
|---|---|---|
| `Module_Function()` | ฟังก์ชันที่ไฟล์อื่นเรียกได้ | `Game_OnKey`, `Tft_SendBands` |
| `snake_case()` แบบ `static` | ใช้ในไฟล์เดียว | `draw_battle`, `read_inputs` |
| `..._isr` / `...Isr` | ทำงานใน interrupt ต้องสั้นและเร็ว | `surrender_isr`, `Keypad_ScanIsr` |
| `..._Port_...` | ส่วนที่ผูกกับ HAL ของโมดูลที่ไม่ใช้ HAL | `Ws2812_Port_Start` |
| `UPPER_CASE` | ค่าคงที่ / ค่าตั้ง | `GAME_AI_SHOT_DELAY_MS` |
| `test_*` | ตัวแปรให้ดูใน Live Expressions | `test_key_count` |
| ชื่อขา `COL0_Pin`, `RST2_Pin` | มาจาก **User Label** ใน CubeMX | ดู `PINOUT.md` |

## 6. ทดสอบบนคอมก่อนลงบอร์ด

```powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
powershell -NoProfile -ExecutionPolicy Bypass -File .\firmware\host\run_host_tests.ps1   # เทสต์เกม/จอ/LCD
powershell -NoProfile -ExecutionPolicy Bypass -File .\firmware\host\render_screens.ps1   # ภาพทุกหน้าจอ → build/screens
python .\firmware\tools\pinmap.py                                                       # ตรวจผังขา
```
