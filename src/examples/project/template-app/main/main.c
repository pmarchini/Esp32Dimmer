#include "esp32-triac-dimmer-driver.h"

#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "dimmer_demo";

// Dimmer pointer
dimmertyp *ptr_dimmer;
dimmertyp *ptr_dimmer_2;

volatile bool _init_done;
bool flag;

#define INIT_NOT_DONE _init_done == false
#define _50Hz 50

#define ZEROCROSS_GPIO GPIO_NUM_21
#define TRIAC_1_GPIO GPIO_NUM_22
#define TRIAC_2_GPIO GPIO_NUM_26
#define DIAGNOSTIC_LED_GPIO GPIO_NUM_17

void init();

void app_main()
{
    if (INIT_NOT_DONE)
    {
        init();
        // Initial point;
        setPower(ptr_dimmer, 1);
    }

    while (1)
    {
        // change the output power
        getPower(ptr_dimmer) < 50 ? setPower(ptr_dimmer, (getPower(ptr_dimmer) + 1)) : setPower(ptr_dimmer, 20);
        setPower(ptr_dimmer_2, getPower(ptr_dimmer));
        // wait
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void init()
{
    ESP_LOGI(TAG, "Starting init sequence");
    // Set diagnostic leds
    gpio_set_direction(DIAGNOSTIC_LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(DIAGNOSTIC_LED_GPIO, 1);
    // Instantiate the new dimmer
    ptr_dimmer = createDimmer(TRIAC_1_GPIO, ZEROCROSS_GPIO);
    ptr_dimmer_2 = createDimmer(TRIAC_2_GPIO, ZEROCROSS_GPIO);
    // startup
    begin(ptr_dimmer, NORMAL_MODE, ON, _50Hz);
    begin(ptr_dimmer_2, NORMAL_MODE, ON, _50Hz);
    ESP_LOGI(TAG, "Init sequence completed");
}