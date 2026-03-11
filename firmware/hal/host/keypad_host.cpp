//
// Created by miracleaigbogun on 3/10/26.
//

#include "keypad_host.h"

void KeypadHost::init() {

}

Key KeypadHost::getKey() {
    Key k = m_currentKey;
    m_currentKey = Key::NONE;
    return k;
}

void KeypadHost::handleEvent(const SDL_Event &event) {
    if (event.type != SDL_KEYDOWN) return;

    switch (event.key.keysym.sym) {
        case SDLK_0: m_currentKey = Key::NUM_0; break;
        case SDLK_1: m_currentKey = Key::NUM_1; break;
        case SDLK_2: m_currentKey = Key::NUM_2; break;
        case SDLK_3: m_currentKey = Key::NUM_3; break;
        case SDLK_4: m_currentKey = Key::NUM_4; break;
        case SDLK_5: m_currentKey = Key::NUM_5; break;
        case SDLK_6: m_currentKey = Key::NUM_6; break;
        case SDLK_7: m_currentKey = Key::NUM_7; break;
        case SDLK_8: m_currentKey = Key::NUM_8; break;
        case SDLK_9: m_currentKey = Key::NUM_9; break;
        case SDLK_PLUS:     m_currentKey = Key::PLUS;     break;
        case SDLK_MINUS:    m_currentKey = Key::MINUS;    break;
        case SDLK_ASTERISK: m_currentKey = Key::MULTIPLY; break;
        case SDLK_SLASH:    m_currentKey = Key::DIVIDE;   break;
        case SDLK_RETURN:   m_currentKey = Key::ENTER;    break;
        case SDLK_BACKSPACE:m_currentKey = Key::CLEAR;    break;
        case SDLK_PERIOD:   m_currentKey = Key::DOT;      break;
        default: break;
    }
}