//
// Created by Miracle Aigbogun on 3/21/26
// pin_config.h — physical wiring reference for the MI-23 prototype
//

#pragma once

// ── Display (SPI1) ───────────────────────────────────────────────────────────
#define PIN_DISPLAY_SCK     10   // Serial clock        — SPI1 SCK
#define PIN_DISPLAY_MOSI    11   // Data out            — SPI1 TX
#define PIN_DISPLAY_CS      13   // Chip select         — active LOW
#define PIN_DISPLAY_DC      14   // Data/command select — LOW=command, HIGH=data
#define PIN_DISPLAY_RST     15   // Hardware reset      — active LOW
// BL (backlight) is wired directly to 3.3V — always on, not software controlled

// ── Keypad 1 — numpad (4×4 matrix) ──────────────────────────────────────────
// Wire color reference:
//   Rows:    Purple=R1, Gray=R2, White=R3, Black=R4
//   Columns: Blue=C1,   Green=C2, Yellow=C3, Orange=C4
#define PIN_KP1_ROW_0   19
#define PIN_KP1_ROW_1   20
#define PIN_KP1_ROW_2   26
#define PIN_KP1_ROW_3   21
#define PIN_KP1_COL_0   16
#define PIN_KP1_COL_1    6
#define PIN_KP1_COL_2    9
#define PIN_KP1_COL_3    7

// ── Keypad 2 — navigation + operators (4×4 matrix) ──────────────────────────
// Wire color reference:
//   Rows:    Gray=R1, White=R2, Black=R3, Brown=R4
//   Columns: Purple=C1, Blue=C2, Green=C3, Yellow=C4
#define PIN_KP2_ROW_0    2
#define PIN_KP2_ROW_1    3
#define PIN_KP2_ROW_2   17
#define PIN_KP2_ROW_3   27
#define PIN_KP2_COL_0    4
#define PIN_KP2_COL_1    5
#define PIN_KP2_COL_2   18
#define PIN_KP2_COL_3   23