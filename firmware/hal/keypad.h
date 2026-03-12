//
// Created by miracleaigbogun on 3/10/26.
//

#pragma once

enum class Key {
    NONE = 0,
    NUM_0 = 0x30,  // '0'
    NUM_1 = 0x31,  // '1'
    NUM_2 = 0x32,  // '2'
    NUM_3 = 0x33,  // '3'
    NUM_4 = 0x34,  // '4'
    NUM_5 = 0x35,  // '5'
    NUM_6 = 0x36,  // '6'
    NUM_7 = 0x37,  // '7'
    NUM_8 = 0x38,  // '8'
    NUM_9 = 0x39,  // '9'
    // Symbol keys
    PLUS        = 0x2B,  // '+'
    MINUS       = 0x2D,  // '-'
    MULTIPLY    = 0x2A,  // '*'
    DIVIDE      = 0x2F,  // '/'
    EQUALS      = 0x3D,  // '='
    OPEN_PAREN  = 0x28,  // '('
    CLOSE_PAREN = 0x29,  // ')'
    DOT         = 0x2E,  // '.' (decimal point)
    // These have no ASCII equivalent so just let them count up from 0x80
    // (above 127 so they never collide with any printable ASCII character)
    ENTER  = 0x80,
    CLEAR  = 0x81,
    ESCAPE = 0x82,
};

inline bool isPrintable(Key k) {
    return static_cast<int>(k) >= 0x20 && static_cast<int>(k) < 0x80;
}

inline char toChar(Key k) {
    return static_cast<char>(k);  // just works
}

class Keypad {
public:
    virtual ~Keypad() = default;
    virtual void init() = 0;

    // Returns Key::NONE if nothing is being pressed
    virtual Key getKey() = 0;
};

