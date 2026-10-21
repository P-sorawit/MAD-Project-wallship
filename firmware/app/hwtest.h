/* Hardware bring-up test mode (no game). Enable with APP_HW_TEST 1 in
 * app_config.h; App_Init/App_Loop then run these instead of the game.
 *
 * Everything runs at once, each device shows its own result:
 *   LED      : test patterns (red, green, blue, white, channel colors, walk)
 *              until the first key press, then a key map: every panel has its
 *              own dim color, pressed cells toggle white, the last press is red
 *   Switches : press = LED toggles + click on that side's buzzer
 *   Buzzer   : start tune at boot, click per key, "sunk" tune per surrender
 *   Mute     : silences that side instantly; state shown on LCD/TFT
 *   Surrender: press counter shown on LCD/TFT
 *   LCD      : I2C scan at boot; each LCD shows its key count and switches
 *   TFT      : color bars, orientation marks, I2C scan result, last key,
 *              switch states and touch raw values + crosshair (calibration)
 * See firmware/BRINGUP.md for the step-by-step procedure.
 */
#ifndef HWTEST_H
#define HWTEST_H

#include <stdint.h>

void HwTest_Init(void);
void HwTest_Loop(void);

/* Called from app.c EXTI handlers (ISR context). */
void HwTest_OnSurrender(uint8_t player);
void HwTest_OnMute(uint8_t player);

#endif
