#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "esp32-triac-dimmer-driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

// Test GPIO pins (using safe GPIO pins for testing)
#define TEST_TRIAC_GPIO GPIO_NUM_22
#define TEST_TRIAC_GPIO_2 GPIO_NUM_23
#define TEST_ZC_GPIO GPIO_NUM_21

// External variable to reset dimmer count for testing
extern int current_dim;

void setUp(void)
{
    // This is run before each test
}

void tearDown(void)
{
    // This is run after each test
    // Note: We cannot easily reset the dimmer state between tests due to
    // hardware initialization, so tests should be designed to be independent
}

// Test: createDimmer function creates a valid dimmer object
void test_createDimmer_returns_valid_pointer(void)
{
    dimmertyp *dimmer = createDimmer(TEST_TRIAC_GPIO, TEST_ZC_GPIO);
    
    TEST_ASSERT_NOT_NULL(dimmer);
}

// Test: createDimmer with different GPIO pins
void test_createDimmer_with_different_pins(void)
{
    dimmertyp *dimmer1 = createDimmer(TEST_TRIAC_GPIO, TEST_ZC_GPIO);
    dimmertyp *dimmer2 = createDimmer(TEST_TRIAC_GPIO_2, TEST_ZC_GPIO);
    
    TEST_ASSERT_NOT_NULL(dimmer1);
    TEST_ASSERT_NOT_NULL(dimmer2);
    TEST_ASSERT_NOT_EQUAL(dimmer1, dimmer2);
}

// Test: setPower and getPower functions
void test_setPower_getPower_normal_values(void)
{
    dimmertyp *dimmer = createDimmer(TEST_TRIAC_GPIO, TEST_ZC_GPIO);
    TEST_ASSERT_NOT_NULL(dimmer);
    
    // Initialize the dimmer
    begin(dimmer, NORMAL_MODE, ON, 50);
    
    // Test setting power to 50
    setPower(dimmer, 50);
    vTaskDelay(pdMS_TO_TICKS(10)); // Allow time for setting
    TEST_ASSERT_EQUAL(50, getPower(dimmer));
    
    // Test setting power to 0
    setPower(dimmer, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    TEST_ASSERT_EQUAL(0, getPower(dimmer));
    
    // Test setting power to 99 (max valid value)
    setPower(dimmer, 99);
    vTaskDelay(pdMS_TO_TICKS(10));
    TEST_ASSERT_EQUAL(99, getPower(dimmer));
}

// Test: setPower with boundary values
void test_setPower_boundary_values(void)
{
    dimmertyp *dimmer = createDimmer(TEST_TRIAC_GPIO, TEST_ZC_GPIO);
    TEST_ASSERT_NOT_NULL(dimmer);
    
    begin(dimmer, NORMAL_MODE, ON, 50);
    
    // Test setting power above maximum (should be clamped to 99)
    setPower(dimmer, 100);
    vTaskDelay(pdMS_TO_TICKS(10));
    TEST_ASSERT_EQUAL(99, getPower(dimmer));
    
    // Test setting power way above maximum
    setPower(dimmer, 150);
    vTaskDelay(pdMS_TO_TICKS(10));
    TEST_ASSERT_EQUAL(99, getPower(dimmer));
}

// Test: getPower returns 0 when dimmer is OFF
void test_getPower_when_dimmer_off(void)
{
    dimmertyp *dimmer = createDimmer(TEST_TRIAC_GPIO, TEST_ZC_GPIO);
    TEST_ASSERT_NOT_NULL(dimmer);
    
    begin(dimmer, NORMAL_MODE, OFF, 50);
    
    setPower(dimmer, 50);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // When dimmer is OFF, getPower should return 0
    TEST_ASSERT_EQUAL(0, getPower(dimmer));
}

// Test: setState and getState functions
void test_setState_getState(void)
{
    dimmertyp *dimmer = createDimmer(TEST_TRIAC_GPIO, TEST_ZC_GPIO);
    TEST_ASSERT_NOT_NULL(dimmer);
    
    begin(dimmer, NORMAL_MODE, ON, 50);
    
    // Test setting state to ON
    setState(dimmer, ON);
    TEST_ASSERT_TRUE(getState(dimmer));
    
    // Test setting state to OFF
    setState(dimmer, OFF);
    TEST_ASSERT_FALSE(getState(dimmer));
}

// Test: changeState function toggles state
void test_changeState_toggles_state(void)
{
    dimmertyp *dimmer = createDimmer(TEST_TRIAC_GPIO, TEST_ZC_GPIO);
    TEST_ASSERT_NOT_NULL(dimmer);
    
    begin(dimmer, NORMAL_MODE, ON, 50);
    
    // Initial state is ON
    TEST_ASSERT_TRUE(getState(dimmer));
    
    // Change state should toggle to OFF
    changeState(dimmer);
    TEST_ASSERT_FALSE(getState(dimmer));
    
    // Change state again should toggle back to ON
    changeState(dimmer);
    TEST_ASSERT_TRUE(getState(dimmer));
}

// Test: setMode and getMode functions
void test_setMode_getMode(void)
{
    dimmertyp *dimmer = createDimmer(TEST_TRIAC_GPIO, TEST_ZC_GPIO);
    TEST_ASSERT_NOT_NULL(dimmer);
    
    begin(dimmer, NORMAL_MODE, ON, 50);
    
    // Test setting to NORMAL_MODE
    setMode(dimmer, NORMAL_MODE);
    TEST_ASSERT_EQUAL(NORMAL_MODE, getMode(dimmer));
    
    // Test setting to TOGGLE_MODE
    setMode(dimmer, TOGGLE_MODE);
    TEST_ASSERT_EQUAL(TOGGLE_MODE, getMode(dimmer));
}

// Test: toggleSettings function
void test_toggleSettings_with_valid_values(void)
{
    dimmertyp *dimmer = createDimmer(TEST_TRIAC_GPIO, TEST_ZC_GPIO);
    TEST_ASSERT_NOT_NULL(dimmer);
    
    begin(dimmer, NORMAL_MODE, ON, 50);
    
    // Test toggle settings with valid range
    toggleSettings(dimmer, 10, 90);
    
    // After toggleSettings, mode should be TOGGLE_MODE
    TEST_ASSERT_EQUAL(TOGGLE_MODE, getMode(dimmer));
}

// Test: toggleSettings with boundary values
void test_toggleSettings_boundary_values(void)
{
    dimmertyp *dimmer = createDimmer(TEST_TRIAC_GPIO, TEST_ZC_GPIO);
    TEST_ASSERT_NOT_NULL(dimmer);
    
    begin(dimmer, NORMAL_MODE, ON, 50);
    
    // Test toggle settings with max value above 99 (should be clamped)
    toggleSettings(dimmer, 5, 100);
    TEST_ASSERT_EQUAL(TOGGLE_MODE, getMode(dimmer));
    
    // Test toggle settings with min value below 1 (should be clamped)
    toggleSettings(dimmer, 0, 80);
    TEST_ASSERT_EQUAL(TOGGLE_MODE, getMode(dimmer));
}

// Test: begin function with NORMAL_MODE
void test_begin_with_normal_mode(void)
{
    dimmertyp *dimmer = createDimmer(TEST_TRIAC_GPIO, TEST_ZC_GPIO);
    TEST_ASSERT_NOT_NULL(dimmer);
    
    // Begin with NORMAL_MODE, ON state, 50Hz
    begin(dimmer, NORMAL_MODE, ON, 50);
    
    TEST_ASSERT_EQUAL(NORMAL_MODE, getMode(dimmer));
    TEST_ASSERT_TRUE(getState(dimmer));
}

// Test: begin function with TOGGLE_MODE
void test_begin_with_toggle_mode(void)
{
    dimmertyp *dimmer = createDimmer(TEST_TRIAC_GPIO, TEST_ZC_GPIO);
    TEST_ASSERT_NOT_NULL(dimmer);
    
    // Begin with TOGGLE_MODE, OFF state, 60Hz
    begin(dimmer, TOGGLE_MODE, OFF, 60);
    
    TEST_ASSERT_EQUAL(TOGGLE_MODE, getMode(dimmer));
    TEST_ASSERT_FALSE(getState(dimmer));
}

// Test: Multiple dimmers can be created and managed independently
void test_multiple_dimmers_independent(void)
{
    dimmertyp *dimmer1 = createDimmer(TEST_TRIAC_GPIO, TEST_ZC_GPIO);
    dimmertyp *dimmer2 = createDimmer(TEST_TRIAC_GPIO_2, TEST_ZC_GPIO);
    
    TEST_ASSERT_NOT_NULL(dimmer1);
    TEST_ASSERT_NOT_NULL(dimmer2);
    
    begin(dimmer1, NORMAL_MODE, ON, 50);
    begin(dimmer2, TOGGLE_MODE, OFF, 50);
    
    setPower(dimmer1, 30);
    setPower(dimmer2, 70);
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Verify each dimmer maintains independent state
    TEST_ASSERT_EQUAL(NORMAL_MODE, getMode(dimmer1));
    TEST_ASSERT_EQUAL(TOGGLE_MODE, getMode(dimmer2));
    
    TEST_ASSERT_EQUAL(30, getPower(dimmer1));
    TEST_ASSERT_EQUAL(0, getPower(dimmer2)); // dimmer2 is OFF, so getPower returns 0
    
    TEST_ASSERT_TRUE(getState(dimmer1));
    TEST_ASSERT_FALSE(getState(dimmer2));
}

// Test: State changes affect getPower behavior
void test_state_affects_getPower(void)
{
    dimmertyp *dimmer = createDimmer(TEST_TRIAC_GPIO, TEST_ZC_GPIO);
    TEST_ASSERT_NOT_NULL(dimmer);
    
    begin(dimmer, NORMAL_MODE, ON, 50);
    
    // Set power and verify when ON
    setPower(dimmer, 75);
    vTaskDelay(pdMS_TO_TICKS(10));
    TEST_ASSERT_EQUAL(75, getPower(dimmer));
    
    // Turn OFF and verify getPower returns 0
    setState(dimmer, OFF);
    TEST_ASSERT_EQUAL(0, getPower(dimmer));
    
    // Turn back ON and verify power is still 75
    setState(dimmer, ON);
    TEST_ASSERT_EQUAL(75, getPower(dimmer));
}

// Main test runner
void app_main(void)
{
    printf("\n\n");
    printf("========================================\n");
    printf("ESP32 Dimmer Driver Unit Tests\n");
    printf("========================================\n\n");
    
    UNITY_BEGIN();
    
    // Dimmer creation tests
    RUN_TEST(test_createDimmer_returns_valid_pointer);
    RUN_TEST(test_createDimmer_with_different_pins);
    
    // Power control tests
    RUN_TEST(test_setPower_getPower_normal_values);
    RUN_TEST(test_setPower_boundary_values);
    RUN_TEST(test_getPower_when_dimmer_off);
    
    // State management tests
    RUN_TEST(test_setState_getState);
    RUN_TEST(test_changeState_toggles_state);
    RUN_TEST(test_state_affects_getPower);
    
    // Mode management tests
    RUN_TEST(test_setMode_getMode);
    RUN_TEST(test_toggleSettings_with_valid_values);
    RUN_TEST(test_toggleSettings_boundary_values);
    
    // Initialization tests
    RUN_TEST(test_begin_with_normal_mode);
    RUN_TEST(test_begin_with_toggle_mode);
    
    // Multiple dimmer tests
    RUN_TEST(test_multiple_dimmers_independent);
    
    UNITY_END();
    
    printf("\n========================================\n");
    printf("All tests completed!\n");
    printf("========================================\n\n");
    
    // Keep the app running
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
