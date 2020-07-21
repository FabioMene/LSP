#ifndef LSP_DEFS_H
#define LSP_DEFS_H 1

#define MAX_PROG_LEN 512

/*
    This is a "simple" 16 bit CISC virtual machine with a very basic
    instruction set which drives the LED outputs. The machine
    has 4 16-bit general purpose registers and 3 16-bit output registers.
    The VM is intended to run simple time based animations with the aid of
    two instructions which halt the machine until the next timer interrupt,
    fired every millisecond ('cmt' and 'wt')

    In the assembler source the registers are prefixed with a literal % (%A..%D),
    the output regs are prefixed with a literal @ (@R, @G and @B)
    There are 3 types of registers
      Normal registers (%A..%D) 16 bits    General purpose. %A is used to pass the interrupt argument to the int vector
      Output registers (@R..@B) 16 bits    These hold the color intensity values to be output. The output brightness is not involved in this
      PWM registers              8 bits    Not visible in the program. Writing on these directly sets the 3 channels' output brightness
    From now on normal registers are called 'regs', output registers 'oregs' and PWM registers 'pregs'

    All registers hold a 16 bit unsigned value. When the oregs are latched to the PWM output (with the 'cmt' instruction) the MSB of the
    oregs is multiplied by the MSB of the brightness (tracked outside of the VM), then the MSB of the result is written to the pregs.
    This makes smooth animations possible due to the lower 8 bit of the oregs which behave like a fractional
    part of the integer output value

    Some instructions take no data, some take one byte and some can take one
    or two bytes (see vm_opcode_e, 'data width' column). Two byte values are
    in little endian

    The instructions can take 8/16-bit values, which are of type:
        val: signed value
        uval: unsigned value
        addr: signed address offset, from the byte immediatly after this instruction
        uaddr: unsigned address

    The instruction encoding is:

      typedef struct {
          bool           is_word:1; // If the opcode requires variable length data that will be 16 bit wide if true, or 8 bit if false
          unsigned char  _pad0:1;   // Don't care
          unsigned int   ro:2;      // reg/oreg (00: A, 01: B, 10: C, 11: D / 00: R, 01: G, 10: B, 11: reserved)
          opcode_codes_e op:4;      // The operation itself
      } instruction_t;

    This struct is packed in a byte and represents an instruction, bit order is from the most significant to the least significant
    avr-gcc doesn't have the required pragmas/attributes to correctly pack this struct into the said byte (as of ago 2019)
    so we use simple shifts and bitmasks

    You can force a mnemonic to be encoded to a specific width by appending a suffix to the mnemonic itself,
    b/w for 8/16 bits respectively.

    For most mnemonics, when they're not suffixed, the data width is automatically chosen (smallest possible)
    The only exception to this is the 'so' instruction, which requires an explicit suffix.
    That's because the 8 bit variant of the instruction (sob @oreg, uval) sets the high byte of the output register
    (and sets the low byte to 0); for example 'sob @R, 1' does not translate to R = 1, but to R = 256 (1 << 8).
    This simplifies a couple of things if you think about how the VM maps the 16-bit oregs to the 8-bit pregs

    An interrupt can be requested via the serial command 'I<v> <a>'; an interrupt entry point is an instruction with
    VM_OP_IVEC as opcode and the interrupt vector encoded in the .ro field. The instruction itself is a no-op, but
    when an interrupt is requested the VM saves the current program counter, regs and oregs to another location,
    sets %A to the 'a' passed to the command and jumps to the interrupt vector 'v'. An interrupt can do whatever it wants,
    but other interrupt requests are ignored while an IRQ is being serviced. An interrupt handler has access to two more
    instructions, 'iret' and 'isetpc'. iret simply ends the interrupt, restoring saved regs/oregs and program counter.
    The handler can set the saved program counter with the 'isetpc' instruction, making it capable of setting where the VM
    will resume it's operation when exiting the interrupt

    Down below there are some handwritted test programs. I've also written a compiler (lspc), decompiler (lspd) and emulator (lspemu)
    inside the lspvm-asm folder and some assembly programs ready to be compiled in the progs folder
*/

typedef enum {         // Data width  ASM              Operation
    VM_OP_STOP   = 0,  // 0           hlt              Halts the VM
    VM_OP_SETREG = 1,  // 8/16        sr reg, uval     Sets a register (A..D =)
    VM_OP_SETOUT = 2,  // 8/16        so oreg, uval    Sets an output reg (RGB =). The 8 bit instructions sets the MSB (LSB=0)
    VM_OP_MODOUT = 3,  // 8/16        mo oreg, val     Modify an output (RGB +=)
    VM_OP_COMMIT = 4,  // 0           cmt              Send the output regs to the PWM outs and waits the next timer interrupt
    VM_OP_WAIT   = 5,  // 0           wt               Stops the VM until the next timer interrupt (1ms)
    VM_OP_DRJNZ  = 6,  // 8/16        drjnz reg, addr  Decrement Reg, then Jump if Not Zero
    VM_OP_JMP    = 7,  // 8           j addr           Jump to relative address
    VM_OP_JMPABS = 8,  // 8/16        j uaddr          Jump to absolute address
    VM_OP_IVEC   = 9,  // 0           ivector v:       Entry point for the 'v' vector (the vector number 'v' [0, 3] is encoded in the .ro field of the instruction)
    VM_OP_ISETPC = 10, // 8/16        isetpc addr      Set the PC which is restored on iret
    VM_OP_IRET   = 11, // 0           iret             Exit from the interrupt
} vm_opcode_e;


typedef union {
    unsigned char  b[8];
    unsigned short w[4];
} vm_regs_t;

typedef struct {
    // Pause state. When the console code wants to stop the execution
    // it writes true to .is_paused, then waits for the VM to acknowledge
    // that by busy waiting for .is_paused_ack to become true
    bool is_paused, is_paused_ack;

    // Interrupt states
    #define VM_INT_NONE    0
    #define VM_INT_PENDING 1
    #define VM_INT_ONGOING 2

    // req state (VM_INT_*), vector and argument
    byte int_req, int_vector;
    unsigned short int_arg;

    // program memory (raw lspb bytecode)
    byte* imem;

    // VCPU regs
    vm_regs_t      regs;  // A, B, C and D
    vm_regs_t      outs;  // R, G, B and Not Used
    unsigned short pc;

    // Saved state (set before jumping to an interrupt and restored before jumping back)
    vm_regs_t      saved_regs;
    vm_regs_t      saved_outs;
    unsigned short saved_pc;
} vm_state_t;


void vm_reset(volatile vm_state_t& vm, byte* imem);

void vm_set_pause(volatile vm_state_t& vm, bool pause);

void vm_request_interrupt(volatile vm_state_t& vm, byte ivect, unsigned short arg);

void vm_step(volatile vm_state_t& vm, bool debug);


/*
*/

/*
Instruction encoding test
    0B00000001, 127,          srb %A, 127
    0B10010001, 0x34, 0x12,   srw %B, 0x1234
    0B00100010, 255,          sob @B, 255
    0B10010010, 0x78, 0x56,   sow @G, 0x5678
    0B00100011, 0x02,         mob @B, 2
    0B00100011, 0xfe,         mob @B, -2
    0B10010011, 0x04, 0x00,   mow @G, 4
    0B10010011, 0xfc, 0xff,   mow @G, -4
    0B00000100,               cmt
    0B00000101,               wt
    0B00110110, 0xf8,         drjnzb %D, -8
    0B00110110, 0x08,         drjnzb %D, 8
    0B10110110, 0x03, 0x00,   drjnzw %D, 3
    0B10110110, 0x00, 0xff,   drjnzw %D, -256
    0B00000111, 0x10,         jb 16
    0B00000111, 0xf0,         jb -16
    0B10000111, 0x10, 0x00,   jw 16
    0B10000111, 0xf0, 0xff,   jw -16
    0B00001000, 0x12,         jab 18
    0B10001000, 0x00, 0x01,   jaw 256
    0B00000000,               hlt

Simple test program: R 0 -> 255 -> 0 looping
    sob @R, 0       0B00000010, 0,
    cmt             0B00000100,
    srw %A, 5000    0B10000001, 136, 19,
    mob @R, 13      0B00000011, 13,
    cmt             0B00000100,
    drjnzb %A, -5   0B00000110, 0xfb,
    srw %A, 5000    0B10000001, 136, 19,
    mob @R, -13     0B00000011, 0xf3,
    cmt             0B00000100,
    drjnzb %A, -5   0B00000110, 0xfb,
    jab 0           0B00001000, 3

    "serial format": w2w0 w4 w129w136w19 w3w13 w4 w6w251 w129w136w19 w3w243 w4 w6w251 w8w3

RGB Rotation (255,0,0 -> 0,255,0 -> 0,0,255 looping)
    sob @R, 0          0B00000010, 0,
    sob @G, 0          0B00010010, 0,
    sob @B, 255        0B00100010, 255,
    cmt                0B00000100,

    srw %A, 5000       0B10000001, 136, 19,
    mob @R, 13         0B00000011, 13,
    mob @B, -13        0B00100011, 243,
    cmt                0B00000100,
    drjnzb %A, -7      0B00000110, 249,

    sob @R, 255        0B00000010, 255,
    sob @B, 0          0B00100010, 0,
    cmt                0B00000100,

    srw %A, 5000       0B10000001, 136, 19,
    mob @R, -13        0B00000011, 243,
    mob @G, 13         0B00010011, 13,
    cmt                0B00000100,
    drjnzb %A, -7      0B00000110, 249,

    sob @R, 0          0B00000010, 0,
    sob @G, 255        0B00010010, 255,
    cmt                0B00000100,

    srw %A, 5000       0B10000001, 136, 19,
    mob @G, -13        0B00010011, 243,
    mob @B, 13         0B00100011, 13,
    cmt                0B00000100,
    drjnzb %A, -7      0B00000110, 249,

    sob @G, 0          0B00010010, 0,
    sob @B, 255        0B00100010, 255,

    jab 2              0B00001000, 2

    w2w0 w18w0 w34w255 w4
    w129w136w19 w3w13 w35w243 w4 w6w249
    w2w255 w34w0 w4
    w129w136w19 w3w243 w19w13 w4 w6w249
    w2w0 w18w255 w4
    w129w136w19 w19w243 w35w13 w4 w6w249
    w18w0 w34w255
    w8w2

Interrupt test program 0 (vector 0: blink 2sec)
    base program (R 0->255->0 looping)
      w2w0 w4 w129w136w19 w3w13 w4 w6w251 w129w136w19 w3w243 w4 w6w251 w8w3

    ivec 0             0B00001001,
    srb %A, 4          0B00000001, 4,
    
    sob @R, 0          0B00000010, 0,
    cmt                0B00000100,
    srw %B, 500        0B10010001, 244, 1,
    wt                 0B00000101,
    drjnzb %B, -3      0B00010110, 253,
    
    sob @R, 255        0B00000010, 255,
    cmt                0B00000100,
    srw %B, 500        0B10010001, 244, 1,
    wt                 0B00000101,
    drjnzb %B, -3      0B00010110, 253,

    drjnzb %A, -20     0B00000110, 236,
    iret               0B00001011

    w9 w1w4 w2w0 w4 w145w244w1 w5 w22w253 w2w255 w4 w145w244w1 w5 w22w253 w6w236 w11

Interrupt test program 1 (vector 0: change loop, arg = 1 for first loop, arg = 2 for second loop)
  base 0 (R 0->255->0 looping) at addr 0x0000
    w2w0 w4 w129w136w19 w3w13 w4 w6w251 w129w136w19 w3w243 w4 w6w251 w7w235

  base 1 (R 0->255->0 looping, but 10 times faster) at addr 0x0015
    w2w0 w4 w129w244w1 w131w130w0 w4 w6w250 w129w244w1 w131w126w255 w4 w6w250 w7w233

  interrupt (%A contains the interrupt's argument)
    ivec 0             0B00001001,

    drjnzb  %A, 3      0B00000110, 3,
    isetpcb 0          0B00001010, 0,   # 0x0000
    iret               0B00001011,

    drjnzb  %A, 2      0B00000110, 2,
    isetpcb 21         0B00001010, 21,  # 0x0015
    iret               0B00001011

    w9 w6w3 w10w0 w11 w6w2 w10w21 w11

  Complete bytecode
    w2w0 w4 w129w136w19 w3w13 w4 w6w251 w129w136w19 w3w243 w4 w6w251 w7w235
    w2w0 w4 w129w244w1 w131w130w0 w4 w6w250 w129w244w1 w131w126w255 w4 w6w250 w7w233
    w9 w6w3 w10w0 w11 w6w2 w10w21 w11
*/

#endif
