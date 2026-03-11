//
// Created by miracleaigbogun on 3/10/26.
//

#pragma once

enum class Key {
    NONE,
    NUM_0, NUM_1, NUM_2, NUM_3, NUM_4, NUM_5, NUM_6, NUM_7, NUM_8, NUM_9,
    PLUS, MINUS, MULTIPLY, DIVIDE, EQUALS,
    ENTER, CLEAR, ESCAPE,
    DOT,
};

class Keypad {
public:
    virtual ~Keypad() = default;
    virtual void init() = 0;

    // Returns Key::NONE if nothing is being pressed
    virtual Key getKey() = 0;
};