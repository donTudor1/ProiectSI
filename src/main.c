#include "drivers/gpio/gpio.h"
#include "drivers/interrupt/external_interrupt.h"
#include "drivers/timer/timer0.h"
#include "drivers/pwm/pwm.h"
#include "drivers/adc/adc.h"
#include "bsp/nano.h"

/* Set to 0 for automatic mode switch every 10 s (no wire on D2). */
#define USE_BUTTON_MODE_SWITCH 1

#define MOVE_INTERVAL_MS   15U
#define DEBOUNCE_MS        200U
#define AUTO_MODE_MS       10000U
#define ADC_REPICK_MS      400U
#define LED_SLOW_MS        500U
#define LED_FAST_MS        150U
#define BLINK_ON_MS        120U
#define BLINK_OFF_MS       120U

volatile uint8_t  g_mode_change = 0;
volatile uint32_t g_last_isr_ms = 0;

static void OnButton(void)
{
    g_mode_change = 1;
    g_last_isr_ms = Millis();
}

static uint8_t angle_to_duty(uint8_t angle)
{
    if (angle > 180U) {
        angle = 180U;
    }
    return (uint8_t)(13U + ((uint32_t)angle * (26U - 13U)) / 180U);
}

static void servo_write(uint8_t pan, uint8_t tilt)
{
    PWM_SetDutyCycle(D9, angle_to_duty(pan));
    PWM_SetDutyCycle(D10, angle_to_duty(tilt));
}

static void pick_random_targets(uint8_t *pan_tgt, uint8_t *tilt_tgt)
{
    *pan_tgt = (uint8_t)(ADC_Read(0U) % 181U);
    *tilt_tgt = (uint8_t)(ADC_Read(0U) % 181U);
}

static void move_toward(uint8_t *cur, uint8_t tgt)
{
    if (*cur < tgt) {
        (*cur)++;
    } else if (*cur > tgt) {
        (*cur)--;
    }
}

static void start_mode_blink(uint8_t mode, uint8_t *blink_remaining)
{
    *blink_remaining = (uint8_t)(mode + 1U);
}

static void update_mode_blink(uint32_t now, uint32_t *t_blink, uint8_t *blink_remaining, uint8_t *blink_led_on)
{
    uint32_t interval = *blink_led_on ? BLINK_ON_MS : BLINK_OFF_MS;

    if (*blink_remaining == 0U) {
        return;
    }

    if (now - *t_blink < interval) {
        return;
    }

    *t_blink = now;

    if (*blink_led_on) {
        GPIO_Write(LED_BUILTIN, GPIO_LOW);
        *blink_led_on = 0U;
        if (*blink_remaining > 0U) {
            (*blink_remaining)--;
        }
    } else {
        GPIO_Write(LED_BUILTIN, GPIO_HIGH);
        *blink_led_on = 1U;
    }
}

static void update_mode_led(uint8_t mode, uint32_t now, uint32_t *t_led, uint8_t blink_remaining)
{
    if (blink_remaining > 0U) {
        return;
    }

    uint32_t interval = LED_SLOW_MS;
    if (mode == 2U) {
        interval = LED_FAST_MS;
    } else if (mode == 0U) {
        GPIO_Write(LED_BUILTIN, GPIO_LOW);
        return;
    }

    if (now - *t_led >= interval) {
        *t_led = now;
        GPIO_Toggle(LED_BUILTIN);
    }
}

static void reset_mode_state(uint8_t mode, uint8_t *pos, int8_t *dir,
                             uint8_t *pan_cur, uint8_t *tilt_cur,
                             uint8_t *pan_tgt, uint8_t *tilt_tgt,
                             uint32_t *t_adc, uint32_t now)
{
    *pos = 0U;
    *dir = 1;
    *pan_cur = 0U;
    *tilt_cur = 0U;
    pick_random_targets(pan_tgt, tilt_tgt);
    *t_adc = now;

    if (mode == 2U) {
        servo_write(*pan_cur, *tilt_cur);
    } else {
        servo_write(*pos, (mode == 1U) ? (uint8_t)(180U - *pos) : *pos);
    }
}

int main(void)
{
    Timer0_Init();
    ADC_Init();

    GPIO_Init(LED_BUILTIN, GPIO_OUTPUT);
    GPIO_Write(LED_BUILTIN, GPIO_LOW);

    PWM_Init(D9, 50U);
    PWM_Init(D10, 50U);

#if USE_BUTTON_MODE_SWITCH
    GPIO_Init(D2, GPIO_INPUT);
    GPIO_Write(D2, GPIO_HIGH);
    ExtInt_Init(INT_0, EXT_INT_FALLING_EDGE, OnButton);
#endif

    uint8_t mode = 0U;
    uint8_t pos = 0U;
    int8_t dir = 1;
    uint8_t pan_cur = 0U;
    uint8_t tilt_cur = 0U;
    uint8_t pan_tgt = 90U;
    uint8_t tilt_tgt = 90U;

    uint8_t blink_remaining = 0U;
    uint8_t blink_led_on = 0U;

    uint32_t t_move = 0U;
    uint32_t t_led = 0U;
    uint32_t t_blink = 0U;
    uint32_t t_adc = 0U;
#if !USE_BUTTON_MODE_SWITCH
    uint32_t t_mode = 0U;
#endif

    servo_write(0U, 0U);
    start_mode_blink(mode, &blink_remaining);

    while (1) {
        uint32_t now = Millis();

#if USE_BUTTON_MODE_SWITCH
        if (g_mode_change && (now - g_last_isr_ms >= DEBOUNCE_MS)) {
            g_mode_change = 0;
            mode = (uint8_t)((mode + 1U) % 3U);
            reset_mode_state(mode, &pos, &dir, &pan_cur, &tilt_cur, &pan_tgt, &tilt_tgt, &t_adc, now);
            start_mode_blink(mode, &blink_remaining);
            blink_led_on = 0U;
            t_blink = now;
        }
#else
        if (now - t_mode >= AUTO_MODE_MS) {
            t_mode = now;
            mode = (uint8_t)((mode + 1U) % 3U);
            reset_mode_state(mode, &pos, &dir, &pan_cur, &tilt_cur, &pan_tgt, &tilt_tgt, &t_adc, now);
            start_mode_blink(mode, &blink_remaining);
            blink_led_on = 0U;
            t_blink = now;
        }
#endif

        if (now - t_move >= MOVE_INTERVAL_MS) {
            t_move = now;

            switch (mode) {
                case 0U:
                    pos = (uint8_t)(pos + (uint8_t)dir);
                    if (pos == 0U || pos == 180U) {
                        dir = (int8_t)(-dir);
                    }
                    servo_write(pos, pos);
                    break;

                case 1U:
                    pos = (uint8_t)(pos + (uint8_t)dir);
                    if (pos == 0U || pos == 180U) {
                        dir = (int8_t)(-dir);
                    }
                    servo_write(pos, (uint8_t)(180U - pos));
                    break;

                case 2U:
                default:
                    move_toward(&pan_cur, pan_tgt);
                    move_toward(&tilt_cur, tilt_tgt);
                    servo_write(pan_cur, tilt_cur);

                    if ((pan_cur == pan_tgt && tilt_cur == tilt_tgt) ||
                        (now - t_adc >= ADC_REPICK_MS)) {
                        pick_random_targets(&pan_tgt, &tilt_tgt);
                        t_adc = now;
                    }
                    break;
            }
        }

        update_mode_blink(now, &t_blink, &blink_remaining, &blink_led_on);
        update_mode_led(mode, now, &t_led, blink_remaining);
    }
}
