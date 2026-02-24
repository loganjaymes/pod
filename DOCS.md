# DOCS
## BoM
- Raspberry Pi Pico
- `n` piezoelectric sensors (`n` = number of drums you'd like, plus 1 for an interrupt)
- Rubber pads (i used two $7 rugs)
- Breadboard
- Wires
- `n` 3.3V zener diodes
- `n` 1M ohm resistors
- 8x1 or 4x1 multiplexor (I need to get a dedicated one, since the implementation as of 02/19/2026 uses the CD74HC4066E Quad Bilateral Switch)

## Circuitry
TinkerCAD circuit will be uploaded once I get a chance. For now, see below: </br> 
<img width="730" height="974" alt="image" src="https://github.com/user-attachments/assets/7e45bd59-635b-4bd3-b8ca-cd7f271a620a" /> </br>
Although unclean, for a simple, fully working implementation, just run each piezoelectric sensor into a zener diode, resistor and ADC (in that order). </br>
### Where's My Mux?
Since I'm not satisfied with how the mux is implemented as of now (since there are issues related to lacking a fast enough voltage bleed), I've refrained from including it in the previous explanation, 
as well as the main branch. </br>
</br>
For context, though (and if someone happening to read this knows how to resolve the issue): </br>
the switch is powered into its VCC by the Pico's 3.3V OUT pin, </br> 
the switch is grounded into a GND on the Pico, </br> 
the switch's INs/Ys have piezo's plugged into them after a resistor and diode, </br>
the switch's ENABLEs/Es are plugged into digital GPIO pins (since there is no binary select, this behaves as one), </br>
the switch's OUTs/Zs are connected to the a row on the breadboard, then ran through a diode and resistor, then into ADC0. </br>
<img width="392" height="429" alt="image" src="https://github.com/user-attachments/assets/946ccacb-1364-4ef5-aecb-711c8d947924" />  </br>
Code-wise, found in the `mux-test` branch, it turns each channel on the switch OFF to prevent short-circuiting, iterates over each GPIO to behave as a binary select, stores the current voltage, 
discharges any leftover voltage, and then determines which voltage is the highest (and therefore the drum "hit"). 
This determines which MIDI note is sent, but I only want to merge the aforementioned code into the `main` branch once I know the circuit is both accurate and safe in the long term.

## Known Issues
As stated prior, there is an issue relating to my current mux implementation. A friend and I believe it to be hardware related, as we tried an iteration including a bleed resistor, and that still does not
have it drain fast enough. This results in a `mux_vals[]` of, for example `{.873, .355, .376, .455}`, rather than `{.873, 0, 0, 0}`. Another iteration we created had a worse issue, where all `mux_vals[]` would be scaled by a variable factor EXCEPT
the value actually hit.
Currently, it does work, but again, it is a bit hacky and potentially dangerous to components, so I am actively troubleshooting as a result.
I want to purchase a dedicated analog multiplexer regardless, but if I can get a safe version of my current prototype, I will be happy. </br>
