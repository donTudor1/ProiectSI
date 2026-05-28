# Proiect Dual-Servo — cap pan-tilt

Aplicație pe **Arduino Nano/Uno (ATmega328P)** care comandă două servomotoare Tower Pro 9g
cu trei moduri de mișcare comutabile. Implementarea este doar în `src/main.c`; driverele din
`drivers/` rămân nemodificate.

## Conexiuni

| Componentă | Pin | Notă |
|---|---|---|
| Servo pan | D9 | semnal PWM (Timer1 OC1A) |
| Servo tilt | D10 | semnal PWM (Timer1 OC1B) |
| LED mod | D13 | onboard |
| Comutare mod (opțional) | D2 | fir scurt la GND (pull-up intern, INT0) |
| ADC zgomot | A0 | neconectat (pin flotant) |

**Alimentare:** roșu/maro servo la 5V/GND pe placă. Pentru demo fără sarcină pe ax e OK;
dacă servo-urile tremură sau placa se resetează, folosește sursă externă 5V cu **masă comună**
cu Arduino.

## Moduri

| Mod | Comportament | LED (după clipirea de intrare) |
|---|---|---|
| 0 | Baleiere sincronă 0°↔180° | stins |
| 1 | Pan = pos, tilt = 180 − pos | clipire lentă (~500 ms) |
| 2 | Ținte aleatoare din `ADC_Read(0)` | clipire rapidă (~150 ms) |

Comutare mod: **D2 → GND** (debounce 200 ms) sau, dacă `USE_BUTTON_MODE_SWITCH` este 0,
automat la fiecare 10 s.

## Build și flash

```bash
make all BOARD=nano
make flash BOARD=nano PORT=COM4
```

Pe Windows, portul tipic pentru CH340 este `COM4` (verifică în Device Manager).

## Mapare laboratoare

| Lab | Utilizare |
|---|---|
| 1 — GPIO | LED D13, configurare D2 |
| 2 — Întreruperi | INT0 pe D2, callback scurt cu flag |
| 3 — Timere / Millis | mișcare lină, debounce, LED neblocant |
| 4 — PWM | servo pe D9/D10, 50 Hz |
| 5 — ADC | A0 flotant → aleatorism mod 2 |

## Limitare PWM

Maparea unghi → duty 13..26 oferă ~13 trepte de unghi — suficient pentru demo.
Rezoluția fină pe `ICR1` (Timer1) este opțională bonus.
