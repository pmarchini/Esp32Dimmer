# Design Document: Single-Fire Timer Implementation for Triac Control

## Executive Summary

This document proposes a redesign of the ESP32 Triac Dimmer Driver to use a **single-fire timer** approach instead of the current periodic timer with continuous checking. The new implementation will improve efficiency by eliminating unnecessary ISR invocations and reduce CPU overhead.

## Current Implementation Analysis

### How It Currently Works

The existing implementation uses a **periodic timer** that fires every 100μs (for 50Hz) or 83.3μs (for 60Hz):

1. **Zero-Crossing ISR** (`isr_ext`): Sets `zeroCross[i] = 1` when AC voltage crosses zero
2. **Timer ISR** (`onTimerISR`): Fires **every timer interval** (100 times per half-cycle)
   - Checks `if (zeroCross[k] == 1)` for each dimmer
   - Increments `dimCounter[k]++`
   - Checks `if (dimCounter[k] >= dimPulseBegin[k])` to fire the triac
   - Checks `if (dimCounter[k] >= (dimPulseBegin[k] + pulseWidth))` to turn off

### Problems with Current Approach

1. **Excessive ISR Invocations**: Timer ISR fires 100 times per half-cycle, even when nothing needs to happen
2. **Continuous Checking**: Every timer interrupt must check the state of all dimmers
3. **CPU Overhead**: For 50Hz with 3 dimmers, that's 100 × 100 Hz × 3 = 30,000 checks per second
4. **Wasted Cycles**: Most timer interrupts do nothing useful except increment counters
5. **Latency**: Maximum latency is one timer interval (100μs) even if we know the exact firing time

## Proposed Implementation: Single-Fire Timer

### Core Concept

Instead of checking on every timer cycle, **calculate the exact time to fire** and set a **one-shot timer** that fires once at that specific moment.

### Key Components

#### 1. Timer Modes

The ESP32 GPTimer supports both modes:
- **Periodic Mode** (current): Timer auto-reloads and fires repeatedly
- **One-Shot Mode** (proposed): Timer fires once and stops

We'll use **one-shot timers** scheduled at precise firing times.

#### 2. Dual Timer Architecture

To support multiple dimmers and handle the pulse width, we need:

**Timer A - Triac Engagement Timer**
- One-shot timer that fires when the triac should engage
- Calculated as: `fire_time = zeroCross_timestamp + (dimPulseBegin[k] × timer_interval)`

**Timer B - Pulse Width Timer** 
- One-shot timer that fires after the pulse width duration
- Calculated as: `pulse_off_time = fire_time + (pulseWidth × timer_interval)`

**Alternative: Single Timer with Event Queue**
- Use a single timer with dynamically updated alarm values
- Maintain a sorted event queue of (timestamp, action, dimmer_id)
- Fire timer at next event, execute action, schedule next event

#### 3. New Data Structures

```c
// Per-dimmer timing information
typedef struct {
    uint64_t next_fire_time;      // Absolute timestamp when triac should fire
    uint64_t pulse_end_time;      // Absolute timestamp when pulse should end
    bool pending_fire;            // True if fire event is scheduled
    bool pending_pulse_end;       // True if pulse end is scheduled
} dimmer_timing_t;

// Event queue for managing multiple events
typedef struct {
    uint64_t timestamp;           // When this event should occur
    uint8_t dimmer_id;            // Which dimmer this affects
    enum {
        EVENT_FIRE_TRIAC,
        EVENT_END_PULSE
    } event_type;
} timer_event_t;
```

#### 4. Zero-Crossing ISR (Modified)

```c
static void IRAM_ATTR isr_ext(void *arg)
{
    uint64_t current_time = get_timer_count();  // Get current timestamp
    
    for (int i = 0; i < current_dim; i++)
    {
        if (dimState[i] == ON)
        {
            // Calculate exact fire time
            uint64_t fire_delay = dimPulseBegin[i] * timer_interval_us;
            uint64_t fire_time = current_time + fire_delay;
            
            // Schedule one-shot timer to fire at that time
            schedule_timer_event(fire_time, i, EVENT_FIRE_TRIAC);
        }
    }
}
```

#### 5. Timer ISR (Simplified)

```c
static void IRAM_ATTR onTimerISR(void *para)
{
    timer_event_t *event = get_next_event();
    
    if (event->event_type == EVENT_FIRE_TRIAC)
    {
        // Fire the triac
        gpio_set_level(dimOutPin[event->dimmer_id], 1);
        
        // Schedule pulse end event
        uint64_t pulse_end = event->timestamp + (pulseWidth * timer_interval_us);
        schedule_timer_event(pulse_end, event->dimmer_id, EVENT_END_PULSE);
    }
    else if (event->event_type == EVENT_END_PULSE)
    {
        // Turn off triac gate
        gpio_set_level(dimOutPin[event->dimmer_id], 0);
    }
    
    // Schedule next event if queue not empty
    if (has_pending_events())
    {
        timer_event_t *next = peek_next_event();
        set_timer_alarm(next->timestamp);
    }
}
```

## Implementation Strategy

### Phase 1: Add Event Queue Management

Create functions to manage the event queue:

```c
// Initialize event queue
void init_event_queue(void);

// Add event to queue (sorted by timestamp)
void schedule_timer_event(uint64_t timestamp, uint8_t dimmer_id, event_type_t type);

// Get next event from queue
timer_event_t* get_next_event(void);

// Check if events are pending
bool has_pending_events(void);

// Peek at next event without removing
timer_event_t* peek_next_event(void);
```

### Phase 2: Timer Reconfiguration

Modify timer setup to support dynamic alarm values:

```c
void config_timer_oneshot(int ACfreq)
{
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000  // 1MHz = 1μs resolution
    };
    
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
    
    // Don't set auto_reload_on_alarm = true
    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = 0,  // Will be set dynamically
        .flags.auto_reload_on_alarm = false  // One-shot mode
    };
    
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));
    
    // Register callbacks
    gptimer_event_callbacks_t cbs = {
        .on_alarm = onTimerISR
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));
    
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));
}
```

### Phase 3: Zero-Crossing ISR Update

Modify `isr_ext()` to calculate and schedule events:

```c
static void IRAM_ATTR isr_ext(void *arg)
{
    uint64_t zc_time;
    gptimer_get_raw_count(gptimer, &zc_time);
    
    for (int i = 0; i < current_dim; i++)
    {
        if (dimState[i] == ON)
        {
            // Calculate fire time based on power level
            uint64_t delay_ticks = dimPulseBegin[i] * alarm_interval;
            uint64_t fire_time = zc_time + delay_ticks;
            
            // Schedule the fire event
            schedule_timer_event(fire_time, i, EVENT_FIRE_TRIAC);
        }
    }
    
    // Start timer with first event if not already running
    if (has_pending_events() && !timer_running)
    {
        timer_event_t *next = peek_next_event();
        gptimer_set_raw_count(gptimer, 0);
        gptimer_set_alarm_action(gptimer, &(gptimer_alarm_config_t){
            .alarm_count = next->timestamp - zc_time,
            .flags.auto_reload_on_alarm = false
        });
    }
}
```

### Phase 4: Timer ISR Simplification

Simplify `onTimerISR()` to only handle scheduled events:

```c
static void IRAM_ATTR onTimerISR(void *para)
{
    timer_event_t *event = get_next_event();
    
    if (event == NULL) return;
    
    switch (event->event_type)
    {
        case EVENT_FIRE_TRIAC:
            // Fire triac
            gpio_set_level(dimOutPin[event->dimmer_id], 1);
            
            // Schedule pulse end
            uint64_t current_time;
            gptimer_get_raw_count(gptimer, &current_time);
            uint64_t pulse_end_time = current_time + (pulseWidth * alarm_interval);
            schedule_timer_event(pulse_end_time, event->dimmer_id, EVENT_END_PULSE);
            break;
            
        case EVENT_END_PULSE:
            // Turn off triac
            gpio_set_level(dimOutPin[event->dimmer_id], 0);
            break;
    }
    
    // Schedule next event
    if (has_pending_events())
    {
        timer_event_t *next = peek_next_event();
        uint64_t current_time;
        gptimer_get_raw_count(gptimer, &current_time);
        
        gptimer_alarm_config_t alarm_config = {
            .alarm_count = next->timestamp,
            .flags.auto_reload_on_alarm = false
        };
        gptimer_set_alarm_action(gptimer, &alarm_config);
    }
}
```

## Benefits of New Approach

### 1. Reduced CPU Overhead

**Current**: 100 ISR invocations per half-cycle × 100 half-cycles/sec = 10,000 ISRs/sec

**New**: 2 ISR invocations per half-cycle × 100 half-cycles/sec = 200 ISRs/sec

**Reduction**: 98% fewer ISR invocations

### 2. Improved Precision

- No latency from waiting for next timer tick
- Exact firing at calculated time
- Better power control accuracy

### 3. Lower Power Consumption

- Fewer interrupts = less CPU wake-ups
- Better for battery-powered applications

### 4. Better Multi-Dimmer Support

- Scalability: Adding more dimmers doesn't increase ISR frequency
- Current approach: 3 dimmers × 100 checks = 300 checks per half-cycle
- New approach: 3 dimmers × 2 events = 6 events per half-cycle

### 5. Cleaner Code

- No need for counter variables
- No need for state tracking across interrupts
- Event-driven architecture is more maintainable

## Potential Challenges

### 1. Event Queue Management in ISR

**Challenge**: Managing a sorted queue in an ISR context

**Solution**: 
- Use a fixed-size circular buffer
- Pre-allocate event structures
- Use simple insertion sort (small queue size)
- Or use a priority queue implementation

### 2. Timer Counter Overflow

**Challenge**: ESP32 timer is 64-bit but will eventually overflow

**Solution**:
- Use relative timestamps from zero-crossing
- Reset timer counter on each zero-crossing
- Or use wraparound-safe comparisons

### 3. Multiple Simultaneous Events

**Challenge**: Multiple dimmers might need to fire at nearly the same time

**Solution**:
- Process events in timestamp order
- Handle multiple events with same timestamp
- Set next alarm immediately after processing

### 4. Toggle Mode Adaptation

**Challenge**: Toggle mode currently uses periodic counter

**Solution**:
- Implement toggle logic outside of ISR
- Use a separate FreeRTOS task for toggle updates
- Update `dimPulseBegin[]` values periodically
- Zero-crossing ISR will use updated values

## Backward Compatibility

### API Compatibility

All public API functions remain unchanged:
- `createDimmer()`
- `begin()`
- `setPower()`
- `getPower()`
- `setState()`
- `getState()`
- `changeState()`
- `setMode()`
- `getMode()`
- `toggleSettings()`

### Configuration Compatibility

- Same GPIO configuration
- Same frequency settings (50Hz/60Hz)
- Same power level mapping (0-99)
- Same pulse width settings

## Testing Strategy

### 1. Unit Tests

- Event queue operations
- Timer scheduling calculations
- Edge cases (overflow, simultaneous events)

### 2. Integration Tests

- Single dimmer operation
- Multiple dimmer operation
- Power level changes during operation
- Toggle mode functionality

### 3. Performance Tests

- ISR frequency measurement
- Timing accuracy measurement
- CPU utilization measurement
- Power consumption measurement

### 4. Regression Tests

- Ensure all existing examples still work
- Verify backward compatibility
- Test with different AC frequencies

## Migration Path

### Option A: New Implementation (Recommended)

1. Implement as new functions with new internal architecture
2. Keep existing functions for backward compatibility
3. Allow users to opt-in via configuration flag

### Option B: Direct Replacement

1. Replace implementation while keeping API
2. Thoroughly test before release
3. Document changes in release notes

## Code Size Impact

**Estimated Changes:**

- Event queue implementation: +200 lines
- Modified timer configuration: +50 lines
- Simplified ISR code: -30 lines
- Additional helper functions: +100 lines

**Net change**: ~+320 lines (but cleaner architecture)

## Performance Metrics

### Expected Improvements

| Metric | Current | Proposed | Improvement |
|--------|---------|----------|-------------|
| ISR frequency (single dimmer, 50Hz) | 10,000/sec | 200/sec | 98% reduction |
| ISR frequency (3 dimmers, 50Hz) | 10,000/sec | 600/sec | 94% reduction |
| CPU overhead (estimated) | 2-3% | 0.1-0.2% | 90% reduction |
| Timing precision | ±100μs | ±1μs | 100× improvement |
| Response latency | 0-100μs | <1μs | Near-zero latency |

## Implementation Timeline

1. **Phase 1** (Week 1): Design review and approval
2. **Phase 2** (Week 1-2): Implement event queue and helper functions
3. **Phase 3** (Week 2): Modify timer configuration
4. **Phase 4** (Week 2-3): Update ISR handlers
5. **Phase 5** (Week 3): Testing and debugging
6. **Phase 6** (Week 4): Documentation and examples

## Conclusion

The single-fire timer implementation offers significant performance improvements while maintaining full backward compatibility. The event-driven architecture is more efficient, more precise, and more scalable than the current periodic polling approach.

### Recommended Next Steps

1. **Review this design document** with stakeholders
2. **Prototype event queue** implementation
3. **Create test harness** for timing validation
4. **Implement Phase 1** (event queue)
5. **Iteratively implement** remaining phases
6. **Comprehensive testing** before merge

---

**Document Version:** 1.0  
**Date:** 2026-01-25  
**Status:** Proposed  
**Author:** ESP32 Triac Dimmer Driver Development Team
