// RobotDyn porting of RBDDimmer library from Arduino framework to esp-idf
//
// author : pmarchini
// mail   : pietro.marchini94@gmail.com


/* TODO :
 *
 * 
 *
 *
 */

#include "esp32idfDimmer.h"

static const char *TAG = "Esp32idfDimmer";

int pulseWidth = 2;
volatile int current_dim = 0;
int all_dim = 3;
int rise_fall = true;
char user_zero_cross = '0';
int debug_signal_zc = 0;
bool flagDebug = false;

static int toggleCounter = 0;
static int toggleReload = 25;
volatile bool _initDone = false;
volatile int _steps = 0;


static dimmertyp *dimmer[ALL_DIMMERS];
volatile bool firstSetup = false;
volatile uint16_t dimPower[ALL_DIMMERS];
volatile gpio_num_t dimOutPin[ALL_DIMMERS];
volatile gpio_num_t dimZCPin[ALL_DIMMERS];
volatile uint16_t zeroCross[ALL_DIMMERS];
volatile DIMMER_MODE_typedef dimMode[ALL_DIMMERS];
volatile ON_OFF_typedef dimState[ALL_DIMMERS];
volatile uint16_t dimCounter[ALL_DIMMERS];
static uint16_t dimPulseBegin[ALL_DIMMERS];
volatile uint16_t togMax[ALL_DIMMERS];
volatile uint16_t togMin[ALL_DIMMERS];
volatile bool togDir[ALL_DIMMERS];

/* timer configurations */
timer_config_t m_timer_config;

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
	dimCounter[current_dim - 1] = 0;
	zeroCross[current_dim - 1] = 0;
	dimMode[current_dim - 1] = NORMAL_MODE;
	togMin[current_dim - 1] = 0;
	togMax[current_dim - 1] = 1;
	//Return the pointer
	return dimmer[current_dim - 1];
}



#define TIMER_DIVIDER         80  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL0_SEC   (0.0001) 


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

	/* Prepare configuration */
	timer_config_t m_timer_config =
		{
			.alarm_en = TIMER_ALARM_DIS,
			.counter_en = TIMER_PAUSE,
			.counter_dir = TIMER_COUNT_UP,
			.auto_reload = TIMER_AUTORELOAD_EN,
			.divider = TIMER_DIVIDER,
		};

	/*self regulation 50/60 Hz*/
	double m_calculated_interval = (1 / (ACfreq*2)) / 100;

	ESP_LOGI(TAG, "Timer configuration - configure interrupt and timer");
	/* Configure the alarm value and the interrupt on alarm. */
	timer_init(TIMER_GROUP_0, TIMER_0, &m_timer_config);
	timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, TIMER_SCALE * m_calculated_interval);
	timer_enable_intr(TIMER_GROUP_0, TIMER_0);
	timer_isr_register(TIMER_GROUP_0, TIMER_0, onTimerISR, (void*)TIMER_0, ESP_INTR_FLAG_IRAM, NULL);
	/* start timer */
	ESP_LOGI(TAG, "Timer configuration - start timer");
	timer_start(TIMER_GROUP_0, TIMER_0);
	timer_set_alarm(TIMER_GROUP_0,TIMER_0,TIMER_ALARM_EN);
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
	for(int i = 0;i < ptr->current_num;i++){
		if(dimZCPin[i] == dimZCPin[ptr->current_num]){
			alreadyInit = true;
		}
	}
	ESP_LOGI(TAG, "Already init = %3d",alreadyInit);
	if(!alreadyInit)
	{
		gpio_set_direction(dimZCPin[ptr->current_num], GPIO_MODE_INPUT);
		gpio_set_intr_type(dimZCPin[ptr->current_num], GPIO_INTR_NEGEDGE);
		gpio_intr_enable(dimZCPin[ptr->current_num]);
		gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
		gpio_isr_handler_add(dimZCPin[ptr->current_num],isr_ext,(void*)dimZCPin[ptr->current_num]);
	}
	ESP_LOGI(TAG, "Zero Cross interrupt configuration - completed");
	/*TRIAC command - configuration*/
	ESP_LOGI(TAG, "Triac command configuration");
	gpio_set_direction(dimOutPin[ptr->current_num], GPIO_MODE_OUTPUT);
	ESP_LOGI(TAG, "Triac command configuration - completed");
}

/*ISR debug region*/
#if DEBUG_ISR_DIMMER == ISR_DEBUG_ON

static xQueueHandle gpio_evt_queue = NULL;

static void gpio_isr_debug(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
}

#endif


/*ISR timer debug region*/
#if DEBUG_ISR_TIMER == ISR_DEBUG_ON

static xQueueHandle timer_event_queue = NULL;

static void timer_isr_debug(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(timer_event_queue, &io_num, portMAX_DELAY)) {
            printf("Timer interrupt event , counter = %5d \n",io_num);
        }
    }
}

#endif




void begin(dimmertyp *ptr, DIMMER_MODE_typedef DIMMER_MODE, ON_OFF_typedef ON_OFF, int FREQ)
{
	ESP_LOGI(TAG, "Dimmer - begin");
	dimMode[ptr->current_num] = DIMMER_MODE;
	dimState[ptr->current_num] = ON_OFF;

	#if DEBUG_ISR_DIMMER == ISR_DEBUG_ON
	if(!_initDone)
	{
			//create a queue to handle gpio event from isr
    		gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    		//start gpio task
    		xTaskCreate(gpio_isr_debug, "gpio_isr_debug", 2048, NULL, 10, NULL);
	}
	#endif

	#if DEBUG_ISR_TIMER == ISR_DEBUG_ON
	if(!_initDone)
	{
			//create a queue to handle timer event
    		timer_event_queue = xQueueCreate(10, sizeof(uint32_t));
    		//start gpio task
    		xTaskCreate(timer_isr_debug, "timer_isr_debug", 2048, NULL, 10, NULL);
	}
	#endif

	config_timer(FREQ);
	ext_int_init(ptr);

	//init completed
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
	if (maxValue > 99)
	{
		maxValue = 99;
	}
	if (minValue < 1)
	{
		minValue = 1;
	}
	dimMode[ptr->current_num] = TOGGLE_MODE;
	togMin[ptr->current_num] = powerBuf[maxValue];
	togMax[ptr->current_num] = powerBuf[minValue];

	toggleReload = 50;
}



static void IRAM_ATTR isr_ext(void* arg)
{

	#if DEBUG_ISR_DIMMER == ISR_DEBUG_ON
		uint32_t gpio_num = (uint32_t) arg;
    	xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
	#endif

	for (int i = 0; i < current_dim; i++)
		if (dimState[i] == ON)
		{
			zeroCross[i] = 1;
		}
}

static int k;
#if DEBUG_ISR_TIMER == ISR_DEBUG_ON
	static int counter = 0;
#endif
/* Execution on timer event */
static void IRAM_ATTR onTimerISR(void* para)
{
	/*Block needed to handle timer ISR*/
	timer_spinlock_take(TIMER_GROUP_0);
	timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_0);
	/*Give back spinlock at the end of the method*/
	/**********************************/
	#if DEBUG_ISR_TIMER == ISR_DEBUG_ON
		counter++;
		uint32_t info = (uint32_t) counter;
		xQueueSendFromISR(timer_event_queue, &info, NULL);
	#endif

	toggleCounter++;
	for (k = 0; k < current_dim; k++)
	{
		if (zeroCross[k] == 1)
		{
			dimCounter[k]++;

			if (dimMode[k] == TOGGLE_MODE)
			{
				/*****
			 * TOGGLE DIMMING MODE
			 *****/
				if (dimPulseBegin[k] >= togMax[k])
				{
					// if reach max dimming value
					togDir[k] = false; // downcount
				}
				if (dimPulseBegin[k] <= togMin[k])
				{
					// if reach min dimming value
					togDir[k] = true; // upcount
				}
				if (toggleCounter == toggleReload)
				{
					if (togDir[k] == true)
						dimPulseBegin[k]++;
					else
						dimPulseBegin[k]--;
				}
			}

			/*****
			 * DEFAULT DIMMING MODE (NOT TOGGLE)
			 *****/
			if (dimCounter[k] >= dimPulseBegin[k])
			{
				gpio_set_level(dimOutPin[k], 1);
			}

			if (dimCounter[k] >= (dimPulseBegin[k] + pulseWidth))
			{
				gpio_set_level(dimOutPin[k], 0);
				zeroCross[k] = 0;
				dimCounter[k] = 0;
			}
		}
	}
	if (toggleCounter >= toggleReload)
		toggleCounter = 1;


	/* After the alarm has been triggered
       we need enable it again, so it is triggered the next time */
    timer_group_enable_alarm_in_isr(TIMER_GROUP_0, TIMER_0);
	timer_spinlock_give(TIMER_GROUP_0);
}
