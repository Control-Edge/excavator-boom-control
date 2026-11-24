#include "BoomControl.h"

/* Joystick voltage to a command of -100..+100 %, with a neutral zone around the centre */
static int8_t joystickToCommand(const BoomControlConfig_ts *c, uint16_t joystick_mV)
{
    int32_t centre = (c->joystickMin_mV + c->joystickMax_mV) / 2;
    int32_t span = (c->joystickMax_mV - c->joystickMin_mV) / 2 - c->deadband_mV;
    int32_t offset;

    if (joystick_mV < c->joystickMin_mV)
        joystick_mV = c->joystickMin_mV;
    if (joystick_mV > c->joystickMax_mV)
        joystick_mV = c->joystickMax_mV;

    offset = (int32_t)joystick_mV - centre;
    if (offset > c->deadband_mV)
        return (int8_t)((offset - c->deadband_mV) * 100 / span);
    if (offset < -(int32_t)c->deadband_mV)
        return (int8_t)((offset + c->deadband_mV) * 100 / span);
    return 0;
}

/* A command of 1..100 % to a coil current between coilMin_mA and coilMax_mA */
static uint16_t commandToCurrent(const BoomControlConfig_ts *c, int32_t command_pct)
{
    return (uint16_t)(c->coilMin_mA + (c->coilMax_mA - c->coilMin_mA) * command_pct / 100);
}

void BoomControl_init(BoomControl_ts *ctx, const BoomControlConfig_ts *config)
{
    ctx->config = *config;
    ctx->joystick_mV = 0;
    ctx->command_pct = 0;
    ctx->coilUp_mA = 0;
    ctx->coilDown_mA = 0;
    ctx->fault = 0;
    ctx->badSignal_ms = 0;
}

void BoomControl_cyclic(BoomControl_ts *ctx, uint16_t cycle_ms)
{
    const BoomControlConfig_ts *c = &ctx->config;
    uint8_t badSignal = ctx->joystick_mV < c->joystickMin_mV - c->signalMargin_mV ||
                        ctx->joystick_mV > c->joystickMax_mV + c->signalMargin_mV;
    int8_t requested = badSignal ? 0 : joystickToCommand(c, ctx->joystick_mV);

    /* A bad signal holds the boom at once, and latches a fault once it has lasted faultDelay_ms */
    if (!badSignal)
        ctx->badSignal_ms = 0;
    else if (ctx->badSignal_ms < c->faultDelay_ms)
        ctx->badSignal_ms += cycle_ms;

    if (ctx->badSignal_ms >= c->faultDelay_ms)
        ctx->fault = 1;
    else if (ctx->fault && requested == 0)
        ctx->fault = 0; /* cleared only with the joystick back in neutral */

    ctx->command_pct = ctx->fault ? 0 : requested;

    /* Positive commands drive the up coil, negative ones the down coil */
    ctx->coilUp_mA = ctx->command_pct > 0 ? commandToCurrent(c, ctx->command_pct) : 0;
    ctx->coilDown_mA = ctx->command_pct < 0 ? commandToCurrent(c, -ctx->command_pct) : 0;
}
