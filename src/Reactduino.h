#ifndef REACTDUINO_H_
#define REACTDUINO_H_

#include <Arduino.h>
#include <stdint.h>

#include <functional>
#include <list>
#include <queue>

typedef std::function<void()> react_callback;

#define INPUT_STATE_HIGH    HIGH
#define INPUT_STATE_LOW     LOW
#define INPUT_STATE_ANY     0xFF
#define INPUT_STATE_UNSET   0xFE


// forward declarations

class Reactduino;

///////////////////////////////////////
// Reaction classes define the reaction behaviour

class Reaction {
protected:
    const react_callback callback;
public:
    Reaction(const react_callback callback)
    : callback(callback) {}
    // FIXME: why do these have to be defined?
    virtual void alloc(Reactduino& app) { 1/0; }
    virtual void free(Reactduino& app) { 1/0; }
    virtual void disable() {}
    uint8_t flags;
    virtual void tick(Reactduino& app) {}
};

class TimedReaction : public Reaction {
protected:
    const uint32_t interval;
    uint32_t last_trigger_time;
    bool enabled;
public:
    TimedReaction(const uint32_t interval, const react_callback callback) 
    : interval(interval), Reaction(callback) {
        last_trigger_time = millis();
        enabled = true;
    }
    bool operator<(const TimedReaction& other);
    virtual void alloc(Reactduino& app);
    virtual void free(Reactduino& app);
    void disable();
    uint32_t getTriggerTime() { return last_trigger_time + interval; }
    bool isEnabled() { return enabled; }
};

class DelayReaction : public TimedReaction {
public:
    DelayReaction(const uint32_t interval, const react_callback callback); 
    void tick(Reactduino& app);
};

class RepeatReaction: public TimedReaction {
public:
    RepeatReaction(const uint32_t interval, const react_callback callback) 
    : TimedReaction(interval, callback) {}
    void tick(Reactduino& app);
};

class UntimedReaction : public Reaction {
public:
    UntimedReaction(const react_callback callback)
    : Reaction(callback) {}
    virtual void alloc(Reactduino& app);
    virtual void free(Reactduino& app);
};

class StreamReaction : public UntimedReaction {
private:
    Stream& stream;
public:
    StreamReaction(Stream& stream, const react_callback callback)
    : stream(stream), UntimedReaction(callback) {}
    void tick(Reactduino& app);
};

class TickReaction : public UntimedReaction {
public:
    TickReaction(const react_callback callback)
    : UntimedReaction(callback) {}
    void tick(Reactduino& app);
};

class ISRReaction : public UntimedReaction {
private:
    const uint32_t pin_number;
public:
    ISRReaction(uint32_t pin_number, int8_t isr, react_callback callback)
    : pin_number(pin_number), isr(isr), UntimedReaction(callback) {}
    int8_t isr;
    void disable();
    void tick(Reactduino& app);
};


///////////////////////////////////////
// Reactduino main class implementation

class Reactduino
{
    friend class Reaction;
    friend class TimedReaction;
    friend class RepeatReaction;
    friend class UntimedReaction;
public:
    Reactduino(react_callback cb);
    void setup(void);
    void tick(void);

    // static singleton reference to the instantiated Reactduino object
    static Reactduino* app;

    // Public API
    DelayReaction* onDelay(const uint32_t t, const react_callback cb);
    RepeatReaction* onRepeat(const uint32_t t, const react_callback cb);
    StreamReaction* onAvailable(Stream& stream, const react_callback cb);
    ISRReaction* onInterrupt(const uint8_t number, const react_callback cb, int mode);
    ISRReaction* onPinRising(const uint8_t pin, const react_callback cb);
    ISRReaction* onPinFalling(const uint8_t pin, const react_callback cb);
    ISRReaction* onPinChange(const uint8_t pin, const react_callback cb);
    TickReaction* onTick(const react_callback cb);

private:
    const react_callback _setup;
    std::priority_queue<TimedReaction*, std::vector<TimedReaction*>, std::greater<TimedReaction*>> timed_queue;
    std::list<UntimedReaction*> untimed_list;
    void tickTimed();
    void tickUntimed();
    void alloc(Reaction* re);
};

#endif
