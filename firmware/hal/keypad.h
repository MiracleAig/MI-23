//
// Created by Miracle Aigbogun on 3/10/26.
//

#pragma once

enum class Key {
    NONE = 0,
   // Digits
   NUM_0 = 0x30, NUM_1 = 0x31, NUM_2 = 0x32, NUM_3 = 0x33,
   NUM_4 = 0x34, NUM_5 = 0x35, NUM_6 = 0x36, NUM_7 = 0x37,
   NUM_8 = 0x38, NUM_9 = 0x39,
   // Printable symbols
   PLUS        = 0x2B,
   MINUS       = 0x2D,
   MULTIPLY    = 0x2A,
   DIVIDE      = 0x2F,
   EQUALS      = 0x3D,
   OPEN_PAREN  = 0x28,
   CLOSE_PAREN = 0x29,
   COMMA       = 0x2C,
   DOT         = 0x2E,
   POWER       = 0x5E,
   PERCENT     = 0x25,
   FACTORIAL   = 0x21,
   E_CONST     = 0x65,
   // Action keys — above 0x80 so they never collide with printable ASCII
   ENTER       = 0x80,
   CLEAR       = 0x81,
   ESCAPE      = 0x82,
   // Cursor movement
   CURSOR_LEFT  = 0x83,
   CURSOR_RIGHT = 0x84,
   CURSOR_UP    = 0x85,
   CURSOR_DOWN  = 0x86,
   // Unary / sign flip
   NEGATE      = 0x87,
   // Trig
   SIN         = 0x88,
   COS         = 0x89,
   TAN         = 0x8A,
   SQRT        = 0x8B,
   COT         = 0x8C,
   SEC         = 0x8D,
   CSC         = 0x8E,
   ASIN        = 0x8F,
   ACOS        = 0x90,
   ATAN        = 0x91,
   ACOT        = 0x92,
   ASEC        = 0x93,
   ACSC        = 0x94,
   LOG         = 0x95,
   LN          = 0x96,
   ANS         = 0x97,
   ROOT        = 0x98,
   // Custom font characters
   PI          = 0xFF,

};

inline bool isPrintable(Key k) {
    int v = static_cast<int>(k);
    return (v >= 0x20 && v < 0x80) || v == 0xFF;
}

inline char toChar(Key k) {
    if (k == Key::PI) return (char)128;
    return static_cast<char>(k);
}

class Keypad {
public:
    virtual ~Keypad() = default;
    virtual void init() = 0;
    static constexpr char PI_CHAR = (char)128;

    // Returns Key::NONE if nothing is being pressed
    virtual Key getKey() = 0;
};
