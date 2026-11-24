# Excavator Boom Control

A joystick-to-hydraulic-valve boom control function in C, tested in a [CppModel](https://www.cppmodel.com) simulation with verification rules.

This is the code of the tutorial [Excavator Boom Control](https://www.cppmodel.com/docs/ExcavatorBoomControl). The tutorial walks through it step by step, shows the results and explains the fix.

> The function is written to be read, not certified. Do not use it in a real machine.

## The function

An electrical joystick on a 0–5 V analog input gives 0.5 V at full down, 2.5 V in neutral and 4.5 V at full up. The boom valve has two coils on current-controlled outputs: the valve starts to open at 300 mA and is fully open at 700 mA. Every 10 ms cycle, the function:

1. **Checks the signal.** A joystick that reads more than 250 mV outside its range is broken (a cut wire reads 0 V). A bad signal that lasts 100 ms latches a fault.
2. **Turns the voltage into a command** of −100 % (full down) to +100 % (full up), with a ±200 mV neutral zone.
3. **Drives one coil**, scaled to 300–700 mA. In neutral both coils are off.

## Requirements and verification

| Requirement | Verification rule |
| ----------- | ----------------- |
| Only one coil is ever energized. | `one coil at a time` (Always) |
| The boom never moves on a bad joystick signal. | `no motion on a bad signal` (Always) |
| A bad signal raises the fault within 200 ms. | `fault within 200 ms` (Response) |
| After a fault, the boom moves again only once the joystick has been back in neutral. | `restart only from neutral` (Always) |

The simulation plays the operator: neutral, a sweep from full down to full up, then the signal wire breaks for 1 s while the joystick is pushed up, comes back with the joystick still pushed up, and the operator returns to neutral and moves up again.

## Results

Every rule passes, and the program exits with 0:

```
CppModel.Verify.one coil at a time: PASS held at 1701 checked step(s)
CppModel.Verify.no motion on a bad signal: PASS held at 1701 checked step(s)
CppModel.Verify.fault within 200 ms: PASS 1 trigger(s) answered, slowest in 0.09 s (deadline 0.2 s)
CppModel.Verify.restart only from neutral: PASS held at 1701 checked step(s)
CppModel.Verify: 4 of 4 rules passed
```

## The bug the simulation found

The first version of `BoomControl.c` (tag [`0.0.1`](../../tree/0.0.1)) failed two rules:

```
CppModel.Verify.no motion on a bad signal: FAIL violated 1 time(s), 0.09 s in total, first t=12 s–12.09 s
CppModel.Verify.restart only from neutral: FAIL violated 2 time(s), 1.09 s in total, first t=12 s–12.09 s
```

- **A broken wire drove the boom full down.** The cut wire reads 0 V, which the scaling clamped to 0.5 V: full down. The fault only latched after its 100 ms delay, so for 90 ms the down coil got 700 mA.
- **The boom restarted on its own.** The fault cleared as soon as the signal was valid again, so the boom moved with the joystick still pushed up.

The fix holds the boom as soon as the signal is bad, without waiting for the fault, and clears the fault only with the joystick back in neutral. To see the failing run, check out `0.0.1` and run it again. The [tutorial](https://www.cppmodel.com/docs/ExcavatorBoomControl#the-fix) shows both runs on a graph.

## Files

| File | What it is |
| ---- | ---------- |
| `BoomControl.h`, `BoomControl.c` | The controller under test: plain C, no dependency on CppModel |
| `BoomControlSimulation.c` | The simulation: operator scenario, parameters and verification rules |
| `CMakeLists.txt` | Builds the simulation and registers it as a CTest test |

## Build and run

You need a C compiler, CMake 3.21 or later, and the CppModel SDK 0.18.0 or later.

1. Log in to [CppModel Workspace](https://workspace.cppmodel.com) (a free account works), open the `Download` menu and download the SDK for your platform.
2. Extract it into a `dependencies` folder here. On Linux and macOS:

   ```bash
   mkdir -p dependencies
   tar -xzf CppModel-x.x.x-Linux-x86_64.tar.gz -C dependencies --strip-components=1
   ```

   On Windows, extract the `.zip` and move the contents of its top-level folder into `dependencies`. See [Installation](https://www.cppmodel.com/docs/Installation).
3. Build and run:

   ```bash
   cmake -S . -B build
   cmake --build build
   ./build/BoomControlSimulation
   ```

On the first run, the program prints a login link; open it to connect to your workspace. When the run finishes, it prints each rule's outcome and a link to the results, where the signals and the rules are drawn on a graph. The exit code is 0 only when every rule passed, so `ctest --test-dir build` runs it as a test.

In CI, set `CPPMODEL_USERNAME` and `CPPMODEL_PASSWORD` instead of the browser login (see [Command Line and Environment](https://www.cppmodel.com/docs/CommandLineTool)).

## Parameters

The controller's configuration is read as parameters, so it can be changed in the workspace, or from a file with `--inputs`, without rebuilding. For example, save this as `slow-fault.json` to try a fault delay that is too long for the 200 ms requirement:

| Parameter | Default |
| --------- | ------- |
| `JoystickMin [mV]`, `JoystickMax [mV]` | 500, 4500 |
| `Deadband [mV]` | 200 |
| `SignalMargin [mV]` | 250 |
| `CoilMin [mA]`, `CoilMax [mA]` | 300, 700 |
| `FaultDelay [ms]` | 100 |

```json
{ "parameters": { "FaultDelay [ms]": 250 } }
```

```bash
./build/BoomControlSimulation --inputs slow-fault.json
```

## License

[MIT](LICENSE)
