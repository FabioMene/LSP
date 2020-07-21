// Serial mode
// 0: in/out via usb, 115200 baud
// 1: in via audio serial at CFG_AUDIO_BAUDRATE baud, out via usb at 9600 baud
#define CFG_AUDIO_SERIAL   0
#define CFG_AUDIO_BAUDRATE 2756  // See lsp-ctrl-srv/pulse_bridge.py

// Enable debug messages
#define CFG_DO_DEBUG 1

// Enable the help in the console (very memory hard)
#define CFG_ENABLE_HELP 1

// Serial activity LED config
#define CFG_ALED_PHASE_MS   25   // Duration of a single on/off phase
#define CFG_ALED_TIMEOUT_MS 100  // Total duration

// How much time it takes to change the brightness
#define CFG_BRIGHTNESS_ADJ_MS 333


// CONFIG END, down below there is some generated stuff

#if CFG_AUDIO_SERIAL
    #include <SoftwareSerial.h>

    extern SoftwareSerial AudioSerial;

    #define IN_SERIAL  AudioSerial
#else
    #define IN_SERIAL  Serial
#endif

#define OUT_SERIAL Serial

#define PR(args...)  OUT_SERIAL.print(args)
#define PLN(args...) OUT_SERIAL.println(args);
