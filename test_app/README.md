# ESP32 Dimmer Driver Unit Tests

This directory contains unit tests for the ESP32 Dimmer Driver library.

## Test Structure

The tests are organized using the ESP-IDF Unity testing framework and cover the following functionality:

### Test Categories

1. **Dimmer Creation Tests**
   - `test_createDimmer_returns_valid_pointer`: Verifies that createDimmer returns a valid pointer
   - `test_createDimmer_with_different_pins`: Tests creating multiple dimmers with different GPIO pins

2. **Power Control Tests**
   - `test_setPower_getPower_normal_values`: Tests setting and getting power with normal values (0, 50, 99)
   - `test_setPower_boundary_values`: Tests boundary conditions (values above 99 are clamped)
   - `test_getPower_when_dimmer_off`: Verifies getPower returns 0 when dimmer is OFF

3. **State Management Tests**
   - `test_setState_getState`: Tests setting and getting the dimmer state (ON/OFF)
   - `test_changeState_toggles_state`: Verifies changeState toggles between ON and OFF
   - `test_state_affects_getPower`: Tests that state changes affect getPower behavior

4. **Mode Management Tests**
   - `test_setMode_getMode`: Tests setting and getting dimmer modes (NORMAL/TOGGLE)
   - `test_toggleSettings_with_valid_values`: Tests toggle settings with valid range
   - `test_toggleSettings_boundary_values`: Tests toggle settings with boundary values

5. **Initialization Tests**
   - `test_begin_with_normal_mode`: Tests initialization with NORMAL_MODE
   - `test_begin_with_toggle_mode`: Tests initialization with TOGGLE_MODE

6. **Multiple Dimmer Tests**
   - `test_multiple_dimmers_independent`: Verifies multiple dimmers operate independently

## Running the Tests

### Prerequisites

- ESP-IDF v5.0 or higher installed
- ESP32 development board (tests can run without actual hardware connected)

### Validate Your Setup

Before running tests, you can validate your environment:

```bash
./validate_setup.sh
```

This will check:
- Python and ESP-IDF installation
- Required files and directories
- Serial port availability
- CMake installation

### Build and Flash

1. Navigate to the test_app directory:
   ```bash
   cd test_app
   ```

2. Set the target (if not already set):
   ```bash
   idf.py set-target esp32
   ```

3. Build the test application:
   ```bash
   idf.py build
   ```

4. Flash to your ESP32 board:
   ```bash
   idf.py -p /dev/ttyUSB0 flash monitor
   ```
   Replace `/dev/ttyUSB0` with your serial port.

### Expected Output

When the tests run successfully, you should see output similar to:

```
========================================
ESP32 Dimmer Driver Unit Tests
========================================

Running test_createDimmer_returns_valid_pointer...
PASS

Running test_createDimmer_with_different_pins...
PASS

...

========================================
All tests completed!
========================================

16 Tests 0 Failures 0 Ignored
OK
```

## Test Coverage

The unit tests cover:

- ✅ Dimmer object creation
- ✅ Power setting and retrieval with boundary conditions
- ✅ State management (ON/OFF)
- ✅ Mode management (NORMAL/TOGGLE)
- ✅ Toggle settings configuration
- ✅ Multiple independent dimmers
- ✅ State-dependent behavior

## Notes for Future Refactoring

These unit tests are designed to:

1. **Enable safe refactoring**: The tests verify the API contract, allowing you to refactor internal implementation while ensuring the public API behavior remains consistent.

2. **Document expected behavior**: Each test serves as executable documentation of how the library should behave.

3. **Catch regressions**: Running these tests after changes helps catch any unintended behavior changes.

4. **Hardware independence**: These tests focus on the software logic and can run without actual dimmer hardware connected. They test the API layer and state management.

## Limitations

- These tests do not verify actual hardware functionality (zero-crossing detection, TRIAC firing)
- ISR behavior is not directly tested (requires hardware/simulation)
- Timing-critical code paths are not fully tested without hardware

For hardware integration testing, additional tests would be needed with actual hardware or simulation.

## Adding New Tests

To add new tests:

1. Add new test functions to `main/test_main.c`
2. Use the Unity assertion macros (TEST_ASSERT_*, etc.)
3. Register the test with `RUN_TEST()` in the `app_main()` function
4. Follow the naming convention: `test_<functionality>_<scenario>`

Example:
```c
void test_new_feature_basic_functionality(void)
{
    dimmertyp *dimmer = createDimmer(TEST_TRIAC_GPIO, TEST_ZC_GPIO);
    TEST_ASSERT_NOT_NULL(dimmer);
    
    // Your test code here
}
```

Then add to `app_main()`:
```c
RUN_TEST(test_new_feature_basic_functionality);
```
