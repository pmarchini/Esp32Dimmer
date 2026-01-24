# Testing Guide for ESP32 Dimmer Driver

## Overview

This guide provides instructions for running and extending the unit tests for the ESP32 Dimmer Driver library.

## Test Coverage Summary

The unit test suite covers 16 test cases across the following categories:

### 1. Dimmer Creation (2 tests)
- Creating a single dimmer instance
- Creating multiple dimmer instances with different GPIO pins

### 2. Power Control (3 tests)
- Setting and getting power with normal values (0, 50, 99)
- Boundary value testing (values >99 are clamped)
- Power retrieval when dimmer is OFF

### 3. State Management (3 tests)
- Setting and getting ON/OFF state
- Toggling state with changeState()
- State's effect on getPower() return value

### 4. Mode Management (3 tests)
- Setting and getting NORMAL/TOGGLE modes
- Toggle settings with valid ranges
- Toggle settings with boundary values

### 5. Initialization (2 tests)
- Initialization with NORMAL_MODE
- Initialization with TOGGLE_MODE

### 6. Multiple Dimmers (1 test)
- Independent operation of multiple dimmers

### 7. Integration (2 tests)
- State changes affecting power reporting
- Complete workflow validation

## Prerequisites

### Software Requirements
- ESP-IDF v5.0 or higher
- Python 3.7 or higher (for ESP-IDF)
- CMake 3.16 or higher
- Appropriate compiler toolchain for your platform

### Hardware Requirements
- ESP32 development board (any variant: ESP32, ESP32-S2, ESP32-C3, etc.)
- USB cable for flashing and monitoring
- (Optional) Oscilloscope for hardware validation

## Installation

1. Install ESP-IDF v5.0 or higher following the [official guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)

2. Clone this repository:
   ```bash
   git clone https://github.com/pmarchini/Esp32Dimmer.git
   cd Esp32Dimmer
   ```

## Running Tests

### Quick Start

```bash
# Navigate to test application
cd test_app

# Set target (choose your ESP32 variant)
idf.py set-target esp32

# Build the tests
idf.py build

# Flash and monitor (replace with your serial port)
idf.py -p /dev/ttyUSB0 flash monitor
```

### Supported Targets

The tests can run on any ESP32 variant:
- ESP32 (default)
- ESP32-S2
- ESP32-C3
- ESP32-S3

To set a specific target:
```bash
idf.py set-target esp32s3
```

### Build Options

For verbose build output:
```bash
idf.py -v build
```

For clean build:
```bash
idf.py fullclean
idf.py build
```

## Understanding Test Output

### Successful Test Run

```
========================================
ESP32 Dimmer Driver Unit Tests
========================================

Running test_createDimmer_returns_valid_pointer...
PASS

Running test_createDimmer_with_different_pins...
PASS

[... more tests ...]

========================================
All tests completed!
========================================

16 Tests 0 Failures 0 Ignored
OK
```

### Failed Test Example

If a test fails, you'll see:
```
Running test_setPower_getPower_normal_values...
test_main.c:56:test_setPower_getPower_normal_values:FAIL: Expected 50 Was 0

16 Tests 1 Failures 0 Ignored
FAIL
```

## Writing New Tests

### Test Function Template

```c
void test_your_feature_description(void)
{
    // 1. Setup - Create dimmer and initialize
    dimmertyp *dimmer = createDimmer(TEST_TRIAC_GPIO, TEST_ZC_GPIO);
    TEST_ASSERT_NOT_NULL(dimmer);
    begin(dimmer, NORMAL_MODE, ON, 50);
    
    // 2. Execute - Perform the operation you're testing
    setPower(dimmer, 42);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // 3. Verify - Check the results
    TEST_ASSERT_EQUAL(42, getPower(dimmer));
    
    // 4. Cleanup (if needed)
    // Note: Library doesn't provide cleanup functions
}
```

### Unity Assertion Macros

Common assertions used in the tests:

```c
TEST_ASSERT_TRUE(condition)           // Verify condition is true
TEST_ASSERT_FALSE(condition)          // Verify condition is false
TEST_ASSERT_EQUAL(expected, actual)   // Verify values are equal
TEST_ASSERT_NOT_EQUAL(val1, val2)     // Verify values are different
TEST_ASSERT_NULL(pointer)             // Verify pointer is NULL
TEST_ASSERT_NOT_NULL(pointer)         // Verify pointer is not NULL
TEST_ASSERT_EQUAL_INT(exp, act)       // Compare integers
TEST_ASSERT_GREATER_THAN(threshold, actual) // Verify actual > threshold
TEST_ASSERT_LESS_THAN(threshold, actual)    // Verify actual < threshold
```

### Adding a New Test

1. Add test function to `test_app/main/test_main.c`:
   ```c
   void test_my_new_feature(void)
   {
       // Your test code here
   }
   ```

2. Register it in `app_main()`:
   ```c
   void app_main(void)
   {
       UNITY_BEGIN();
       
       // Existing tests...
       RUN_TEST(test_my_new_feature);  // Add this line
       
       UNITY_END();
   }
   ```

3. Rebuild and test:
   ```bash
   idf.py build flash monitor
   ```

## Test Best Practices

### 1. Test Independence
Each test should be independent and not rely on state from other tests.

### 2. Descriptive Names
Use clear, descriptive test names: `test_<component>_<scenario>`
- Good: `test_setPower_boundary_values`
- Bad: `test_power_1`

### 3. Test One Thing
Each test should verify one specific behavior.

### 4. Use Delays
When testing asynchronous operations, add small delays:
```c
setPower(dimmer, 50);
vTaskDelay(pdMS_TO_TICKS(10)); // Allow setting to take effect
TEST_ASSERT_EQUAL(50, getPower(dimmer));
```

### 5. Document Edge Cases
Add comments for non-obvious test cases:
```c
// Test that power values above 99 are clamped to 99
setPower(dimmer, 150);
TEST_ASSERT_EQUAL(99, getPower(dimmer));
```

## Continuous Integration

### GitHub Actions Example

Create `.github/workflows/test.yml`:

```yaml
name: ESP32 Dimmer Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Setup ESP-IDF
        uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v5.0
          target: esp32
      - name: Build Tests
        run: |
          cd test_app
          idf.py build
```

## Troubleshooting

### Build Errors

**Error: "No such file or directory" for esp32-triac-dimmer-driver.h**
- Solution: Ensure EXTRA_COMPONENT_DIRS is set correctly in test_app/CMakeLists.txt

**Error: "undefined reference to unity_*"**
- Solution: Ensure unity is in the REQUIRES list in main/CMakeLists.txt

### Runtime Issues

**Tests fail to start**
- Check serial port permissions: `sudo usermod -a -G dialout $USER`
- Verify correct serial port: `ls /dev/tty*`

**Random test failures**
- Hardware interference - ensure stable power supply
- Increase delay times in tests
- Check for race conditions in test logic

### Memory Issues

**Stack overflow in tests**
- Increase CONFIG_ESP_MAIN_TASK_STACK_SIZE in sdkconfig.defaults

**Heap allocation failures**
- Tests may exceed ALL_DIMMERS limit (50)
- Reset the board between test runs

## Hardware Validation Testing

While unit tests validate software behavior, hardware testing validates actual dimmer operation:

### Setup for Hardware Tests
1. Connect zero-crossing detector to GPIO_NUM_21
2. Connect TRIAC outputs to GPIO_NUM_22 and GPIO_NUM_23
3. Connect dimmable AC load (e.g., incandescent bulb)
4. Use oscilloscope to monitor TRIAC firing pulses

### Safety Warnings
⚠️ **AC VOLTAGE HAZARD** ⚠️
- Only qualified personnel should work with AC circuits
- Ensure proper isolation and grounding
- Use current-limited test circuits
- Never touch circuits while powered

## Future Enhancements

Potential areas for test expansion:

1. **Performance Tests**
   - Measure ISR timing
   - Verify zero-crossing detection accuracy
   - Test rapid power changes

2. **Stress Tests**
   - Maximum dimmer count (50 dimmers)
   - Continuous operation for extended periods
   - Rapid state/mode changes

3. **Hardware-in-the-Loop Tests**
   - Actual zero-crossing signal simulation
   - TRIAC timing verification
   - Multi-frequency testing (50Hz/60Hz)

4. **Error Handling Tests**
   - Invalid GPIO pins
   - Null pointer handling
   - Resource exhaustion scenarios

## References

- [ESP-IDF Unit Testing Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/unit-tests.html)
- [Unity Testing Framework](https://github.com/ThrowTheSwitch/Unity)
- [ESP32 GPIO Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html)

## Support

For questions or issues:
- Open an issue on [GitHub](https://github.com/pmarchini/Esp32Dimmer/issues)
- Check existing issues and discussions
- Review the test code for examples
