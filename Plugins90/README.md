# Xaza FX — 90 plugins de audio (VST3, JUCE)

90 efectos independientes, cada uno un `.vst3` propio ("Xaza <Nombre>"), todos con **Mix** y **Output dB** además de sus parámetros.
El DSP (`Dsp.h`, `Fx*.h`) no depende de JUCE; `Processor.cpp` es el envoltorio genérico (interfaz genérica del host) y `Registry.h` la lista de los 90.

| Categoría | Plugins |
|---|---|
| Filters (12) | Low Pass, High Pass, Band Pass, Notch, Low Shelf, High Shelf, Peak EQ, Auto Wah, Vowel, Comb Filter, DJ Filter, Tilt EQ |
| Dynamics (12) | Compressor, Limiter, Gate, Expander, De-Esser, Transient Shaper, Upward Comp, Pump, Leveler, Tremolo, Squash, Peak Clipper |
| Distortion (14) | Saturator, Tube, Tape, Hard Clip, Soft Clip, Foldback, Bitcrusher, Decimator, Rectifier, Fuzz, Overdrive, Sine Shaper, Asym Distortion, Exciter |
| Modulation (14) | Chorus, Flanger, Phaser, Vibrato, Auto Pan, Ring Mod, Rotary, Ensemble, Freq Shifter, Wobble, Harmonic Tremolo, Dimension, Pitch Shifter, Doubler |
| Time (12) | Delay, Ping Pong, Slapback, Tape Echo, Multi Tap, Reverb, Room, Hall, Plate, Spring, Stutter, Reverse |
| Stereo (12) | Stereo Width, Mid Side, Mono Maker, Pan, Gain, Polarity, Channel Swap, Haas, Crossfeed, Stereo Rotate, Balance, Decorrelator |
| Creative (14) | Tape Stop, Lo-Fi, Vinyl, Radio, Telephone, Megaphone, Robot, Underwater, Gater, Sample Hold Filter, Glitch, Shimmer, Resonator, Freeze |

## Compilar

```
cmake -S Plugins90 -B build -DCMAKE_BUILD_TYPE=Release            # los 90 (tarda: JUCE se compila una vez por plugin)
cmake -S Plugins90 -B build -DXFX_ONLY=58                          # solo uno (n.º de plugins.txt)
cmake -S Plugins90 -B build -DXFX_CATEGORY=Dynamics                # solo una categoría
cmake --build build --config Release --parallel
```
En GitHub: el workflow `Compilar Xaza FX` compila las 7 categorías en paralelo (Windows) y sube los `.vst3`.

## Prueba del DSP (sin JUCE)

```
g++ -std=c++17 -O2 -fsanitize=address,undefined -IPlugins90 Plugins90/test/dsp_test.cpp -o dsp_test && ./dsp_test Plugins90/plugins.txt
```
Comprueba, a 44.1/48/96 kHz con parámetros por defecto, mínimos, máximos y aleatorios/en movimiento: sin NaN/Inf, sin niveles descontrolados y que `plugins.txt` coincide con el registro.

## Añadir un efecto
1. Clase `Fx` (o `PerCh<S>`) en `FxA/B/C.h`. 2. Línea `add(...)` en `Registry.h`. 3. `dsp_test --list > plugins.txt`.

## Límites conocidos
Sin oversampling en las distorsiones (puede haber aliasing); los efectos de pitch/reverse tienen latencia inherente que no se reporta al host; solo estéreo.
