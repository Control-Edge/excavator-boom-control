#pragma once
#include <stdint.h>

typedef struct BoomControlConfig_s
{
    uint16_t joystickMin_mV;  /* joystick fully down */
    uint16_t joystickMax_mV;  /* joystick fully up */
    uint16_t deadband_mV;     /* half-width of the neutral zone around the centre */
    uint16_t signalMargin_mV; /* how far outside its range the signal may read before it is bad */
    uint16_t coilMin_mA;      /* current at which the valve starts to open */
    uint16_t coilMax_mA;      /* current for full flow */
    uint16_t faultDelay_ms;   /* how long a bad signal lasts before it latches a fault */
} BoomControlConfig_ts;

typedef struct BoomControl_s
{
    BoomControlConfig_ts config;

    /* Input */
    uint16_t joystick_mV;

    /* Outputs */
    int8_t command_pct;   /* -100 (full down) .. +100 (full up) */
    uint16_t coilUp_mA;
    uint16_t coilDown_mA;
    uint8_t fault;

    /* State */
    uint16_t badSignal_ms;
} BoomControl_ts;

void BoomControl_init(BoomControl_ts *ctx, const BoomControlConfig_ts *config);
void BoomControl_cyclic(BoomControl_ts *ctx, uint16_t cycle_ms);
