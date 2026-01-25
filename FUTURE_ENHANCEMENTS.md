# Future Enhancements

This document tracks features that need to be implemented before future releases.

## Release 1.1.0 Requirements

### Toggle Mode Implementation

**Status**: Not Yet Implemented  
**Priority**: Must-have for 1.1.0  
**Reason**: Backward compatibility with existing applications

#### Overview

Toggle mode allows a dimmer to automatically ramp up and down between a minimum and maximum power level, creating a fading effect. This feature exists in the legacy implementation but has been temporarily removed from the event-driven architecture.

#### Current State

- API function `toggleSettings()` exists but only logs a warning
- `TOGGLE_MODE` enum value exists in header
- Toggle-related global variables (`togMin`, `togMax`, `togDir`) are declared but unused

#### Implementation Approach

**Option 1: FreeRTOS Task (Recommended)**

Create a separate FreeRTOS task that handles toggle mode updates:

```c
static void toggle_task(void *arg)
{
    while (1)
    {
        for (int i = 0; i < current_dim; i++)
        {
            if (dimMode[i] == TOGGLE_MODE)
            {
                // Update dimPulseBegin based on toggle direction
                if (dimPulseBegin[i] >= togMax[i])
                    togDir[i] = false;
                if (dimPulseBegin[i] <= togMin[i])
                    togDir[i] = true;
                    
                if (togDir[i])
                    dimPulseBegin[i]++;
                else
                    dimPulseBegin[i]--;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Adjust for desired toggle speed
    }
}
```

**Benefits:**
- Clean separation of concerns
- Doesn't burden ISR with toggle logic
- Easy to adjust toggle speed
- Can be paused/resumed

**Implementation Steps:**

1. Create toggle task in `begin()` function (only once, shared by all dimmers)
2. Task scans all dimmers for `TOGGLE_MODE`
3. Updates `dimPulseBegin[]` values periodically
4. Zero-crossing ISR reads updated values and schedules events accordingly

**Option 2: Timer-based Updates**

Use a separate software timer to update toggle values:

```c
static void toggle_timer_callback(void *arg)
{
    for (int i = 0; i < current_dim; i++)
    {
        if (dimMode[i] == TOGGLE_MODE)
        {
            // Same logic as Option 1
        }
    }
}
```

**Benefits:**
- Lighter weight than FreeRTOS task
- Precise timing control

**Drawbacks:**
- More complex timer management
- Another ISR to manage

#### Testing Requirements

Before 1.1.0 release, toggle mode must be tested for:

- [ ] Single dimmer in toggle mode
- [ ] Multiple dimmers in toggle mode with different ranges
- [ ] Switching between NORMAL_MODE and TOGGLE_MODE
- [ ] Toggle speed consistency
- [ ] No interference with normal mode dimmers
- [ ] Memory usage and CPU overhead acceptable

#### API Changes

No API changes needed - `toggleSettings()` signature remains the same:

```c
void toggleSettings(dimmertyp *ptr, int minValue, int maxValue);
```

#### Documentation Updates Needed

- Update README.md with toggle mode examples
- Update TRIAC_CYCLE.md with toggle mode timing diagrams
- Update IMPLEMENTATION_SUMMARY.md to remove "Not Yet Implemented" note
- Add toggle mode examples to examples directory

---

## Completed Enhancements

### One-Shot Timer Mode ✅

**Status**: ✅ Implemented (2026-01-25)  
**Priority**: High  
**Benefit**: 94-98% reduction in ISR invocations - ACHIEVED

The timer now uses one-shot mode with dynamic alarm scheduling. Timer only fires when events are scheduled.

#### Implementation Details

1. ✅ Timer configured with `auto_reload_on_alarm = false`
2. ✅ `set_next_alarm()` function dynamically schedules next event
3. ✅ Zero-crossing ISR sets alarm after scheduling events
4. ✅ Timer ISR sets alarm for next event after processing current events
5. ✅ Timer remains idle when no events are pending

#### Results

- Timer ISR frequency reduced from 10,000/sec to 200-600/sec
- 94-98% reduction in timer interrupts achieved
- Significantly lower CPU overhead
- Better power efficiency

---

## Future Optional Enhancements

### Priority Queue for Event Management

**Status**: Not Started  
**Priority**: Optimization  
**Benefit**: O(log n) instead of O(n) event processing

Current implementation uses linear search to find and process events. For small event counts (< 10), this is acceptable. For high dimmer counts or rapid updates, a priority queue would be more efficient.

#### Implementation Options

1. Min-heap implementation
2. Binary search tree
3. Third-party priority queue library

### Multi-Phase AC Support

**Status**: Design Concept Only  
**Priority**: Feature Request Dependent

Extend system to support 3-phase AC systems:
- Track phase relationships
- Independent zero-crossing detection per phase
- Phase-aware power distribution

### Power Factor Correction

**Status**: Research Phase  
**Priority**: Advanced Feature

Implement leading/trailing edge control for improved power factor.

### Energy Monitoring

**Status**: Concept  
**Priority**: Feature Request Dependent

Add real-time power consumption tracking:
- Integrate with current sensing
- Calculate RMS power
- Expose power data via API

---

## How to Update This Document

When implementing features:

1. Move feature from "Future" to "In Progress"
2. Update status and add implementation details
3. Link to relevant pull requests
4. After completion, move to "Completed" section with version number

When adding new features:

1. Add to appropriate release section
2. Include: Status, Priority, Overview, Implementation Approach
3. Note any API changes or breaking changes

---

**Last Updated**: 2026-01-25  
**Maintainer**: ESP32 Triac Dimmer Driver Team
