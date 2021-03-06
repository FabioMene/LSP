# LSP
Five packages to drive and animate a rgb led strip, controllable via an android app over the local network. 

## Premise
The aim of the project is to make simple rgb led strip animations controlled by an atmega328p mcu, connected to
my home server, which hosts an HTTP based API to send instructions to the board. The server's motherboard doesn't have
an onboard serial port, but because the communication is simplex I didn't bother buying an FTDI chip or the like. Instead
I've made a simple hardware filter which translates binary ASK audio data from the line-out connector to an mcu pin. The microcontroller
firmware doesn't control the led animations directly; instead it executes compiled "LSP bytecode", generated by a custom assembly language

### lsp-avr/
This is the arduino ide sketch that runs on the atmega. The virtual machine bytecode documentation is in 'vm.h'

### lspvm-asm/
These are a compiler (lspc), a decompiler (lspd) and a bytecode emulator (lspemu). Example programs are available in /progs/

### lsp-ctrl-srv/
This is a python 3 HTTP server with an integrated pulseaudio interface library, which hosts the API. The pulseaudio library is used
to send the serial data in binary ASK, where silence is logic high and an high frequency wave is logic zero; this is the link-layer proto, the data-layer proto
is UART, 8-bit bytes, no parity, 2 stop bits (or 8n2 if you prefer). The upper layer proto is a simple ascii protocol: one-letter commands followed by a
fixed number of arguments, if any, separated by whitespace

### misc/
There are some photos, a schematic, two pcb renders and gerber files of the atmega board

### LSPController/
The android application. This is very simple and very hardcoded, use at own risk
