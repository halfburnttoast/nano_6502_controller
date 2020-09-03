# nano_6502_controller
The Arduino portion of my 65C02 SBC

The Arduino Nano on my 65C02 SBC provides four basic functions:
- Initial RESET signal.
- PH0 clock generation.
- Memory-mapped serial IO.
- Memory-mapped random number generator.

Because the computer piggy-backs off of the Arduino's serial output, the entire system needs to be synchronized with the Arduino to work. Thus, internal interrupts from the Arduino will pause the output clock signal to the 65C02. 

There are pros and cons to this approach:

Pros:
- The Arduino replaces and consolidates additional components needed for serial I/O.
- The Arduino can take advantage of its (and connecting computer's) internal buffers, thus no control signals are needed when sending or recieving IO.
- The "firmware" of the computer (this Arduino code) can be changed to add additional functions without needing more components.

Cons:
- SLOW. The Arduino's PH0 clock output will only ever be as fast as the slowest function called within loop() (based on the 16MHz clock).
- Non-stable clock output: Arduino requires interrupts to function for serial, thus an interrupt will pause the PH0 clock output. 
- Somewhat complicated to optimize and makes it harder to read. 

I have optimized the code (along with adjusting the compiler optimization settings) to achieve an average PH0 clock output, according to my oscilloscope, of 1.66MHz (maximum ~2MHz, minimum ~200KHz). Without the optimizations, the PH0 output is around 80KHz. The true speed is hard to calculate, and programs requesting serial IO will run slower than programs that don't. 
