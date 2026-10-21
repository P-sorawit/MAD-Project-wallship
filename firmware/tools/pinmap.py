"""Single source of truth for the board wiring.

Run:  python firmware/tools/pinmap.py
  - checks every pin is on a Zio header (CN7..CN10), unique, not board-reserved,
    and that EXTI lines do not clash
  - writes firmware/tools/pinmap.json (used by the wiring infographic)

Zio table: UM1974 Rev 7, Table 13 (NUCLEO-F746ZG/F756ZG/F767ZI pin assignments).
The ST morpho headers CN11/CN12 are NOT soldered by default, so only Zio pins
are used.
"""
import json
import os
import sys

# connector -> {pin number: (signal, Zio name)}; power pins have no Zio name
ZIO = {
    "CN8": {1: ("NC", ""), 3: ("IOREF", ""), 5: ("RESET", ""), 7: ("+3.3V", ""),
            9: ("+5V", ""), 11: ("GND", ""), 13: ("GND", ""), 15: ("VIN", ""),
            2: ("PC8", "D43"), 4: ("PC9", "D44"), 6: ("PC10", "D45"), 8: ("PC11", "D46"),
            10: ("PC12", "D47"), 12: ("PD2", "D48"), 14: ("PG2", "D49"), 16: ("PG3", "D50")},
    "CN9": {1: ("PA3", "A0"), 3: ("PC0", "A1"), 5: ("PC3", "A2"), 7: ("PF3", "A3"),
            9: ("PF5", "A4"), 11: ("PF10", "A5"), 13: ("NC", "D72"), 15: ("PA7", "D71"),
            17: ("PF2", "D70"), 19: ("PF1", "D69"), 21: ("PF0", "D68"), 23: ("GND", ""),
            25: ("PD0", "D67"), 27: ("PD1", "D66"), 29: ("PG0", "D65"),
            2: ("PD7", "D51"), 4: ("PD6", "D52"), 6: ("PD5", "D53"), 8: ("PD4", "D54"),
            10: ("PD3", "D55"), 12: ("GND", ""), 14: ("PE2", "D56"), 16: ("PE4", "D57"),
            18: ("PE5", "D58"), 20: ("PE6", "D59"), 22: ("PE3", "D60"), 24: ("PF8", "D61"),
            26: ("PF7", "D62"), 28: ("PF9", "D63"), 30: ("PG1", "D64")},
    "CN7": {1: ("PC6", "D16"), 3: ("PB15", "D17"), 5: ("PB13", "D18"), 7: ("PB12", "D19"),
            9: ("PA15", "D20"), 11: ("PC7", "D21"), 13: ("PB5", "D22"), 15: ("PB3", "D23"),
            17: ("PA4", "D24"), 19: ("PB4", "D25"),
            2: ("PB8", "D15"), 4: ("PB9", "D14"), 6: ("AREF", ""), 8: ("GND", ""),
            10: ("PA5", "D13"), 12: ("PA6", "D12"), 14: ("PA7", "D11"), 16: ("PD14", "D10"),
            18: ("PD15", "D9"), 20: ("PF12", "D8")},
    "CN10": {1: ("AVDD", ""), 3: ("AGND", ""), 5: ("GND", ""), 7: ("PB1", "A6"),
             9: ("PC2", "A7"), 11: ("PF4", "A8"), 13: ("PB6", "D26"), 15: ("PB2", "D27"),
             17: ("GND", ""), 19: ("PD13", "D28"), 21: ("PD12", "D29"), 23: ("PD11", "D30"),
             25: ("PE2", "D31"), 27: ("GND", ""), 29: ("PA0", "D32"), 31: ("PB0", "D33"),
             33: ("PE0", "D34"),
             2: ("PF13", "D7"), 4: ("PE9", "D6"), 6: ("PE11", "D5"), 8: ("PF14", "D4"),
             10: ("PE13", "D3"), 12: ("PF15", "D2"), 14: ("PG14", "D1"), 16: ("PG9", "D0"),
             18: ("PE8", "D42"), 20: ("PE7", "D41"), 22: ("GND", ""), 24: ("PE10", "D40"),
             26: ("PE12", "D39"), 28: ("PE14", "D38"), 30: ("PE15", "D37"), 32: ("PB10", "D36"),
             34: ("PB11", "D35")},
}

RESERVED = set("PA1 PA2 PA7 PB13 PC1 PC4 PC5 PG11 PG13 PA8 PA9 PA10 PA11 PA12 PG6 PG7 "
               "PA13 PA14 PB3 PD8 PD9 PB0 PB7 PB14 PC13 PH0 PH1 PC14 PC15".split())

# (pin, label, group, mode, goes to). Group keys are used for colors/filters.
PINS = [
    # 1. WS2812 LED strips
    ("PE9",  "",       "led",    "TIM1_CH1 (AF)", "74AHCT125 1A (pin 2) -> panel 0 P1 fleet"),
    ("PE11", "",       "led",    "TIM1_CH2 (AF)", "74AHCT125 2A (pin 5) -> panel 1 P1 attack"),
    ("PE13", "",       "led",    "TIM1_CH3 (AF)", "74AHCT125 3A (pin 9) -> panel 2 P2 fleet"),
    ("PE14", "",       "led",    "TIM1_CH4 (AF)", "74AHCT125 4A (pin 12) -> panel 3 P2 attack"),
    # 2. switch matrix columns (shared by all 4 panels)
    ("PD0",  "COL0",   "col",    "Output Open Drain", "column A (all 4 panels)"),
    ("PD1",  "COL1",   "col",    "Output Open Drain", "column B"),
    ("PD14", "COL2",   "col",    "Output Open Drain", "column C"),
    ("PD3",  "COL3",   "col",    "Output Open Drain", "column D"),
    ("PD4",  "COL4",   "col",    "Output Open Drain", "column E"),
    ("PD5",  "COL5",   "col",    "Output Open Drain", "column F"),
    ("PD6",  "COL6",   "col",    "Output Open Drain", "column G"),
    ("PD7",  "COL7",   "col",    "Output Open Drain", "column H"),
    # switch matrix rows: panel 0 P1 fleet
    ("PF0",  "ROW0",   "row0",   "Input + pull-up", "panel 0 (P1 fleet) row 1"),
    ("PF1",  "ROW1",   "row0",   "Input + pull-up", "panel 0 row 2"),
    ("PF2",  "ROW2",   "row0",   "Input + pull-up", "panel 0 row 3"),
    ("PF3",  "ROW3",   "row0",   "Input + pull-up", "panel 0 row 4"),
    ("PF4",  "ROW4",   "row0",   "Input + pull-up", "panel 0 row 5"),
    ("PF5",  "ROW5",   "row0",   "Input + pull-up", "panel 0 row 6"),
    ("PE15", "ROW6",   "row0",   "Input + pull-up", "panel 0 row 7"),
    # panel 1 P1 attack
    ("PF7",  "ROW7",   "row1",   "Input + pull-up", "panel 1 (P1 attack) row 1"),
    ("PF8",  "ROW8",   "row1",   "Input + pull-up", "panel 1 row 2"),
    ("PF9",  "ROW9",   "row1",   "Input + pull-up", "panel 1 row 3"),
    ("PF10", "ROW10",  "row1",   "Input + pull-up", "panel 1 row 4"),
    ("PB10", "ROW11",  "row1",   "Input + pull-up", "panel 1 row 5"),
    ("PF12", "ROW12",  "row1",   "Input + pull-up", "panel 1 row 6"),
    ("PF13", "ROW13",  "row1",   "Input + pull-up", "panel 1 row 7"),
    # panel 2 P2 fleet
    ("PF14", "ROW14",  "row2",   "Input + pull-up", "panel 2 (P2 fleet) row 1"),
    ("PF15", "ROW15",  "row2",   "Input + pull-up", "panel 2 row 2"),
    ("PG0",  "ROW16",  "row2",   "Input + pull-up", "panel 2 row 3"),
    ("PG1",  "ROW17",  "row2",   "Input + pull-up", "panel 2 row 4"),
    ("PG2",  "ROW18",  "row2",   "Input + pull-up", "panel 2 row 5"),
    ("PG3",  "ROW19",  "row2",   "Input + pull-up", "panel 2 row 6"),
    ("PB11", "ROW20",  "row2",   "Input + pull-up", "panel 2 row 7"),
    # panel 3 P2 attack
    ("PD11", "ROW21",  "row3",   "Input + pull-up", "panel 3 (P2 attack) row 1"),
    ("PD12", "ROW22",  "row3",   "Input + pull-up", "panel 3 row 2"),
    ("PG9",  "ROW23",  "row3",   "Input + pull-up", "panel 3 row 3"),
    ("PD13", "ROW24",  "row3",   "Input + pull-up", "panel 3 row 4"),
    ("PB2",  "ROW25",  "row3",   "Input + pull-up", "panel 3 row 5"),
    ("PG14", "ROW26",  "row3",   "Input + pull-up", "panel 3 row 6"),
    ("PB6",  "ROW27",  "row3",   "Input + pull-up", "panel 3 row 7"),
    # 3. TFT x2 (SPI1)
    ("PA5",  "",       "tft",    "SPI1_SCK",  "SCK of both TFTs"),
    ("PA6",  "",       "tft",    "SPI1_MISO", "SDO(MISO) of both TFTs (optional)"),
    ("PB5",  "",       "tft",    "SPI1_MOSI", "SDI(MOSI) of both TFTs"),
    ("PC8",  "CS",     "tft",    "Output, High", "TFT P1 CS"),
    ("PC9",  "DC",     "tft",    "Output", "TFT P1 DC"),
    ("PC10", "RST",    "tft",    "Output, High", "TFT P1 RESET"),
    ("PC11", "CS2",    "tft",    "Output, High", "TFT P2 CS"),
    ("PC12", "DC2",    "tft",    "Output", "TFT P2 DC"),
    ("PD2",  "RST2",   "tft",    "Output, High", "TFT P2 RESET"),
    # 4. touch x2 (bit-bang)
    ("PE3",  "T_CLK",  "touch",  "Output, Very High", "T_CLK of both panels"),
    ("PE5",  "T_MOSI", "touch",  "Output, Very High", "T_DIN of both panels"),
    ("PE4",  "T_MISO", "touch",  "Input", "T_DO of both panels"),
    ("PE6",  "T_CS",   "touch",  "Output, High", "T_CS P1"),
    ("PE7",  "T_CS2",  "touch",  "Output, High", "T_CS P2"),
    ("PE2",  "T_IRQ",  "touch",  "EXTI2 falling, pull-up", "T_IRQ P1"),
    ("PE8",  "T_IRQ2", "touch",  "EXTI8 falling, pull-up", "T_IRQ P2"),
    # 5. LCD1602 x2 (I2C1)
    ("PB8",  "",       "lcd",    "I2C1_SCL", "SCL of both LCD backpacks"),
    ("PB9",  "",       "lcd",    "I2C1_SDA", "SDA of both LCD backpacks"),
    # 6. sound + side switches
    ("PC6",  "",       "sound",  "TIM3_CH1 (AF)", "1k -> 2N2222 base -> buzzer P1"),
    ("PC7",  "",       "sound",  "TIM3_CH2 (AF)", "1k -> 2N2222 base -> buzzer P2"),
    ("PE0",  "MUTE1",  "switch", "EXTI0 rising+falling, pull-up", "mute slide switch P1 (other leg GND)"),
    ("PB1",  "MUTE2",  "switch", "EXTI1 rising+falling, pull-up", "mute slide switch P2 (other leg GND)"),
    ("PE10", "SURR1",  "switch", "EXTI10 falling, pull-up", "surrender button P1 (other leg GND)"),
    ("PE12", "SURR2",  "switch", "EXTI12 falling, pull-up", "surrender button P2 (other leg GND)"),
]


def locate():
    where = {}
    for conn, pins in ZIO.items():
        for num, (sig, zname) in pins.items():
            where.setdefault(sig, []).append((conn, num, zname))
    return where


def main():
    where = locate()
    errors = []
    seen = {}
    exti = {}
    for pin, label, group, mode, to in PINS:
        if pin in seen:
            errors.append(f"{pin} used twice ({seen[pin]} and {label or group})")
        seen[pin] = label or group
        if pin in RESERVED:
            errors.append(f"{pin} is reserved by the board")
        if pin not in where:
            errors.append(f"{pin} is not on any Zio header")
        if "EXTI" in mode:
            line = int(pin[2:])
            if line in exti:
                errors.append(f"EXTI{line} used by {exti[line]} and {pin}")
            exti[line] = pin
    if errors:
        print("ERRORS:\n  " + "\n  ".join(errors))
        sys.exit(1)

    out = []
    for pin, label, group, mode, to in PINS:
        conn, num, zname = where[pin][0]
        out.append({"pin": pin, "label": label, "group": group, "mode": mode, "to": to,
                    "conn": conn, "num": num, "zio": zname,
                    "also": [f"{c}-{n}" for c, n, _ in where[pin][1:]]})
    headers = {c: [{"num": n, "sig": s, "zio": z} for n, (s, z) in sorted(p.items())]
               for c, p in ZIO.items()}
    here = os.path.dirname(os.path.abspath(__file__))
    data = {"pins": out, "headers": headers}
    path = os.path.join(here, "pinmap.json")
    with open(path, "w", encoding="utf8") as f:
        json.dump(data, f, indent=1)

    # wiring infographic: tools/wiring_template.html + data -> docs/wiring.html
    with open(os.path.join(here, "wiring_template.html"), encoding="utf8") as f:
        page = f.read().replace("/*PINMAP*/null", json.dumps(data, ensure_ascii=False))
    html = os.path.join(here, "..", "..", "docs", "wiring.html")
    with open(html, "w", encoding="utf8") as f:
        f.write(page)
    print(f"OK: {len(out)} pins, all on Zio, unique, EXTI lines {sorted(exti)}")
    print(f"   -> {os.path.normpath(path)}\n   -> {os.path.normpath(html)}")


if __name__ == "__main__":
    main()
