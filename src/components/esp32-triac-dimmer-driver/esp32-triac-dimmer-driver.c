#include "esp32-triac-dimmer-driver.h"

static const char *TAG = "Esp32idfDimmer";

int pulseWidth = 2;
volatile int current_dim = 0;
int all_dim = 3;
int rise_fall = true;
char user_zero_cross = '0';
int debug_signal_zc = 0;
bool flagDebug = false;

volatile bool _initDone = false;
volatile int _steps = 0;

static dimmertyp *dimmer[ALL_DIMMERS];
volatile bool firstSetup = false;
volatile uint16_t dimPower[ALL_DIMMERS];
volatile gpio_num_t dimOutPin[ALL_DIMMERS];
volatile gpio_num_t dimZCPin[ALL_DIMMERS];
volatile DIMMER_MODE_typedef dimMode[ALL_DIMMERS];
volatile ON_OFF_typedef dimState[ALL_DIMMERS];
static uint16_t dimPulseBegin[ALL_DIMMERS];
volatile uint16_t togMax[ALL_DIMMERS];
volatile uint16_t togMin[ALL_DIMMERS];
volatile bool togDir[ALL_DIMMERS];

/* timer configurations */
gptimer_config_t m_timer_config;
gptimer_handle_t gptimer = NULL;
typedef struct {
    uint64_t event_count;
} example_queue_element_t;

/* Event queue for single-fire timer implementation */
static timer_event_t event_queue[MAX_TIMER_EVENTS];
static volatile int event_queue_size = 0;
static uint64_t alarm_interval_ticks = 0;     // Calculated based on AC frequency (set by config_alarm)
static uint64_t pulse_width_ticks = 0;        // Pre-calculated pulse width in ticks (set by config_alarm)


dimmertyp *createDimmer(gpio_num_t user_dimmer_pin, gpio_num_t zc_dimmer_pin)
{
	if (current_dim >= ALL_DIMMERS)
	{
		return NULL;
	}
	current_dim++;
	dimmer[current_dim - 1] = malloc(sizeof(dimmertyp));
	dimmer[current_dim - 1]->current_num = current_dim - 1;
	dimmer[current_dim - 1]->toggle_state = false;

	dimPulseBegin[current_dim - 1] = 1;
	dimOutPin[current_dim - 1] = user_dimmer_pin;
	dimZCPin[current_dim - 1] = zc_dimmer_pin;
	dimMode[current_dim - 1] = NORMAL_MODE;
	togMin[current_dim - 1] = 0;
	togMax[current_dim - 1] = 1;
	// Return the pointer
	return dimmer[current_dim - 1];
}

/**
 * @brief Initialize the event queue
 */
static void init_event_queue(void)
{
	for (int i = 0; i < MAX_TIMER_EVENTS; i++)
	{
		event_queue[i].active = false;
		event_queue[i].timestamp = 0;
		event_queue[i].dimmer_id = 0;
		event_queue[i].event_type = EVENT_FIRE_TRIAC;
	}
	event_queue_size = 0;
}

/**
 * @brief Find the next event in queue (earliest timestamp)
 * @return Index of next event, or -1 if queue is empty
 */
static int find_next_event_index(void)
{
	int next_idx = -1;
	uint64_t earliest_time = UINT64_MAX;
	
	for (int i = 0; i < MAX_TIMER_EVENTS; i++)
	{
		if (event_queue[i].active && event_queue[i].timestamp < earliest_time)
		{
			earliest_time = event_queue[i].timestamp;
			next_idx = i;
		}
	}
	
	return next_idx;
}

/**
 * @brief Schedule a timer event
 * @param timestamp Absolute timestamp when event should occur
 * @param dimmer_id Which dimmer this event affects
 * @param event_type Type of event (fire or end pulse)
 * @return true if event was scheduled, false if queue is full or invalid input
 */
static bool schedule_timer_event(uint64_t timestamp, uint8_t dimmer_id, timer_event_type_t event_type)
{
	// Validate dimmer_id to prevent array bounds violations
	if (dimmer_id >= ALL_DIMMERS)
	{
		ESP_LOGE(TAG, "Invalid dimmer_id: %d (max: %d)", dimmer_id, ALL_DIMMERS - 1);
		return false;
	}
	
	// Validate event_type
	if (event_type != EVENT_FIRE_TRIAC && event_type != EVENT_END_PULSE)
	{
		ESP_LOGE(TAG, "Invalid event_type: %d", event_type);
		return false;
	}
	
	// Find an empty slot
	for (int i = 0; i < MAX_TIMER_EVENTS; i++)
	{
		if (!event_queue[i].active)
		{
			event_queue[i].timestamp = timestamp;
			event_queue[i].dimmer_id = dimmer_id;
			event_queue[i].event_type = event_type;
			event_queue[i].active = true;
			event_queue_size++;
			return true;
		}
	}
	
	// Queue is full
	ESP_LOGE(TAG, "Event queue full!");
	return false;
}

/**
 * @brief Remove an event from the queue
 * @param index Index of event to remove
 */
static void remove_event(int index)
{
	if (index >= 0 && index < MAX_TIMER_EVENTS && event_queue[index].active)
	{
		event_queue[index].active = false;
		event_queue_size--;
	}
}

/**
 * @brief Set the alarm for the next event in the queue
 * Called from ISR context to schedule the next timer interrupt
 */
static void IRAM_ATTR set_next_alarm(void)
{
	int next_idx = find_next_event_index();
	
	if (next_idx >= 0)
	{
		uint64_t current_time = 0;
		gptimer_get_raw_count(gptimer, &current_time);
		
		uint64_t next_event_time = event_queue[next_idx].timestamp;
		
		// If the event is in the past or very soon, fire immediately
		if (next_event_time <= current_time)
		{
			next_event_time = current_time + 1;  // Fire on next tick
		}
		
		// Set one-shot alarm for the next event
		gptimer_alarm_config_t alarm_config = {
			.alarm_count = next_event_time,
			.flags.auto_reload_on_alarm = false  // One-shot mode
		};
		gptimer_set_alarm_action(gptimer, &alarm_config);
	}
	// If no events, timer will remain idle until next zero-crossing
}

#define TIMER_BASE_CLK 1 * 1000 * 1000,              // 1MHz, 1 tick = 1us

/**
 * @brief Configure the timer alarm
 */
void config_alarm(gptimer_handle_t *timer, int ACfreq) 
{   
    /*self regulation 50/60 Hz*/
	double m_calculated_interval = (1 / (double)(ACfreq * 2)) / 100;
    ESP_LOGI(TAG, "Interval between wave calculated for frequency : %3dHz = %5f", ACfreq, m_calculated_interval);
	
	// Store the interval in ticks for use in event scheduling
	alarm_interval_ticks = (uint64_t)(1000000 * m_calculated_interval);
	ESP_LOGI(TAG, "Timer interval in ticks: %llu", alarm_interval_ticks);
	
	// Pre-calculate pulse width in ticks for ISR efficiency
	pulse_width_ticks = (uint64_t)pulseWidth * alarm_interval_ticks;
	ESP_LOGI(TAG, "Pulse width in ticks: %llu", pulse_width_ticks);

    ESP_LOGI(TAG, "Timer configuration - configure interrupt and timer");
    ESP_LOGI(TAG, "Timer configuration - configure alarm");
    // Initial alarm config - will be set dynamically by set_next_alarm()
    // Starting with a large value so timer doesn't fire until first event is scheduled
    gptimer_alarm_config_t alarm_config = {
    .reload_count = 0,
    .alarm_count = UINT64_MAX,  // Will be updated when first event is scheduled
    .flags.auto_reload_on_alarm = false, // One-shot mode - alarm set dynamically per event
    };
    ESP_LOGI(TAG, "Timer configuration - set alarm action");
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

    gptimer_event_callbacks_t cbs = {
    .on_alarm = onTimerISR, // register user callback
    };
    ESP_LOGI(TAG, "Timer configuration - register event callbacks");
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));

    ESP_LOGI(TAG, "Timer configuration - configuration completed");
}

void config_timer(int ACfreq)
{
	ESP_LOGI(TAG, "Timer configuration - start");
	/*System timer startup has been done*/
	if (_initDone)
	{
		ESP_LOGW(TAG, "Timer configuration - timer already configured");
		return;
	}

	memset(&m_timer_config, 0, sizeof(m_timer_config));
	
	/* Initialize event queue */
	init_event_queue();
	ESP_LOGI(TAG, "Event queue initialized");

	/* Prepare configuration */
    gptimer_config_t m_timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = TIMER_BASE_CLK
    };

	ESP_LOGI(TAG, "Timer configuration - configure interrupt and timer");
	/* Configure the alarm value and the interrupt on alarm. */
	ESP_ERROR_CHECK(gptimer_new_timer(&m_timer_config, &gptimer));

    /* Configure the alarm value and the interrupt on alarm. */
    config_alarm(gptimer, ACfreq);
	/* start timer */
	ESP_LOGI(TAG, "Timer configuration - start timer");
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));
	ESP_LOGI(TAG, "Timer configuration - completed");
}

/*Zero-crossing pin setting
 *set as input
 *set as pullup
 *set its interrupt*/
void ext_int_init(dimmertyp *ptr)
{
	ESP_LOGI(TAG, "Setting ZCPin : %3d as input", dimZCPin[ptr->current_num]);
	ESP_LOGI(TAG, "Checking for previous declaration of zc input on the same gpio");
	/*Zero crossing*/
	bool alreadyInit = false;
	for (int i = 0; i < ptr->current_num; i++)
	{
		if (dimZCPin[i] == dimZCPin[ptr->current_num])
		{
			alreadyInit = true;
		}
	}
	ESP_LOGI(TAG, "Already init = %3d", alreadyInit);
	if (!alreadyInit)
	{
		gpio_set_direction(dimZCPin[ptr->current_num], GPIO_MODE_INPUT);
		gpio_set_intr_type(dimZCPin[ptr->current_num], GPIO_INTR_NEGEDGE);
		gpio_intr_enable(dimZCPin[ptr->current_num]);
		gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
		gpio_isr_handler_add(dimZCPin[ptr->current_num], isr_ext, (void *)dimZCPin[ptr->current_num]);
	}
	ESP_LOGI(TAG, "Zero Cross interrupt configuration - completed");
	/*TRIAC command - configuration*/
	ESP_LOGI(TAG, "Triac command configuration");

	gpio_set_direction(dimOutPin[ptr->current_num], GPIO_MODE_OUTPUT);
	ESP_LOGI(TAG, "Triac command configuration - completed");
}

/*ISR debug region*/
#if DEBUG_ZERO_CROSS_SIGNAL == ISR_DEBUG_ON

static QueueHandle_t gpio_zero_cross_evt_queue = NULL;

static void gpio_isr_zerocross_debug(void *arg)
{
	uint32_t io_num;
	for (;;)
	{
		if (xQueueReceive(gpio_zero_cross_evt_queue, &io_num, portMAX_DELAY))
		{
			ESP_LOGI(TAG, "Zero-cross trigger fired");
		}
	}
}

#endif

/*ISR timer debug region*/
#if DEBUG_ISR_TIMER == ISR_DEBUG_ON

static QueueHandle_t timer_event_queue = NULL;

static void timer_isr_debug(void *arg)
{
	uint32_t io_num;
	for (;;)
	{
		if (xQueueReceive(timer_event_queue, &io_num, portMAX_DELAY))
		{
			printf("Timer interrupt event , counter = %5lu \n", io_num);
		}
	}
}

#endif

void begin(dimmertyp *ptr, DIMMER_MODE_typedef DIMMER_MODE, ON_OFF_typedef ON_OFF, int FREQ)
{
	ESP_LOGI(TAG, "Dimmer - begin");
	dimMode[ptr->current_num] = DIMMER_MODE;
	dimState[ptr->current_num] = ON_OFF;

#if DEBUG_ZERO_CROSS_SIGNAL == ISR_DEBUG_ON
	if (!_initDone)
	{
		// create a queue to handle gpio event from isr
		gpio_zero_cross_evt_queue = xQueueCreate(10, sizeof(uint32_t));
		// start gpio task
		xTaskCreate(gpio_isr_zerocross_debug, "gpio_isr_debug", 2048, NULL, 10, NULL);
	}
#endif

#if DEBUG_ISR_TIMER == ISR_DEBUG_ON
	if (!_initDone)
	{
		// create a queue to handle timer event
		timer_event_queue = xQueueCreate(10, sizeof(uint32_t));
		// start gpio task
		xTaskCreate(timer_isr_debug, "timer_isr_debug", 2048, NULL, 10, NULL);
	}
#endif

	config_timer(FREQ);
	ext_int_init(ptr);

	// init completed
	_initDone = true;
	ESP_LOGI(TAG, "Dimmer begin - completed");
}

void setPower(dimmertyp *ptr, int power)
{
	if (power >= 99)
	{
		power = 99;
	}
	dimPower[ptr->current_num] = power;
	dimPulseBegin[ptr->current_num] = powerBuf[power];

	vTaskDelay(1);
}

int getPower(dimmertyp *ptr)
{
	if (dimState[ptr->current_num] == ON)
		return dimPower[ptr->current_num];
	else
		return 0;
}

void setState(dimmertyp *ptr, ON_OFF_typedef ON_OFF)
{
	dimState[ptr->current_num] = ON_OFF;
}

bool getState(dimmertyp *ptr)
{
	bool ret;
	if (dimState[ptr->current_num] == ON)
		ret = true;
	else
		ret = false;
	return ret;
}

void changeState(dimmertyp *ptr)
{
	if (dimState[ptr->current_num] == ON)
		dimState[ptr->current_num] = OFF;
	else
		dimState[ptr->current_num] = ON;
}

DIMMER_MODE_typedef getMode(dimmertyp *ptr)
{
	return dimMode[ptr->current_num];
}

void setMode(dimmertyp *ptr, DIMMER_MODE_typedef DIMMER_MODE)
{
	dimMode[ptr->current_num] = DIMMER_MODE;
}

void toggleSettings(dimmertyp *ptr, int minValue, int maxValue)
{
	// TODO: Toggle mode not yet implemented in event-driven architecture
	// This will be implemented before release 1.1.0
	// See FUTURE_ENHANCEMENTS.md for implementation plan
	ESP_LOGW(TAG, "toggleSettings called but TOGGLE_MODE is not yet implemented in this version");
	ESP_LOGW(TAG, "Toggle mode will be available in release 1.1.0");
	
	// Store the settings for future use, but mode won't function yet
	if (maxValue > 99)
	{
		maxValue = 99;
	}
	if (minValue < 1)
	{
		minValue = 1;
	}
	togMin[ptr->current_num] = powerBuf[maxValue];
	togMax[ptr->current_num] = powerBuf[minValue];
}

static void IRAM_ATTR isr_ext(void *arg)
{

#if DEBUG_ZERO_CROSS_SIGNAL == ISR_DEBUG_ON
	uint32_t gpio_num = (uint32_t)arg;
	xQueueSendFromISR(gpio_zero_cross_evt_queue, &gpio_num, NULL);
#endif

	// Get current timer count
	uint64_t zc_time = 0;
	gptimer_get_raw_count(gptimer, &zc_time);
	
	for (int i = 0; i < current_dim; i++)
	{
		if (dimState[i] == ON)
		{
			// Calculate the exact time to fire the triac
			// fire_time = current_time + (dimPulseBegin[i] * interval_per_step)
			uint64_t fire_delay = (uint64_t)dimPulseBegin[i] * alarm_interval_ticks;
			uint64_t fire_time = zc_time + fire_delay;
			
			// Schedule the fire event
			schedule_timer_event(fire_time, i, EVENT_FIRE_TRIAC);
		}
	}
	
	// Set alarm for the next scheduled event
	set_next_alarm();
}

#if DEBUG_ISR_TIMER == ISR_DEBUG_ON
static int counter = 0;
#endif
/* Execution on timer event */
static void IRAM_ATTR onTimerISR(void *para)
{
/**********************************/
#if DEBUG_ISR_TIMER == ISR_DEBUG_ON
	counter++;
	uint32_t info = (uint32_t)counter;
	xQueueSendFromISR(timer_event_queue, &info, NULL);
#endif

	// Get current timer count
	uint64_t current_time = 0;
	gptimer_get_raw_count(gptimer, &current_time);
	
	// Process all events that should fire at or before current time
	// Note: We scan through all active events once - O(n) where n = event_queue_size
	// Early termination after processing all active events avoids scanning empty slots
	// For small queue sizes (typically < 10 active events), this is more efficient
	// than maintaining a separate active events list or priority queue
	int processed_events = 0;
	for (int i = 0; i < MAX_TIMER_EVENTS && processed_events < event_queue_size; i++)
	{
		if (!event_queue[i].active)
			continue;
		
		processed_events++;  // Count this active event
			
		if (event_queue[i].timestamp > current_time)
			continue;
		
		timer_event_t *event = &event_queue[i];
		
		// Validate dimmer_id to prevent array bounds violations
		if (event->dimmer_id < ALL_DIMMERS)
		{
			if (event->event_type == EVENT_FIRE_TRIAC)
			{
				// Fire the triac
				gpio_set_level(dimOutPin[event->dimmer_id], 1);
				
				// Schedule pulse end event based on the event's original fire time
				// to maintain precise pulse width
				uint64_t pulse_end_time = event->timestamp + pulse_width_ticks;
				bool scheduled = schedule_timer_event(pulse_end_time, event->dimmer_id, EVENT_END_PULSE);
				
				// If scheduling failed, turn off triac immediately
				// to prevent it staying on
				if (!scheduled)
				{
					gpio_set_level(dimOutPin[event->dimmer_id], 0);
					ESP_LOGE(TAG, "Failed to schedule pulse end for dimmer %d", event->dimmer_id);
				}
			}
			else if (event->event_type == EVENT_END_PULSE)
			{
				// Turn off triac gate
				gpio_set_level(dimOutPin[event->dimmer_id], 0);
			}
		}
		else
		{
			ESP_LOGE(TAG, "Invalid dimmer_id in event: %d", event->dimmer_id);
		}
		
		// Remove processed event
		remove_event(i);
	}
	
	// Set alarm for the next scheduled event
	set_next_alarm();
}
