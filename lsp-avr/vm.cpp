#include <Arduino.h>

#include "vm.h"
#include "config.h"


void vm_reset(volatile vm_state_t& vm, byte* imem){
    // Initially paused
    vm.is_paused = 1;

    // No pending interrupt
    vm.int_req = VM_INT_NONE;

    // Program memory
    vm.imem = imem;

    // Reset registers
    for(byte i = 0;i < 4;i++){
        vm.regs.w[i] = 0;
        vm.outs.w[i] = 0;
        vm.saved_regs.w[i] = 0;
        vm.saved_outs.w[i] = 0;
    }

    // Reset the program counter
    vm.pc = 0;
    vm.saved_pc = 0;
}


void vm_set_pause(volatile vm_state_t& vm, bool pause){
    if(pause){
        vm.is_paused = true;
        while(!vm.is_paused_ack) delay(10);
    } else {
        vm.is_paused = false;
    }
}


void vm_request_interrupt(volatile vm_state_t& vm, byte ivect, unsigned short arg){
    if(vm.int_req != VM_INT_NONE) return;

    vm.int_vector = ivect;
    vm.int_arg    = arg;

    vm.int_req = VM_INT_PENDING;
}



static inline void _vm_copy_regs(volatile vm_regs_t& dst, volatile vm_regs_t& src){
    dst.w[0] = src.w[0];
    dst.w[1] = src.w[1];
    dst.w[2] = src.w[2];
    dst.w[3] = src.w[3];
}

void vm_step(volatile vm_state_t& vm, bool debug){
    // Pause and debug check because the debug mode is intended
    // to be used with the VM paused
    if(vm.is_paused && !debug){
        vm.is_paused_ack = true;
        return;
    }
    
    vm.is_paused_ack = 0;
    
    register byte op_is_word;
    register byte op_ro;
    register byte op_ro_b;  // byte offset, that is op_ro << 1, to address vm_regs_t.b

    register          byte  tmp;
    register unsigned short tmpw;
    
    static union {
        unsigned char ub[2];
          signed char  b;
          signed short w;
    } stmp;

    // Check pending interrupts
    if(vm.int_req == VM_INT_PENDING){
        // Search for the vector
        byte int_entry = (vm.int_vector << 4) | VM_OP_IVEC;
        unsigned short i;

        bool enter_interrupt = false;
        
        for(i = 0;i < MAX_PROG_LEN;i++){
            if(vm.imem[i] == int_entry){
                enter_interrupt = true;
                break;
            }

            // Skip instruction's data
            switch(vm.imem[i] & 0B00001111){
                // No data
                case VM_OP_STOP:
                case VM_OP_COMMIT:
                case VM_OP_WAIT:
                case VM_OP_IVEC:
                case VM_OP_IRET:
                    i += 0;
                    break;

                // A single byte
                case VM_OP_JMP:
                    i += 1;
                    break;

                // Depends on is_word
                default:
                    i += (vm.imem[i] >> 7) ? 2 : 1;
                    break;
            }
        }

        if(enter_interrupt){
            // Save regs and current program counter
            _vm_copy_regs(vm.saved_regs, vm.regs);
            _vm_copy_regs(vm.saved_outs, vm.outs);
            vm.saved_pc   = vm.pc;

            // Set %A
            vm.regs.w[0] = vm.int_arg;

            // Jump to the interrupt
            vm.pc = i;
            
            vm.int_req = VM_INT_ONGOING;
        } else {
            if(debug && CFG_DO_DEBUG){
                PLN("Requested interrupt but no vector found");
            }
            
            vm.int_req = VM_INT_NONE;
        }
    }

    // Run the VM until an hlt, cmt or wt instruction are executed
    while(true){
        tmp = vm.imem[vm.pc++];

        op_is_word =  tmp >> 7;
        op_ro      = (tmp >> 4) & 3;
        op_ro_b    = op_ro << 1;

        switch(tmp & 0B00001111){
            case VM_OP_STOP:
                vm.is_paused = true;

                if(debug && CFG_DO_DEBUG) PLN("VM_OP_STOP");
                return;

            case VM_OP_SETREG:
                // LSB
                vm.regs.b[op_ro_b + 0] = vm.imem[vm.pc++];
                // MSB
                tmp = op_is_word ? vm.imem[vm.pc++] : 0;
                vm.regs.b[op_ro_b + 1] = tmp;

                if(debug && CFG_DO_DEBUG){
                    PR("VM_OP_SETREG@");
                    PR(op_is_word ? "16" : "8");
                    PR(" reg ");
                    PR(op_ro);
                    PR(" = ");
                    PLN(vm.regs.w[op_ro]);
                }
                break;

            case VM_OP_SETOUT:
                vm.outs.b[op_ro_b + 0] = op_is_word ? vm.imem[vm.pc++] : 0;
                vm.outs.b[op_ro_b + 1] = vm.imem[vm.pc++];

                if(debug && CFG_DO_DEBUG){
                    PR("VM_OP_SETOUT@");
                    PR(op_is_word ? "16" : "8");
                    PR(" out ");
                    PR(op_ro);
                    PR(" = ");
                    PLN(vm.outs.w[op_ro]);
                }
                break;

            case VM_OP_MODOUT:
                stmp.ub[0] = vm.imem[vm.pc++];
                
                if(op_is_word){
                    stmp.ub[1] = vm.imem[vm.pc++];
                    vm.outs.w[op_ro] += stmp.w;
                } else {
                    vm.outs.w[op_ro] += stmp.b;
                }
                
                if(debug && CFG_DO_DEBUG){
                    PR("VM_OP_MODOUT@");
                    PR(op_is_word ? "16" : "8");
                    PR(" reg ");
                    PR(op_ro);
                    PR(" += ");
                    PLN(op_is_word ? stmp.w : stmp.b);
                }
                break;

            case VM_OP_COMMIT: {
                extern unsigned short brightness;
                  
                tmpw  = vm.outs.b[1];  // MSB of vm.outs.w[0]
                tmpw *= brightness >> 8;
                OCR0A = tmpw >> 8;
                
                tmpw  = vm.outs.b[3];  // MSB of .w[1]
                tmpw *= brightness >> 8;
                OCR0B = tmpw >> 8;
                
                tmpw  = vm.outs.b[5];  // .w[2]
                tmpw *= brightness >> 8;
                OCR2A = tmpw >> 8;
                
                if(debug && CFG_DO_DEBUG){
                    PR("VM_OP_COMMIT RGB ");
                    PR(vm.outs.b[1]); PR(" ");
                    PR(vm.outs.b[3]); PR(" ");
                    PLN(vm.outs.b[5]);
                }
                return;
            }

            case VM_OP_WAIT:
                if(debug && CFG_DO_DEBUG) PLN("VM_OP_WAIT");
                return;

            case VM_OP_DRJNZ:
                vm.regs.w[op_ro]--;
                
                if(debug && CFG_DO_DEBUG){
                    PR("VM_OP_DRJNZ@");
                    PR(op_is_word ? "16" : "8");
                    PR(" reg ");
                    PR(op_ro);
                    PR(" jmp to ");
                    stmp.ub[0] = vm.imem[vm.pc + 0];
                    if(op_is_word){
                        stmp.ub[1] = vm.imem[vm.pc + 1];
                        PR(stmp.w);
                    } else {
                        PR(stmp.b);
                    }
                    if(vm.regs.w[op_ro] == 0) PR(" cont");
                    PLN();
                }
                
                if(vm.regs.w[op_ro] != 0){
                    stmp.ub[0] = vm.imem[vm.pc++];
                
                    if(op_is_word){
                        stmp.ub[1] = vm.imem[vm.pc++];
                        vm.pc += stmp.w;
                    } else {
                        vm.pc += stmp.b;
                    }
                } else {
                    vm.pc += 1 << op_is_word;  // +1 or +2
                }

                break;

            case VM_OP_JMP:
                stmp.ub[0] = vm.imem[vm.pc++];
                
                vm.pc += stmp.b;
                
                if(debug && CFG_DO_DEBUG){
                    PR("VM_OP_JMP ");
                    PR(" off ");
                    PLN(stmp.b);
                }
                break;

            case VM_OP_JMPABS:
                // LSB
                tmpw = vm.imem[vm.pc++];
                // MSB
                tmpw |= op_is_word ? (vm.imem[vm.pc] << 8) : 0;
                
                vm.pc = tmpw;

                if(debug && CFG_DO_DEBUG){
                    PR("VM_OP_JMPABS@");
                    PR(op_is_word ? "16" : "8");
                    PR(" dst ");
                    PLN(tmpw);
                }
                break;

            case VM_OP_IVEC:
                if(debug && CFG_DO_DEBUG){
                    PR("VM_OP_IVEC n ");
                    PLN(op_ro);
                }
                break;

            case VM_OP_ISETPC:
                // LSB
                tmpw = vm.imem[vm.pc++];
                // MSB
                tmpw |= op_is_word ? (vm.imem[vm.pc++] << 8) : 0;
                
                if(vm.int_req != VM_INT_ONGOING) break;
                
                vm.saved_pc = tmpw;
                
                if(debug && CFG_DO_DEBUG){
                    PR("VM_OP_ISETPC@");
                    PR(op_is_word ? "16" : "8");
                    PR(" dst ");
                    PLN(tmpw);
                }
                break;

            case VM_OP_IRET:
                if(vm.int_req != VM_INT_ONGOING) break;
                
                // Restore the registers and the prog counter
                _vm_copy_regs(vm.regs, vm.saved_regs);
                _vm_copy_regs(vm.outs, vm.saved_outs);
                vm.pc   = vm.saved_pc;

                // Reset the interrupt
                vm.int_req = VM_INT_NONE;
                
                if(debug && CFG_DO_DEBUG){
                    PLN("VM_OP_IRET");
                }
                break;
                
        }

        if(debug){
            if(CFG_DO_DEBUG){
                PR("int_mode "); PLN(vm.int_req);
                
                PR("regs");
                PR(" "); PR(vm.regs.w[0], HEX);
                PR(" "); PR(vm.regs.w[1], HEX);
                PR(" "); PR(vm.regs.w[2], HEX);
                PR(" "); PLN(vm.regs.w[3], HEX);
    
                PR("outs");
                PR(" "); PR(vm.outs.w[0], HEX);
                PR(" "); PR(vm.outs.w[1], HEX);
                PR(" "); PLN(vm.outs.w[2], HEX);
            }
            
            return;
        }
    }
}
