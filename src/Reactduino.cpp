#include <Arduino.h>
#include <string.h>
#include <FunctionalInterrupt.h>

#include "Reactduino.h"
#include "ReactduinoISR.h"

// ReactionEntry classes define the behaviour of each particular
// Reaction

void DelayReactionEntry::tick(Reactduino app, reaction r_pos) {
    uint32_t elapsed;
    uint32_t now = millis();
    elapsed = now - this->last_trigger_time;
    if (elapsed >= this->interval) {
        this->last_trigger_time = now;
        app->free(r_pos);
        this->callback();
    }
}

void RepeatReactionEntry::tick(Reactduino app, reaction r_pos) {
    uint32_t elapsed;
    uint32_t now = millis();
    elapsed = now - this->last_trigger_time;
    if (elapsed >= this->interval) {
        this->last_trigger_time = now;
        this->callback();
    }
}

void StreamReactionEntry::tick(Reactduino app, reaction r_pos) {
    if (stream->available()) {
        this->callback();
    }
}

void TickReactionEntry::tick(Reactduino app, reaction r_pos) {
    this->callback();
}

void ISRReactionEntry::tick(Reactduino app, reaction r_pos) {
    if (react_isr_check(this->pin_number)) {
        this->callback();
    }
}

// Need to define the static variable outside of the class
Reactduino* Reactduino::app = NULL;

void setup(void)
{
    Reactduino::app->setup();
}

void loop(void)
{
    Reactduino::app->tick();
    yield();
}

Reactduino::Reactduino(react_callback cb) : _setup(cb)
{
    app = this;
}

void Reactduino::setup(void)
{
    _setup();
}

void Reactduino::tick(void)
{
    reaction r;
    uint32_t now = millis();    

    for (r = 0; r < _top; r++) {
        reaction_entry_t& r_entry = _table[r];

        if (!(r_entry.flags & REACTION_FLAG_ALLOCATED) || !(r_entry.flags & REACTION_FLAG_ENABLED)) {
            continue;
        }

        r_entry->tick(app, r);
    }
}

reaction Reactduino::onDelay(uint32_t t, react_callback cb)
{
    reaction r;

    r = alloc(REACTION_TYPE_DELAY, cb);

    if (r == INVALID_REACTION) {
        return INVALID_REACTION;
    }

    _table[r].param1 = millis();
    _table[r].param2 = t;

    return r;
}

reaction Reactduino::onRepeat(uint32_t t, react_callback cb)
{
    reaction r;

    r = alloc(REACTION_TYPE_REPEAT, cb);

    if (r == INVALID_REACTION) {
        return INVALID_REACTION;
    }

    _table[r].param1 = millis();
    _table[r].param2 = t;

    return r;
}

reaction Reactduino::onAvailable(Stream *stream, react_callback cb)
{
    reaction r;

    r = alloc(REACTION_TYPE_STREAM, cb);

    if (r == INVALID_REACTION) {
        return INVALID_REACTION;
    }

    _table[r].ptr = stream;

    return r;
}

reaction Reactduino::onInterrupt(uint8_t number, react_callback cb, int mode)
{
    reaction r;
    int8_t isr;

    // Obtain and use ISR to handle the interrupt, see: ReactduinoISR.c/.h

    isr = react_isr_alloc();

    if (isr == INVALID_ISR) {
        return INVALID_REACTION;
    }

    r = alloc(REACTION_TYPE_INTERRUPT, cb);

    if (r == INVALID_REACTION) {
        react_isr_free(isr);
        return INVALID_REACTION;
    }

    _table[r].param1 = isr;
    _table[r].param2 = number;

    attachInterrupt(number, react_isr_get(isr), mode);

    return r;
}

reaction Reactduino::onPinRising(uint8_t pin, react_callback cb)
{
    return onInterrupt(digitalPinToInterrupt(pin), cb, RISING);
}

reaction Reactduino::onPinFalling(uint8_t pin, react_callback cb)
{
    return onInterrupt(digitalPinToInterrupt(pin), cb, FALLING);
}

reaction Reactduino::onPinChange(uint8_t pin, react_callback cb)
{
    return onInterrupt(digitalPinToInterrupt(pin), cb, CHANGE);
}

reaction Reactduino::onTick(react_callback cb)
{
    return alloc(REACTION_TYPE_TICK, cb);
}

void Reactduino::free(reaction r)
{
    if (r == INVALID_REACTION) {
        return;
    }

    // Disable any interrupts and free the ISR for reallocation
    if (REACTION_TYPE(_table[r].flags) == REACTION_TYPE_INTERRUPT) {
        detachInterrupt(_table[r].param2);
        react_isr_free(_table[r].param1);
    }

    _table[r].flags &= ~REACTION_FLAG_ALLOCATED;

    // Move the top of the stack pointer down if we free from the top
    if (_top == r + 1) {
        _top--;
    }
}

reaction Reactduino::alloc(uint8_t type, react_callback cb)
{
    reaction r;

    for (r = 0; r < REACTDUINO_MAX_REACTIONS; r++) {
        // If we're at the top of the stak or the allocated flag isn't set
        if (r >= _top || !(_table[r].flags & REACTION_FLAG_ALLOCATED)) {
            // Reaction is allocated, enabled and of the provided type
            _table[r].flags = REACTION_FLAG_ALLOCATED | REACTION_FLAG_ENABLED | (type & REACTION_TYPE_MASK);
            _table[r].cb = cb;

            // Move the stack pointer up if we add to the top
            if (r >= _top) {
                _top = r + 1;
            }

            return r;
        }
    }

    return INVALID_REACTION;
}
