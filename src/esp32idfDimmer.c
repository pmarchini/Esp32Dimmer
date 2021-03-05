#include "esp32idfDimmer.h"

int pulseWidth = 1;
volatile int current_dim = 0;
int all_dim = 3;
int rise_fall = true;
char user_zero_cross = '0';

static int toggleCounter = 0;
static int toggleReload = 25;
volatile bool _initDone = false;
volatile int _steps = 0;

#define STEPS _steps

static dimmertyp *dimmer[ALL_DIMMERS];
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
	gpio_set_direction(user_dimmer_pin, GPIO_MODE_OUTPUT);
	//Return the pointer
	return dimmer[current_dim - 1];
}

/*TODO -> fix correct values with details and a small guide on value set*/
#define F 1600
#define BASE_SPEED 320

const int base_speed = 320;
int steps = 0;

uint32_t timer_divider = (TIMER_BASE_CLK / F);

void config_timer(int ACfreq)
{
	/*System timer startup has been done*/
	if (_initDone)
	{
		return;
	}

	STEPS = 0;

	if (ACfreq > 10 && ACfreq <= 100)
	{
		STEPS = (1 / ACfreq) * pow(10, 6);
	}
	else
	{
		STEPS = 100;
	}

	memset(&m_timer_config, 0, sizeof(m_timer_config));

	/* Prepare configuration */
	timer_config_t m_timer_config =
		{
			.alarm_en = TIMER_ALARM_DIS,
			.counter_en = TIMER_PAUSE,
			.counter_dir = TIMER_COUNT_UP,
			.auto_reload = TIMER_AUTORELOAD_EN,
			.divider = 250,
		};

	/* Configure the alarm value and the interrupt on alarm. */
	timer_init(TIMER_GROUP_0, TIMER_0, &m_timer_config);
	timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, STEPS);
	timer_enable_intr(TIMER_GROUP_0, TIMER_0);
	timer_isr_register(TIMER_GROUP_0, TIMER_0, onTimerISR, NULL, ESP_INTR_FLAG_IRAM, NULL);
	/* start timer */
	timer_start(TIMER_GROUP_0, TIMER_0);
}

/*Zero-crossing pin setting
 *set as input
 *set as pullup
 *set its interrupt*/
void ext_int_init(dimmertyp *ptr)
{
	gpio_set_direction(dimZCPin[ptr->current_num], GPIO_MODE_INPUT);
	gpio_set_pull_mode(dimZCPin[ptr->current_num], GPIO_PULLUP_ONLY);
	gpio_set_intr_type(dimZCPin[ptr->current_num], GPIO_INTR_POSEDGE);
}

void begin(dimmertyp *ptr, DIMMER_MODE_typedef DIMMER_MODE, ON_OFF_typedef ON_OFF, int FREQ)
{
	dimMode[ptr->current_num] = DIMMER_MODE;
	dimState[ptr->current_num] = ON_OFF;
	config_timer(FREQ);
	ext_int_init(ptr);
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

void IRAM_ATTR isr_ext()
{
	for (int i = 0; i < current_dim; i++)
		if (dimState[i] == ON)
		{
			zeroCross[i] = 1;
		}
}

static int k;
/* Execution on timer event */
void IRAM_ATTR onTimerISR()
{

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
}
