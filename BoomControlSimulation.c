#include <cppmodel/CModel.h>
#include "BoomControl.h"

static BoomControl_ts boom;
static uint8_t waitingForNeutral;
static CppModelRule_t oneCoil, noMotionOnBadSignal, faultDetected, restartFromNeutral;

/* The operator's joystick, in mV: points joined by straight lines. Two points at the same time make a jump. */
static const struct { unsigned long t_ms; double mV; } scenario[] = {
    {0, 2500},     {1000, 2500},                /* neutral */
    {3000, 500},                                /* move to full down */
    {11000, 4500}, {12000, 4500},               /* sweep to full up and hold */
    {12000, 0},    {13000, 0},                  /* the signal wire breaks: 0 V */
    {13000, 3500}, {14000, 3500},               /* the wire is back, joystick still pushed up */
    {14000, 2500}, {15000, 2500},               /* operator returns to neutral */
    {16000, 3500}, {17000, 3500},               /* and moves up again */
};

static double joystickAt(unsigned long t)
{
    size_t i, n = sizeof(scenario) / sizeof(scenario[0]);
    for (i = n - 1; i > 0; i--)
    {
        if (t >= scenario[i - 1].t_ms && t < scenario[i].t_ms)
            return scenario[i - 1].mV + (scenario[i].mV - scenario[i - 1].mV) * (double)(t - scenario[i - 1].t_ms) /
                                            (double)(scenario[i].t_ms - scenario[i - 1].t_ms);
    }
    return scenario[n - 1].mV;
}

static void Init(CModelSimulation_ts *self, unsigned long stepTime_ms)
{
    BoomControlConfig_ts config;
    (void)stepTime_ms;

    config.joystickMin_mV = CppModel_getParameterU16(self, "JoystickMin [mV]", 500);
    config.joystickMax_mV = CppModel_getParameterU16(self, "JoystickMax [mV]", 4500);
    config.deadband_mV = CppModel_getParameterU16(self, "Deadband [mV]", 200);
    config.signalMargin_mV = CppModel_getParameterU16(self, "SignalMargin [mV]", 250);
    config.coilMin_mA = CppModel_getParameterU16(self, "CoilMin [mA]", 300);
    config.coilMax_mA = CppModel_getParameterU16(self, "CoilMax [mA]", 700);
    config.faultDelay_ms = CppModel_getParameterU16(self, "FaultDelay [ms]", 100);
    BoomControl_init(&boom, &config);
    waitingForNeutral = 0;

    /* The requirements, as verification rules */
    oneCoil = CppModel_ruleAlways(self, "one coil at a time", 0.0);
    noMotionOnBadSignal = CppModel_ruleAlways(self, "no motion on a bad signal", 0.0);
    faultDetected = CppModel_ruleResponse(self, "fault within 200 ms", 0.2, 0.0);
    restartFromNeutral = CppModel_ruleAlways(self, "restart only from neutral", 0.0);

    /* The signals the workspace marks red where a rule fails */
    CppModel_ruleSignal(noMotionOnBadSignal, "Coil Down [mA]");
    CppModel_ruleSignal(faultDetected, "Fault");
    CppModel_ruleSignal(restartFromNeutral, "Coil Up [mA]");
}

void RunCyclic(CModelSimulation_ts *self, unsigned long currentTime_ms)
{
    const BoomControlConfig_ts *c = &boom.config;
    uint8_t badSignal, inNeutral, moving;

    boom.joystick_mV = CppModel_getInputU16(self, "Joystick [mV]", (uint16_t)joystickAt(currentTime_ms));

    BoomControl_cyclic(&boom, (uint16_t)self->stepTime_ms);

    CppModel_setOutputI8(self, "Command [%]", boom.command_pct);
    CppModel_setOutputU16(self, "Coil Up [mA]", boom.coilUp_mA);
    CppModel_setOutputU16(self, "Coil Down [mA]", boom.coilDown_mA);
    CppModel_setOutputU8(self, "Fault", boom.fault);

    /* What the requirements are about, worked out from the signals, not from the controller's state */
    badSignal = boom.joystick_mV < c->joystickMin_mV - c->signalMargin_mV ||
                boom.joystick_mV > c->joystickMax_mV + c->signalMargin_mV;
    inNeutral = !badSignal && boom.joystick_mV >= (c->joystickMin_mV + c->joystickMax_mV) / 2 - c->deadband_mV &&
                boom.joystick_mV <= (c->joystickMin_mV + c->joystickMax_mV) / 2 + c->deadband_mV;
    moving = boom.coilUp_mA > 0 || boom.coilDown_mA > 0;
    if (badSignal)
        waitingForNeutral = 1;
    else if (inNeutral)
        waitingForNeutral = 0;

    CppModel_updateAlways(oneCoil, CppModel_condition(!(boom.coilUp_mA > 0 && boom.coilDown_mA > 0)));
    CppModel_updateAlways(noMotionOnBadSignal, CppModel_implies(CppModel_condition(badSignal), CppModel_condition(!moving)));
    CppModel_updateResponse(faultDetected, badSignal, boom.fault);
    CppModel_updateAlways(restartFromNeutral, CppModel_implies(CppModel_condition(waitingForNeutral), CppModel_condition(!moving)));
}

int main(void)
{
    CModelSimulation_ts *simulation = CppModel_create("BoomControl", 17000, 10);
    if (simulation == NULL)
        return 1;

    CppModel_setInitFunction(simulation, Init);
    CppModel_setRunStepFunction(simulation, RunCyclic);
    CppModel_Simulate(simulation);

    int result = CppModel_getSimulationResult(simulation);
    CppModel_destroy(simulation);

    return result;
}
