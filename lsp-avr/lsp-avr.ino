/*
 * R: Timer0.A -> PD6 -> DigPin6
 * G: Timer0.B -> PD5 -> DigPin5
 * B: Timer2.A -> PB3 -> DigPin11
 * 
 * SERIAL Activity LED -> PB5
 */

#include <SoftwareSerial.h>
#include <TimerOne.h>

#include "vm.h"
#include "config.h"


#if CFG_AUDIO_SERIAL
    SoftwareSerial AudioSerial(10, -1);

    static void init_serial(){
        AudioSerial.begin(CFG_AUDIO_BAUDRATE);
        Serial.begin(9600);
    }
#else
    static void init_serial(){
        Serial.begin(115200);
    }
#endif


// lsp intruction memory, or simply "the program"
volatile byte lsp_imem[MAX_PROG_LEN];

volatile vm_state_t lsp_vm;

// 0 -> min, 255 -> max (MSB)
volatile unsigned short brightness = 0;
volatile unsigned short target_brightness = 0;

// Serial activity led counters
volatile unsigned short aled_cnt;
volatile unsigned short aled_phase_cnt;


void timer1_isr(){
    // Activity led
    if(aled_cnt){
        if(!aled_phase_cnt){
            PORTB ^= (unsigned char)(1 << PB5);
            aled_phase_cnt = CFG_ALED_PHASE_MS;
        }

        aled_phase_cnt--;
        aled_cnt--;
    } else {
        PORTB &= (unsigned char)~(1 << PB5);
    }

    // Brightness animation
    static unsigned short last_target_brightness = 0, brightness_count;
    static   signed short brightness_step = 0;
    if(brightness != target_brightness){
        if(brightness == 0){
            // enable PWM outputs (they were off)
            TCCR0A |= (1 << COM0A1) | (1 << COM0B1);
            TCCR2A |= (1 << COM2A1);
        }
        
        if(target_brightness != last_target_brightness){
            // Recompute deltas
            last_target_brightness = target_brightness;

            register signed short h_target   = target_brightness >> 1;
            register signed short h_bright   = brightness >> 1;
            register signed short s_duration = CFG_BRIGHTNESS_ADJ_MS;

            brightness_step  = ((h_target - h_bright) / s_duration) * 2;
            brightness_count = CFG_BRIGHTNESS_ADJ_MS;
        }

        brightness += brightness_step;
        brightness_count--;

        if(!brightness_count){
            brightness = target_brightness;

            if(brightness == 0){
                // disable PWM outputs (they were on)
                
                TCCR0A &= ~((1 << COM0A1) | (1 << COM0B1));
                TCCR2A &= ~(1 << COM2A1);
            }
        }
    }
    
    vm_step(lsp_vm, false);
}



void setup() {
    // Set PD5, PD6, PB3 e PB5 as outputs
    DDRD |= (1 << DDD5) | (1 << DDD6);
    DDRB |= (1 << DDB3) | (1 << DDB5);
    
    // Sets up timer0 (R and G) and timer2 (B)
    TCCR0A = (1 << COM0A1) | (1 << COM0B1) | (1 << WGM00) | (1 << WGM01);
    TCCR0B = 1 << CS00;

    TCCR2A = (1 << COM2A1) | (1 << WGM20) | (1 << WGM21);
    TCCR2B = 1 << CS20;

    // Initial duty cycle is 0%
    OCR0A = 0;
    OCR0B = 0;
    OCR2A = 0;

    // Disable PWM outputs (yes i enabled them in the setup up there)
    TCCR0A &= ~((1 << COM0A1) | (1 << COM0B1));
    TCCR2A &= ~(1 << COM2A1);

    init_serial();
    OUT_SERIAL.println("lsp init v1.1");

    vm_reset(lsp_vm, lsp_imem);

    //
    Timer1.initialize(1000);
    Timer1.attachInterrupt(timer1_isr);

    // This is the serial console state
    unsigned int   imem_len = 0;
    unsigned short saved_brightness = 0;
    bool           output_enabled = false;
    
    while(1){
      _loop_start:
        char cmd = IN_SERIAL.read();
        if(cmd < 0) continue;

        switch(cmd){
            case '\n':
            case '\t':
            case ' ':
                continue;

            case '(':
                target_brightness = saved_brightness;
                output_enabled = true;
                
                OUT_SERIAL.println("Out: on");
                break;
               
            case ')':
                saved_brightness = target_brightness;
                target_brightness = 0;
                output_enabled = 0;
                
                OUT_SERIAL.println("Out: off");
                break;

            case 'b': {
                byte wanted_brightness = IN_SERIAL.parseInt();
                if(output_enabled){
                    target_brightness = wanted_brightness << 8;
                } else {
                    saved_brightness = wanted_brightness << 8;
                }
                break;
            }

            case 'I': {
                byte ivect          = IN_SERIAL.parseInt();
                unsigned short iarg = IN_SERIAL.parseInt();
                vm_request_interrupt(lsp_vm, ivect, iarg);
                
                OUT_SERIAL.println("Requesting interrupt");
                break;
            }

            case '[':
                vm_set_pause(lsp_vm, true);
                
                OUT_SERIAL.println("VM pause");
                break;

            case ']':
                vm_set_pause(lsp_vm, false);

                OUT_SERIAL.println("VM unpause");
                break;

            case 'r':
                imem_len = 0;
                break;

            case 'w':
                lsp_imem[imem_len++] = IN_SERIAL.parseInt();
                break;

            case 'R':
                vm_reset(lsp_vm, lsp_imem);

                OUT_SERIAL.println("VM reset");
                break;

            case 's':
                vm_step(lsp_vm, true);
                break;

            case 'O':
                OUT_SERIAL.println("Manual Override");
                OCR0A = IN_SERIAL.parseInt();
                OCR0B = IN_SERIAL.parseInt();
                OCR2A = IN_SERIAL.parseInt();
                break;
            
            case 'D':
                for(unsigned short i = 0;i < imem_len;i += 8){
                    if(i < 0x1000U) OUT_SERIAL.print('0');
                    if(i < 0x100U)  OUT_SERIAL.print('0');
                    if(i < 0x10U)   OUT_SERIAL.print('0');
                    OUT_SERIAL.print(i, HEX);
                    OUT_SERIAL.print(": ");
                    for(unsigned int j = 0;j < 8 && (i + j) < imem_len;j++){
                        if(lsp_imem[i + j] < 0x10) OUT_SERIAL.print('0');
                        OUT_SERIAL.print(lsp_imem[i + j], HEX);
                        OUT_SERIAL.print(' ');
                    }
                    OUT_SERIAL.println();
                }
                break;

            case '?':
              #if CFG_ENABLE_HELP
                OUT_SERIAL.println("lsp cmdline");
                OUT_SERIAL.println("  (              on (restore saved brightness)");
                OUT_SERIAL.println("  )              off (save bright. and set to 0)");
                OUT_SERIAL.println("  b<n>           set bright. (set saved if off)");
                OUT_SERIAL.println("  I<v> <a>       req. interrupt v, w/ arg a");
                OUT_SERIAL.println("  [              pause VM");
                OUT_SERIAL.println("  ]              unpause VM");
                OUT_SERIAL.println("These cmds have undefined behaviour if used with running VM");
                OUT_SERIAL.println("  r              Rewind imem write index to 0 (to rewrite pgm)");
                OUT_SERIAL.println("  w <byte>       Write byte to imem and increments index");
                OUT_SERIAL.println("  R              Reset VM state");
                OUT_SERIAL.println("  s              Single step VM, with debug info printed here");
                OUT_SERIAL.println("  O <r> <g> <b>  Manual output override");
                OUT_SERIAL.println("  D              Dump imem");
              #endif
                break;

            default:
                OUT_SERIAL.print("unk inst ");
                OUT_SERIAL.println(cmd);
                goto _loop_start;
        }

        // A command was received, reset the activity led counter
        aled_cnt = CFG_ALED_TIMEOUT_MS;
    }
}
