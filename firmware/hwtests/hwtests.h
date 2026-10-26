/* Stand-alone hardware tests, one device per file (see hwtests/README.md).
 * Tests marked "no CubeMX" set up their own pins, use no interrupt and define
 * no HAL callback, so any number of them can be in the build together. Each
 * of the others defines the HAL callbacks it needs: put only ONE of those in
 * the CubeIDE build. Then in main.c:
 *
 *   USER CODE BEGIN Includes : #include "hwtests.h"
 *   USER CODE BEGIN 2        : TestLed_Init();      (the test you added)
 *   USER CODE BEGIN 3        : TestLed_Loop();      (inside while (1))
 *
 * Results are shown on the device itself where possible, and always in
 * variables named test_* that you can watch in Debug > Live Expressions.
 */
#ifndef HWTESTS_H
#define HWTESTS_H

void TestLed_Init(void);        void TestLed_Loop(void);        /* test_led.c */
void TestWs_Init(void);         void TestWs_Loop(void);         /* test_ws2812b.c (no driver files) */
void TestSwitch_Init(void);     void TestSwitch_Loop(void);     /* test_switch.c */
void TestTable_Init(void);      void TestTable_Loop(void);      /* test_table.c (4 tables, keys + LEDs) */
void TestBuzzer_Init(void);     void TestBuzzer_Loop(void);     /* test_buzzer.c (+ mute) */
void TestSurrender_Init(void);  void TestSurrender_Loop(void);  /* test_surrender.c */
void TestSide_Init(void);       void TestSide_Loop(void);       /* test_side.c (surrender + mute + buzzers, no CubeMX) */
void TestLcd_Init(void);        void TestLcd_Loop(void);        /* test_lcd.c */
void TestLcd16x2_Init(void);    void TestLcd16x2_Loop(void);    /* test_lcd16x2.c (2 LCDs, no CubeMX) */
void TestTft_Init(void);        void TestTft_Loop(void);        /* test_tft.c */
void TestTouch_Init(void);      void TestTouch_Loop(void);      /* test_touch.c (+ TFT) */
void TestTftTouch_Init(void);   void TestTftTouch_Loop(void);   /* test_tft_touch.c (2 screens + touch, no CubeMX) */
void TestPanelArt_Init(void);   void TestPanelArt_Loop(void);   /* test_panel_art.c (pixel art + keys) */

#endif
