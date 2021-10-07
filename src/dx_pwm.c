#include "dx_pwm.h"

bool dx_pwmSetDutyCycle(DX_PWM_BINDING *pwm_binding, uint32_t hertz, uint32_t dutyCyclePercentage)
{
    if (pwm_binding->initialized && IN_RANGE(dutyCyclePercentage, 0, 100) && hertz < 100000) {
        PwmState state = {.enabled = true, .polarity = pwm_binding->pwmPolarity};

        state.period_nsec = 1000000000 / hertz;
        state.dutyCycle_nsec = state.period_nsec * dutyCyclePercentage / 100;

        if (PWM_Apply(pwm_binding->fd, pwm_binding->channelId, &state) == -1) {
            return false;
        }
    } else {
        return false;
    }

    return true;
}

bool dx_pwmStop(DX_PWM_BINDING *pwm_binding)
{
    PwmState state = {.period_nsec = 100000, .dutyCycle_nsec = 100, .polarity = pwm_binding->pwmPolarity, .enabled = false};

    if (PWM_Apply(pwm_binding->fd, pwm_binding->channelId, &state) == -1) {
        Log_Debug("PWM Channel ID (%d), aka (%s) apply failed\n", pwm_binding->channelId, pwm_binding->name);
        return false;
    }
    return true;
}

bool dx_pwmOpen(DX_PWM_BINDING *pwm_binding)
{
    if (pwm_binding->initialized) {
        return true;
    }

    if ((pwm_binding->fd = PWM_Open(pwm_binding->controllerId)) == -1) {
        return false;
    } else {
        pwm_binding->initialized = true;
    }

    return true;
}

void dx_pwmSetOpen(DX_PWM_BINDING **pwmSet, size_t pwmSetCount)
{
    for (int i = 0; i < pwmSetCount; i++) {
        if (!dx_pwmOpen(pwmSet[i])) {
            break;
        }
    }
}