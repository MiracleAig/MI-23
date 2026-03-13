//
// Created by Miracle Aigbogun on 3/10/26.
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
    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_LSHIFT || event.key.keysym.sym == SDLK_RSHIFT) {
            m_shiftHeld = true;
            return;
        }
    }
    if (event.type == SDL_KEYUP) {
        if (event.key.keysym.sym == SDLK_LSHIFT || event.key.keysym.sym == SDLK_RSHIFT) {
            m_shiftHeld = false;
            return;
        }
    }

    if (event.type != SDL_KEYDOWN) return;

    switch (event.key.keysym.sym) {
        case SDLK_0: m_currentKey = m_shiftHeld ? Key::CLOSE_PAREN : Key::NUM_0; break;
        case SDLK_1: m_currentKey = Key::NUM_1; break;
        case SDLK_2: m_currentKey = Key::NUM_2; break;
        case SDLK_3: m_currentKey = Key::NUM_3; break;
        case SDLK_4: m_currentKey = Key::NUM_4; break;
        case SDLK_5: m_currentKey = Key::NUM_5; break;
        case SDLK_6: m_currentKey = m_shiftHeld ? Key::POWER       : Key::NUM_6; break;
        case SDLK_7: m_currentKey = Key::NUM_7; break;
        case SDLK_8: m_currentKey = m_shiftHeld ? Key::MULTIPLY    : Key::NUM_8; break;
        case SDLK_9: m_currentKey = m_shiftHeld ? Key::OPEN_PAREN  : Key::NUM_9; break;
        case SDLK_EQUALS: m_currentKey = m_shiftHeld ? Key::PLUS   : Key::EQUALS; break;
        case SDLK_MINUS:  m_currentKey = Key::MINUS;  break;  // no shift needed
        case SDLK_SLASH:  m_currentKey = Key::DIVIDE; break;  // no shift needed
        case SDLK_PLUS:     m_currentKey = Key::PLUS;     break;
        case SDLK_ASTERISK: m_currentKey = Key::MULTIPLY; break;
        case SDLK_CARET:    m_currentKey = Key::POWER;    break;
        case SDLK_RETURN:   m_currentKey = Key::ENTER;    break;
        case SDLK_BACKSPACE:m_currentKey = Key::CLEAR;    break;
        case SDLK_PERIOD:   m_currentKey = Key::DOT;      break;
        default: break;
    }
}