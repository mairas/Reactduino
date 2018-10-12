#include <Arduino.h>
#include <string.h>
#include <FunctionalInterrupt.h>

#include "Reactduino.h"
#include "ReactduinoISR.h"

// Reaction classes define the behaviour of each particular
// Reaction

bool TimedReaction::operator<(const TimedReaction& other) {
    return (this->last_trigger_time + this->interval) >
        (other.last_trigger_time + other.interval);
}

void TimedReaction::alloc(Reactduino& app) {
    app.timed_queue.push(this);
}

void TimedReaction::free(Reactduino& app) {
    this->disable();
}

void TimedReaction::disable() {
    this->enabled = false;
}


DelayReaction::DelayReaction(uint32_t interval, const react_callback callback) 
        : TimedReaction(interval, callback) {
    this->last_trigger_time = millis();
}

void DelayReaction::tick(Reactduino& app) {
    this->last_trigger_time = millis();
    this->callback();
    delete this;
}


void RepeatReaction::tick(Reactduino& app) {
    this->last_trigger_time = millis();
    this->callback();
    app.timed_queue.push(this);
}


void UntimedReaction::alloc(Reactduino& app) {
    app.untimed_list.push_back(this);
}

void UntimedReaction::free(Reactduino& app) {
    auto it = std::find(app.untimed_list.begin(), app.untimed_list.end(), this);
    app.untimed_list.erase(it);
}


void StreamReaction::tick(Reactduino& app) {
    if (stream.available()) {
        this->callback();
    }
}


void TickReaction::tick(Reactduino& app) {
    this->callback();
}


void ISRReaction::tick(Reactduino& app) {
    if (react_isr_check(this->pin_number)) {
        this->callback();
    }
}

void ISRReaction::disable() {
    detachInterrupt(this->pin_number);
    react_isr_free(this->isr);
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

Reactduino::Reactduino(const react_callback cb) : _setup(cb)
{
    app = this;
}

void Reactduino::setup(void)
{
    _setup();
}

void Reactduino::tickTimed() {
    uint32_t now = millis();
    uint32_t trigger_t;
    TimedReaction* top;

    //Serial.println("Beginning tickTimed");
    while (true) {
        if (timed_queue.empty()) {
            Serial.println("Empty queue");
            break;
        }
        top = timed_queue.top();
        if (!top->isEnabled()) {
            Serial.println("Top item not enabled");
            timed_queue.pop();
            delete top;
            continue;
        }
        trigger_t = top->getTriggerTime();
        if (trigger_t<=now) {
            //Serial.println("Triggered");
            timed_queue.pop();
            top->tick(*this);
        } else {
            //Serial.println("Not yet triggered");
            break;
        }
    }
    //Serial.println("Exiting tickTimed");
}

void Reactduino::tickUntimed() {
    for (UntimedReaction* re : this->untimed_list) {
        re->tick(*this);
    }
}

void Reactduino::tick() {
    tickTimed();
    tickUntimed();
}

DelayReaction* Reactduino::onDelay(const uint32_t t, const react_callback cb) {
    DelayReaction* dre = new DelayReaction(t, cb);
    dre->alloc(*this);
    return dre;
}

RepeatReaction* Reactduino::onRepeat(const uint32_t t, const react_callback cb) {
    RepeatReaction* rre = new RepeatReaction(t, cb);
    rre->alloc(*this);
    return rre;
}

StreamReaction* Reactduino::onAvailable(Stream& stream, const react_callback cb) {
    StreamReaction *sre = new StreamReaction(stream, cb);
    sre->alloc(*this);
    return sre;
}

ISRReaction* Reactduino::onInterrupt(const uint8_t number, const react_callback cb, int mode) {
    int8_t isr;

    // Obtain and use ISR to handle the interrupt, see: ReactduinoISR.c/.h

    isr = react_isr_alloc();

    ISRReaction* isrre = new ISRReaction(number, isr, cb);
    isrre->alloc(*this);
    attachInterrupt(number, react_isr_get(isr), mode);

    return isrre;
}

ISRReaction* Reactduino::onPinRising(const uint8_t pin, const react_callback cb) {
    return onInterrupt(digitalPinToInterrupt(pin), cb, RISING);
}

ISRReaction* Reactduino::onPinFalling(const uint8_t pin, const react_callback cb) {
    return onInterrupt(digitalPinToInterrupt(pin), cb, FALLING);
}

ISRReaction* Reactduino::onPinChange(const uint8_t pin, const react_callback cb) {
    return onInterrupt(digitalPinToInterrupt(pin), cb, CHANGE);
}

TickReaction* Reactduino::onTick(const react_callback cb) {
    TickReaction* tre = new TickReaction(cb);
    tre->alloc(*this);
    return tre;
}

void Reactduino::alloc(Reaction *re)
{
    re->alloc(*this);
}
