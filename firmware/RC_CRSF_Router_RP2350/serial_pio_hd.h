// serial_pio_hd.h - CRSF-Router 8: Halbduplex-UART ueber PIO (ein Pin)
//
// arduino-pico (6.3.2) kennt keinen Halbduplex-UART. Diese Klasse nimmt den
// normalen SerialPIO mit TX und RX auf demselben Pin und schaltet den
// Treiber des Pins selbst um (GPIO-Override, die PIO-Programme bleiben
// unveraendert):
//   - Ruhe: Pin ist Eingang mit Pull-up, das Geraet darf senden.
//   - Senden: erst wenn die Leitung frei ist (keine Bytes in den letzten
//     HD_GAP_BYTES Bytezeiten und HD_TURNAROUND_US nach dem eigenen Frame,
//     damit das Geraet antworten kann). Dann treibt der Pin einen ganzen
//     Frame lang und wird danach wieder losgelassen.
//   - Das eigene Echo (RX liest mit) wird beim Loslassen verworfen.
// Frame-Grenzen erkennt die Klasse selbst am Laengenbyte des CRSF-Frames.
//
// Anschluss: Signal des Geraets an den TX-Pin des Ports, RX-Pin bleibt frei.
// Nur fuer PIO-Ports (Port 1, 3-7), nicht fuer Port 2 (Hardware-UART).
#ifndef SERIAL_PIO_HD_H
#define SERIAL_PIO_HD_H

#include <Arduino.h>
#include <SerialPIO.h>

#define HD_GAP_BYTES     3     // Leitung frei nach so vielen ruhigen Bytezeiten
#define HD_TURNAROUND_US 600   // nach eigenem Frame: Zeit fuer die Antwort

class SerialPIOHD : public SerialPIO {
public:
    SerialPIOHD(pin_size_t pin, size_t fifoSize) : SerialPIO(pin, pin, fifoSize), _pin(pin) {}

    void begin(unsigned long baud) override { begin(baud, SERIAL_8N1); }
    void begin(unsigned long baud, uint16_t config) override;

    int    available() override;
    int    read() override;
    int    peek() override;
    int    availableForWrite() override;
    size_t write(uint8_t c) override;
    using Print::write;

private:
    void service();
    void release();

    pin_size_t _pin;
    bool     _driving   = false;   // Pin treibt gerade (eigener Frame)
    uint8_t  _txPos     = 0;       // Bytes des aktuellen Frames schon geschrieben
    uint8_t  _txLen     = 0;       // Laenge des aktuellen Frames (0 = noch offen)
    bool     _emptySeen = false;
    uint32_t _emptyUs   = 0;
    uint32_t _lastRxUs  = 0;
    uint32_t _releaseUs = 0;
    uint32_t _byteUs    = 25;
};

#endif
