/* 
 *  WARNING: Optimizations for Arduino Nano controller ONLY
 *  To enable full speed optimization:
 *      Uncomment NANO_OPT and COMPILE_O3 header switches
 */

#define NANO_OPT            // Optimize for high-speed.
#define COMPILE_O3          // Compile for high-speed.

#ifdef COMPILE_O3
#pragma GCC push_options
#pragma GCC optimize ("O3")
#endif

typedef uint8_t  BYTE;
typedef uint16_t WORD;
enum PIN_MODE {DINPUT = 0, DOUTPUT = 1};

// ==== Begin Serial com vars ====
#define SERIAL_RATE 115200
#define SERIAL_INPUT_BUFFER_SIZE 128
#define SERIAL_OUTPUT_BUFFER_SIZE 128
char serial_input_buffer[SERIAL_INPUT_BUFFER_SIZE];
char serial_output_buffer[SERIAL_OUTPUT_BUFFER_SIZE];
// ==== End Serial com vars ====

#define RDY 2
#define CLK 3
#define N_RES 13
#define N_IOCE 12
#define ADDR0 A5   // PORTC 5
#define ADDR1 A4   // PORTC 4
#define DATA_WIDTH 8
const BYTE data_pins[] = {
	4, 5, 6, 7, 8, 9, 10, 11
};


// printf wrapper for serial output. Function is super slow and shouldn't be
// used for anything except debug messages.
void __attribute__((optimize("Os"))) sprint(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(serial_output_buffer, SERIAL_OUTPUT_BUFFER_SIZE, fmt, args);
    va_end(args);
    Serial.print(serial_output_buffer);
}

#ifndef NANO_OPT                            // use default abstracted functions 
void data_direction_input(void) {
	for(BYTE i = 0; i < 8; i++)
		pinMode(data_pins[i], INPUT);
#else                                       // use port registers
inline void data_direction_input(void) {
    /* Uses 2 instructions instead of 6. Only works because I figured out what possible
     *  states these pins can be in at this point of execution. Thus, no lookup or 
     *  bitwise operations are needed.
     */
    DDRD = 0x0C;                // bits 0x0C are always outputs
    DDRB = 0x20;                // bit 0x20 is always output
    /*
    DDRB = DDRB & 0xF0;
    DDRD = DDRD & 0x0F;
    */
#endif
}

#ifndef NANO_OPT                            // use default abstracted functions 
void data_direction_output(void) {
	for(BYTE i = 0; i < 8; i++)
		pinMode(data_pins[i], OUTPUT);
#else                                       // use port registers
inline void data_direction_output(void) {
    // This uses 4 instructions instead of 12. Same reasoning as data_direction_input
    DDRD = 0xFC;                // bits 0x0C will always be output, 0x3 are never used
    DDRB = 0x2F;                // bit  0x20 will always be output, 0xC0 are never used
    PORTD = 0x0F;               // data ports must still be set to low on state change
    PORTB = 0x20;               //  "
    /*
    DDRD = DDRD | 0xF0;
    DDRB = DDRB | 0x0F;
    PORTD = PORTD & 0x0F;
    PORTB = PORTB & 0xF0;
    */
#endif
}

inline void clk_pulse(void) {
#ifndef NANO_OPT
    digitalWrite(CLK, LOW);
    digitalWrite(CLK, HIGH);
#else
/*
    /*  This saves two instructions and a register over port manipulation. 
     *   The compiler will do another 'in' instruction which is unnecessary.
    __asm__ __volatile__ (
        "in r24, 0xB    \n\t"           // PORTD->R24
        "andi r24, 0xF7 \n\t"           // R24 &= 0xF7 - sets clock bit to low
        "out 0xB, r24   \n\t"           // R24->PORTD
        "ori r24, 0x8   \n\t"           // R24 |= 0x8  - sets clock bit back to high
        "out 0xB, r24   \n\t"           // R24->PORTD
        :
        :
        : "r24"
    );
    */
    
    /* Found these opcodes in the datasheet. Does the same as above with two instructions.
     *  which translates to about two AVR cpu cycles. This increases the PH0 output from
     *  2MHz to about 2.28MHz. I doubt it's possible for the AVR to go any faster without
     *  losing synchronization with the 65C02.
     */
    __asm__ __volatile__ (
        "cbi 0xB, 0x3   \n\t"
        "sbi 0xB, 0x3   \n\t"
        ::
    );
    //PORTD = PORTD ^ 0x8;   
    //PORTD = PORTD ^ 0x8;
#endif
}


void setup(void) {
    Serial.begin(SERIAL_RATE);
    sprint("\r\n\r\n\r\n# -- TPC65 Controller RESET --\r\n");
    sprint("# Serial Enabled.\r\n");
    randomSeed(analogRead(1));
    sprint("# Random seed\r\n");
	
	// setup pins
    sprint("# Setting initial pin states.\r\n");
	data_direction_input();
	pinMode(RDY, OUTPUT);
	pinMode(CLK, OUTPUT);
	pinMode(N_RES, OUTPUT);
	pinMode(N_IOCE, INPUT);
	pinMode(ADDR0, INPUT);
	pinMode(ADDR1, INPUT);

    // set initial states
	digitalWrite(N_RES, HIGH);
    sprint("# Reset start\r\n");
    digitalWrite(CLK, HIGH);
    digitalWrite(RDY, HIGH);
    sprint("# Setup Complete.\r\n");
}

void __attribute__((flatten)) loop(void) {
    digitalWrite(N_RES, LOW);
    BYTE reset_timer = 50;
    sprint("# Reset timer: %d\r\n", reset_timer);
    BYTE ctrl = 0;
    sprint("# Starting CPU...\r\n");
    while(1) {
        clk_pulse();
        reset_timer--;
        if(reset_timer == 0) {
            digitalWrite(N_RES, HIGH);
            sprint("# Reset end\r\n");
            sprint("# ---------------------\r\n");
            break;
        }
    }
    while(1) {
#ifndef NANO_OPT
        if(digitalRead(N_IOCE) == 0) {
#else
        if((PINB & 0x10) == 0) {
#endif
#ifndef NANO_OPT
            ctrl = 0 | (digitalRead(ADDR0) << 1) | digitalRead(ADDR1);
            ctrl = ctrl << 4;
#else
            ctrl = PINC & 0x30;
#endif
            switch(ctrl) {
                case 0x10: {
                    BYTE dout = 0;
                    data_direction_output();
                    if(Serial.available()) {
                        dout = Serial.read();
                        if(dout >= 0x61)
                            dout = dout - 0x20;    
#ifndef NANO_OPT                    
                        for(int i = 0; i < 8; i++) 
                            digitalWrite(data_pins[i], (dout >> i) & 0x1);
#else
                        PORTD = PORTD | ((dout & 0x0F) << 4);
                        PORTB = PORTB | ((dout & 0xF0) >> 4);
#endif
                    }
                    data_direction_input();         // ISSUE: possible race condition between data direction change and clock pulse out
                    break;
                } 
                case 0x20: {
                    uint8_t din = 0;
#ifndef NANO_OPT
                    for(uint8_t i = 0; i < DATA_WIDTH; i++)
                        din = din | ((digitalRead(data_pins[i]) & 0x1) << i);
#else
                    din = ((PIND & 0xF0) >> 4) | ((PINB & 0x0F) << 4);
#endif
                    Serial.write(din);
                    break;
                }
                case 0x30: {
                    BYTE dout = random(256);
                    data_direction_output();
#ifndef NANO_OPT
                    for(int i = 0; i < 8; i++) 
                        digitalWrite(data_pins[i], (dout >> i) & 0x1);
#else
                    PORTD = PORTD | ((dout & 0x0F) << 4);
                    PORTB = PORTB | ((dout & 0xF0) >> 4);    
#endif            
                    data_direction_input(); 
                    break;                
                }
            }
        } 
        // perform one clock pulse per loop. 
        clk_pulse();
    }
}
#ifdef COMPILE_O3
#pragma GCC pop_options
#endif
