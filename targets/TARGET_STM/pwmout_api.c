/* mbed Microcontroller Library
 *******************************************************************************
 * Copyright (c) 2015, STMicroelectronics
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of STMicroelectronics nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *******************************************************************************
 */
#include "pwmout_api.h"

#if DEVICE_PWMOUT

#include "cmsis.h"
#include "pinmap.h"
#include "mbed_error.h"
#include "PeripheralPins.h"
#include "pwmout_device.h"

static TIM_HandleTypeDef TimHandle;

#if defined(HRTIM1)
#define HRTIM_CHANNEL(Y,X)  (uint32_t)(0x00000001 << ((((Y*2)+(X)) & 0xF)))
#define HRTIM_TIMERID(X)  (uint32_t)(0x00000001 << (17U  + X))// << (X)))

typedef struct{
    uint8_t timer;
    uint32_t channel;
    uint32_t timerid;
} hrtim_t;

static hrtim_t hrtim_timer;
static HRTIM_HandleTypeDef HrtimHandle;
static HRTIM_CompareCfgTypeDef sConfig_compare;
static HRTIM_TimeBaseCfgTypeDef sConfig_time_base;

static void _pwmout_obj_period_us(pwmout_t *obj, int us);
#endif

/* Convert STM32 Cube HAL channel to LL channel */
uint32_t TIM_ChannelConvert_HAL2LL(uint32_t channel, pwmout_t *obj)
{
#if !defined(PWMOUT_INVERTED_NOT_SUPPORTED)
    if (obj->inverted) {
        switch (channel) {
            case TIM_CHANNEL_1  :
                return LL_TIM_CHANNEL_CH1N;
            case TIM_CHANNEL_2  :
                return LL_TIM_CHANNEL_CH2N;
            case TIM_CHANNEL_3  :
                return LL_TIM_CHANNEL_CH3N;
#if defined(LL_TIM_CHANNEL_CH4N)
            case TIM_CHANNEL_4  :
                return LL_TIM_CHANNEL_CH4N;
#endif
            default : /* Optional */
                return 0;
        }
    } else
#endif
    {
        switch (channel) {
            case TIM_CHANNEL_1  :
                return LL_TIM_CHANNEL_CH1;
            case TIM_CHANNEL_2  :
                return LL_TIM_CHANNEL_CH2;
            case TIM_CHANNEL_3  :
                return LL_TIM_CHANNEL_CH3;
            case TIM_CHANNEL_4  :
                return LL_TIM_CHANNEL_CH4;
            default : /* Optional */
                return 0;
        }
    }
}

#if STATIC_PINMAP_READY
#define PWM_INIT_DIRECT pwmout_init_direct
void pwmout_init_direct(pwmout_t *obj, const PinMap *pinmap)
#else
#define PWM_INIT_DIRECT _pwmout_init_direct
static void _pwmout_init_direct(pwmout_t *obj, const PinMap *pinmap)
#endif
{
    // Get the peripheral name from the pin and assign it to the object
    obj->pwm = (PWMName)pinmap->peripheral;

    // Get the functions (timer channel, (non)inverted) from the pin and assign it to the object
    uint32_t function = (uint32_t)pinmap->function;
    MBED_ASSERT(function != (uint32_t)NC);
    obj->channel = STM_PIN_CHANNEL(function);
    obj->inverted = STM_PIN_INVERTED(function);

#if defined(HRTIM1)
    if (obj->pwm == PWM_I) {

        HRTIM_TimerCfgTypeDef       sConfig_timer;
        HRTIM_OutputCfgTypeDef      sConfig_output_config;

        __HAL_RCC_HRTIM1_CLK_ENABLE();

        if(STM_PORT(pinmap->pin) == 0) {
            __HAL_RCC_GPIOA_CLK_ENABLE();
        } else if(STM_PORT(pinmap->pin) == 1) {
            __HAL_RCC_GPIOB_CLK_ENABLE();
        } else if(STM_PORT(pinmap->pin) == 2) {
            __HAL_RCC_GPIOC_CLK_ENABLE();
        } else if(STM_PORT(pinmap->pin) == 3) {
            __HAL_RCC_GPIOD_CLK_ENABLE();
        } else if(STM_PORT(pinmap->pin) == 4) {
            __HAL_RCC_GPIOE_CLK_ENABLE();
        } else if(STM_PORT(pinmap->pin) == 5) {
            __HAL_RCC_GPIOF_CLK_ENABLE();
        } else if(STM_PORT(pinmap->pin) == 6) {
            __HAL_RCC_GPIOG_CLK_ENABLE();
        } else {
           __HAL_RCC_GPIOH_CLK_ENABLE();
        }

        hrtim_timer.timer = obj->channel;
        hrtim_timer.channel = HRTIM_CHANNEL(hrtim_timer.timer,obj->inverted);
        hrtim_timer.timerid = HRTIM_TIMERID(hrtim_timer.timer);

        pin_function(pinmap->pin, pinmap->function);
        pin_mode(pinmap->pin, PullNone);

        // Initialize obj with default values (period 550Hz, duty 0%)
        _pwmout_obj_period_us(obj, 18000);
        obj->pulse = (uint32_t)((float)obj->period * 1.0 + 0.5);

        // Initialize the HRTIM structure
        HrtimHandle.Instance = HRTIM1;
        HrtimHandle.Init.HRTIMInterruptResquests = HRTIM_IT_NONE;
        HrtimHandle.Init.SyncOptions = HRTIM_SYNCOPTION_NONE;

        HAL_HRTIM_Init(&HrtimHandle);

        // Configure the HRTIM TIME PWM channels 2
        sConfig_time_base.Mode = HRTIM_MODE_CONTINUOUS;
        sConfig_time_base.Period = 0xFFDFU;
        sConfig_time_base.PrescalerRatio = HRTIM_PRESCALERRATIO_DIV4;
        sConfig_time_base.RepetitionCounter = 0;

        HAL_HRTIM_TimeBaseConfig(&HrtimHandle, hrtim_timer.timer, &sConfig_time_base);

        sConfig_timer.DMARequests = HRTIM_TIM_DMA_NONE;
        sConfig_timer.HalfModeEnable = HRTIM_HALFMODE_DISABLED;
        sConfig_timer.StartOnSync = HRTIM_SYNCSTART_DISABLED;
        sConfig_timer.ResetOnSync = HRTIM_SYNCRESET_DISABLED;
        sConfig_timer.DACSynchro = HRTIM_DACSYNC_NONE;
        sConfig_timer.PreloadEnable = HRTIM_PRELOAD_ENABLED;
        sConfig_timer.UpdateGating = HRTIM_UPDATEGATING_INDEPENDENT;
        sConfig_timer.BurstMode = HRTIM_TIMERBURSTMODE_MAINTAINCLOCK;
        sConfig_timer.RepetitionUpdate = HRTIM_UPDATEONREPETITION_ENABLED;
        sConfig_timer.ResetUpdate = HRTIM_TIMUPDATEONRESET_DISABLED;
        sConfig_timer.InterruptRequests = HRTIM_TIM_IT_NONE;
        sConfig_timer.PushPull = HRTIM_TIMPUSHPULLMODE_DISABLED;
        sConfig_timer.FaultEnable = HRTIM_TIMFAULTENABLE_NONE;
        sConfig_timer.FaultLock = HRTIM_TIMFAULTLOCK_READWRITE;
        sConfig_timer.DeadTimeInsertion = HRTIM_TIMDEADTIMEINSERTION_DISABLED;
        sConfig_timer.UpdateTrigger = HRTIM_TIMUPDATETRIGGER_NONE;
        sConfig_timer.ResetTrigger = HRTIM_TIMRESETTRIGGER_NONE;

        HAL_HRTIM_WaveformTimerConfig(&HrtimHandle, hrtim_timer.timer, &sConfig_timer);

        sConfig_compare.AutoDelayedMode = HRTIM_AUTODELAYEDMODE_REGULAR;
        sConfig_compare.AutoDelayedTimeout = 0;
        sConfig_compare.CompareValue = 0;

        HAL_HRTIM_WaveformCompareConfig(&HrtimHandle, hrtim_timer.timer, HRTIM_COMPAREUNIT_2, &sConfig_compare);

        sConfig_output_config.Polarity = HRTIM_OUTPUTPOLARITY_LOW;
        sConfig_output_config.SetSource = HRTIM_OUTPUTRESET_TIMCMP2;
        sConfig_output_config.ResetSource = HRTIM_OUTPUTSET_TIMPER;
        sConfig_output_config.IdleMode = HRTIM_OUTPUTIDLEMODE_NONE;
        sConfig_output_config.IdleLevel = HRTIM_OUTPUTIDLELEVEL_INACTIVE;
        sConfig_output_config.FaultLevel = HRTIM_OUTPUTFAULTLEVEL_NONE;
        sConfig_output_config.ChopperModeEnable = HRTIM_OUTPUTCHOPPERMODE_DISABLED;
        sConfig_output_config.BurstModeEntryDelayed = HRTIM_OUTPUTBURSTMODEENTRY_REGULAR;
        sConfig_output_config.ResetSource = HRTIM_OUTPUTRESET_TIMPER;
        sConfig_output_config.SetSource = HRTIM_OUTPUTSET_TIMCMP2;

        HAL_HRTIM_WaveformOutputConfig(&HrtimHandle,  hrtim_timer.timer, hrtim_timer.channel, &sConfig_output_config);

        // Start PWM signals generation
        if (HAL_HRTIM_WaveformOutputStart(&HrtimHandle, hrtim_timer.channel) != HAL_OK)
        {
            // PWM Generation Error
            return;
        }

        // Start HRTIM counter
        if (HAL_HRTIM_WaveformCounterStart(&HrtimHandle, hrtim_timer.timerid) != HAL_OK)
        {
            // PWM Generation Error
            return;
        }
        pwmout_period_us(obj, 18000); // 550Hz minimum default

        return;
    }
#endif

    // Enable TIM clock
#if defined(TIM1_BASE)
    if (obj->pwm == PWM_1) {
        __HAL_RCC_TIM1_CLK_ENABLE();
    }
#endif
#if defined(TIM2_BASE)
    if (obj->pwm == PWM_2) {
        __HAL_RCC_TIM2_CLK_ENABLE();
    }
#endif
#if defined(TIM3_BASE)
    if (obj->pwm == PWM_3) {
        __HAL_RCC_TIM3_CLK_ENABLE();
    }
#endif
#if defined(TIM4_BASE)
    if (obj->pwm == PWM_4) {
        __HAL_RCC_TIM4_CLK_ENABLE();
    }
#endif
#if defined(TIM5_BASE)
    if (obj->pwm == PWM_5) {
        __HAL_RCC_TIM5_CLK_ENABLE();
    }
#endif
#if defined(TIM8_BASE)
    if (obj->pwm == PWM_8) {
        __HAL_RCC_TIM8_CLK_ENABLE();
    }
#endif
#if defined(TIM9_BASE)
    if (obj->pwm == PWM_9) {
        __HAL_RCC_TIM9_CLK_ENABLE();
    }
#endif
#if defined(TIM10_BASE)
    if (obj->pwm == PWM_10) {
        __HAL_RCC_TIM10_CLK_ENABLE();
    }
#endif
#if defined(TIM11_BASE)
    if (obj->pwm == PWM_11) {
        __HAL_RCC_TIM11_CLK_ENABLE();
    }
#endif
#if defined(TIM12_BASE)
    if (obj->pwm == PWM_12) {
        __HAL_RCC_TIM12_CLK_ENABLE();
    }
#endif
#if defined(TIM13_BASE)
    if (obj->pwm == PWM_13) {
        __HAL_RCC_TIM13_CLK_ENABLE();
    }
#endif
#if defined(TIM14_BASE)
    if (obj->pwm == PWM_14) {
        __HAL_RCC_TIM14_CLK_ENABLE();
    }
#endif
#if defined(TIM15_BASE)
    if (obj->pwm == PWM_15) {
        __HAL_RCC_TIM15_CLK_ENABLE();
    }
#endif
#if defined(TIM16_BASE)
    if (obj->pwm == PWM_16) {
        __HAL_RCC_TIM16_CLK_ENABLE();
    }
#endif
#if defined(TIM17_BASE)
    if (obj->pwm == PWM_17) {
        __HAL_RCC_TIM17_CLK_ENABLE();
    }
#endif
#if defined(TIM18_BASE)
    if (obj->pwm == PWM_18) {
        __HAL_RCC_TIM18_CLK_ENABLE();
    }
#endif
#if defined(TIM19_BASE)
    if (obj->pwm == PWM_19) {
        __HAL_RCC_TIM19_CLK_ENABLE();
    }
#endif
#if defined(TIM20_BASE)
    if (obj->pwm == PWM_20) {
        __HAL_RCC_TIM20_CLK_ENABLE();
    }
#endif
#if defined(TIM21_BASE)
    if (obj->pwm == PWM_21) {
        __HAL_RCC_TIM21_CLK_ENABLE();
    }
#endif
#if defined(TIM22_BASE)
    if (obj->pwm == PWM_22) {
        __HAL_RCC_TIM22_CLK_ENABLE();
    }
#endif
    // Configure GPIO
    pin_function(pinmap->pin, pinmap->function);

    obj->pin = pinmap->pin;
    obj->period = 0;
    obj->pulse = 0;
    obj->prescaler = 1;

    pwmout_period_us(obj, 20000); // 20 ms per default
}

void pwmout_init(pwmout_t *obj, PinName pin)
{
    int peripheral = 0;
    int function = (int)pinmap_find_function(pin, PinMap_PWM);
    // check Function before peripheral because pinmap_peripheral
    // assert a error and stop the execution
    if (function == -1) {
        peripheral = (int)pinmap_peripheral(pin, PinMap_PWM_HRTIM);
        function = (int)pinmap_find_function(pin, PinMap_PWM_HRTIM);
    } else {
        peripheral = (int)pinmap_peripheral(pin, PinMap_PWM);
    }
    const PinMap static_pinmap = {pin, peripheral, function};

    PWM_INIT_DIRECT(obj, &static_pinmap);
}

void pwmout_free(pwmout_t *obj)
{
    // Configure GPIO back to reset value
    pin_function(obj->pin, STM_PIN_DATA(STM_MODE_ANALOG, GPIO_NOPULL, 0));
}

void pwmout_write(pwmout_t *obj, float value)
{

#if defined(HRTIM1)
    if (obj->pwm == PWM_I) {
        if (value <= (float)0.0) {
            value = 1.0;
        } else if (value >= (float)1.0) {
            value = 0.0;
        }
        obj->pulse = (uint32_t)((float)obj->period * value + 0.5);
        sConfig_compare.CompareValue =  obj->pulse;
        if (HAL_HRTIM_WaveformCompareConfig(&HrtimHandle,  hrtim_timer.timer, HRTIM_COMPAREUNIT_2, &sConfig_compare) != HAL_OK)
        {
            return;
        }
        return;
    }
#endif

    TIM_OC_InitTypeDef sConfig;
    int channel = 0;

    TimHandle.Instance = (TIM_TypeDef *)(obj->pwm);

    if (value < (float)0.0) {
        value = 0.0;
    } else if (value > (float)1.0) {
        value = 1.0;
    }

    obj->pulse = (uint32_t)((float)obj->period * value + 0.5);

    // Configure channels
    sConfig.OCMode       = TIM_OCMODE_PWM1;
    sConfig.Pulse        = obj->pulse / obj->prescaler;
    sConfig.OCPolarity   = TIM_OCPOLARITY_HIGH;
    sConfig.OCFastMode   = TIM_OCFAST_DISABLE;
#if defined(TIM_OCIDLESTATE_RESET)
    sConfig.OCIdleState  = TIM_OCIDLESTATE_RESET;
#endif
#if defined(TIM_OCNIDLESTATE_RESET)
    sConfig.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    sConfig.OCNIdleState = TIM_OCNIDLESTATE_RESET;
#endif

    switch (obj->channel) {
        case 1:
            channel = TIM_CHANNEL_1;
            break;
        case 2:
            channel = TIM_CHANNEL_2;
            break;
        case 3:
            channel = TIM_CHANNEL_3;
            break;
        case 4:
            channel = TIM_CHANNEL_4;
            break;
        default:
            return;
    }

    if (LL_TIM_CC_IsEnabledChannel(TimHandle.Instance, TIM_ChannelConvert_HAL2LL(channel, obj)) == 0) {
        // If channel is not enabled, proceed to channel configuration
        if (HAL_TIM_PWM_ConfigChannel(&TimHandle, &sConfig, channel) != HAL_OK) {
            error("Cannot initialize PWM\n");
        }
    } else {
        // If channel already enabled, only update compare value to avoid glitch
        __HAL_TIM_SET_COMPARE(&TimHandle, channel, sConfig.Pulse);
        return;
    }

#if !defined(PWMOUT_INVERTED_NOT_SUPPORTED)
    if (obj->inverted) {
        HAL_TIMEx_PWMN_Start(&TimHandle, channel);
    } else
#endif
    {
        HAL_TIM_PWM_Start(&TimHandle, channel);
    }
}

float pwmout_read(pwmout_t *obj)
{
    float value = 0;
    if (obj->period > 0) {
        value = (float)(obj->pulse) / (float)(obj->period);
    }

    if (obj->pwm == PWM_I) {
        if (value <= (float)0.0) {
            value = 1.0;
        } else if (value >= (float)1.0) {
            value = 0.0;
        }
    }

    return ((value > (float)1.0) ? (float)(1.0) : (value));
}

void pwmout_period(pwmout_t *obj, float seconds)
{
    pwmout_period_us(obj, seconds * 1000000.0f);
}

void pwmout_period_ms(pwmout_t *obj, int ms)
{
    pwmout_period_us(obj, ms * 1000);
}

void pwmout_period_us(pwmout_t *obj, int us)
{

#if defined(HRTIM1)
    if (obj->pwm == PWM_I) {
        float dc = pwmout_read(obj);

        _pwmout_obj_period_us(obj, us);

        sConfig_time_base.Mode = HRTIM_MODE_CONTINUOUS;
        sConfig_time_base.Period = obj->period;
        sConfig_time_base.PrescalerRatio = obj->prescaler;
        sConfig_time_base.RepetitionCounter = 0;

        HAL_HRTIM_TimeBaseConfig(&HrtimHandle,  hrtim_timer.timer, &sConfig_time_base);
        pwmout_write(obj, dc);
        return;
    }
#endif

    TimHandle.Instance = (TIM_TypeDef *)(obj->pwm);
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    uint32_t PclkFreq = 0;
    uint32_t APBxCLKDivider = RCC_HCLK_DIV1;
    float dc = pwmout_read(obj);
    uint8_t i = 0;

    __HAL_TIM_DISABLE(&TimHandle);

    // Get clock configuration
    // Note: PclkFreq contains here the Latency (not used after)
    HAL_RCC_GetClockConfig(&RCC_ClkInitStruct, &PclkFreq);

    /*  Parse the pwm / apb mapping table to find the right entry */
    while (pwm_apb_map_table[i].pwm != obj->pwm) {
        i++;
    }

    if (pwm_apb_map_table[i].pwm == 0) {
        error("Unknown PWM instance");
    }

    if (pwm_apb_map_table[i].pwmoutApb == PWMOUT_ON_APB1) {
        PclkFreq = HAL_RCC_GetPCLK1Freq();
        APBxCLKDivider = RCC_ClkInitStruct.APB1CLKDivider;
    } else {
#if !defined(PWMOUT_APB2_NOT_SUPPORTED)
        PclkFreq = HAL_RCC_GetPCLK2Freq();
        APBxCLKDivider = RCC_ClkInitStruct.APB2CLKDivider;
#endif
    }


    /* By default use, 1us as SW pre-scaler */
    obj->prescaler = 1;
    // TIMxCLK = PCLKx when the APB prescaler = 1 else TIMxCLK = 2 * PCLKx
    if (APBxCLKDivider == RCC_HCLK_DIV1) {
        TimHandle.Init.Prescaler = (((PclkFreq) / 1000000)) - 1; // 1 us tick
    } else {
        TimHandle.Init.Prescaler = (((PclkFreq * 2) / 1000000)) - 1; // 1 us tick
    }
    TimHandle.Init.Period = (us - 1);

    /*  In case period or pre-scalers are out of range, loop-in to get valid values */
    while ((TimHandle.Init.Period > 0xFFFF) || (TimHandle.Init.Prescaler > 0xFFFF)) {
        obj->prescaler = obj->prescaler * 2;
        if (APBxCLKDivider == RCC_HCLK_DIV1) {
            TimHandle.Init.Prescaler = (((PclkFreq) / 1000000) * obj->prescaler) - 1;
        } else {
            TimHandle.Init.Prescaler = (((PclkFreq * 2) / 1000000) * obj->prescaler) - 1;
        }
        TimHandle.Init.Period = (us - 1) / obj->prescaler;
        /*  Period decreases and prescaler increases over loops, so check for
         *  possible out of range cases */
        if ((TimHandle.Init.Period < 0xFFFF) && (TimHandle.Init.Prescaler > 0xFFFF)) {
            error("Cannot initialize PWM\n");
            break;
        }
    }

    TimHandle.Init.ClockDivision = 0;
    TimHandle.Init.CounterMode   = TIM_COUNTERMODE_UP;

    if (HAL_TIM_PWM_Init(&TimHandle) != HAL_OK) {
        error("Cannot initialize PWM\n");
    }

    // Save for future use
    obj->period = us;

    // Set duty cycle again
    pwmout_write(obj, dc);

    __HAL_TIM_ENABLE(&TimHandle);
}

int pwmout_read_period_us(pwmout_t *obj)
{
    return obj->period;
}

void pwmout_pulsewidth(pwmout_t *obj, float seconds)
{
    pwmout_pulsewidth_us(obj, seconds * 1000000.0f);
}

void pwmout_pulsewidth_ms(pwmout_t *obj, int ms)
{
    pwmout_pulsewidth_us(obj, ms * 1000);
}

void pwmout_pulsewidth_us(pwmout_t *obj, int us)
{
    float value = (float)us / (float)obj->period;
    pwmout_write(obj, value);
}

int pwmout_read_pulsewidth_us(pwmout_t *obj)
{
    float pwm_duty_cycle = pwmout_read(obj);
    return (int)(pwm_duty_cycle * (float)obj->period);
}

const PinMap *pwmout_pinmap()
{
    return PinMap_PWM;
}

#if defined(HRTIM1)
void _pwmout_obj_period_us(pwmout_t *obj, int us) {
    uint32_t frequency;
    uint32_t clocksource = __HAL_RCC_GET_HRTIM1_SOURCE();
    switch (clocksource) {
        case RCC_HRTIM1CLK_TIMCLK:
            frequency = HAL_RCC_GetHCLKFreq();
            break;
        case RCC_HRTIM1CLK_CPUCLK:
            frequency = HAL_RCC_GetSysClockFreq();
            break;
    }

    /* conversion from us to clock tick */
    obj->period = us * (frequency / 1000000) / 4;
    obj->prescaler = HRTIM_PRESCALERRATIO_DIV4;

    if (obj->period > 0xFFDFU) {
        obj->period = 0xFFDFU;
    }
}
#endif

#endif
