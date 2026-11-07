// serial_pio_hd.cpp - CRSF-Router 8, siehe serial_pio_hd.h
#include "serial_pio_hd.h"
#include <hardware/gpio.h>
#include <hardware/pio.h>

void SerialPIOHD::begin(unsigned long baud, uint16_t config) {
    SerialPIO::begin(baud, config);
    if (!*this) return;
    // SerialPIO hat den Pin zuletzt dem RX-PIO gegeben. Der Ausgang muss
    // aber vom TX-PIO kommen; lesen kann der RX-PIO jeden Pin.
    pio_gpio_init(_txPIO, _pin);
    gpio_pull_up(_pin);
    gpio_set_oeover(_pin, GPIO_OVERRIDE_LOW);   // loslassen
    _driving = false;
    _txPos = _txLen = 0;
    _byteUs = (uint32_t)(10000000UL / baud) + 1;
    _lastRxUs = _releaseUs = micros();
}

void SerialPIOHD::release() {
    gpio_set_oeover(_pin, GPIO_OVERRIDE_LOW);
    _driving = false;
    // eigenes Echo verwerfen
    while (SerialPIO::available() > 0) SerialPIO::read();
    _releaseUs = micros();
}

// Frame komplett im FIFO und rausgeschoben -> Pin loslassen
void SerialPIOHD::service() {
    if (!_driving || _txLen == 0 || _txPos < _txLen) return;
    if (!pio_sm_is_tx_fifo_empty(_txPIO, _txSM)) { _emptySeen = false; return; }
    uint32_t now = micros();
    if (!_emptySeen) { _emptySeen = true; _emptyUs = now; return; }
    // das letzte Byte kann noch im Schieberegister sein
    if (now - _emptyUs >= 2 * _byteUs) {
        _txPos = _txLen = 0;
        _emptySeen = false;
        release();
    }
}

int SerialPIOHD::available() {
    service();
    if (_driving) return 0;
    int n = SerialPIO::available();
    if (n > 0) _lastRxUs = micros();
    return n;
}

int SerialPIOHD::read() {
    service();
    if (_driving) return -1;
    int c = SerialPIO::read();
    if (c >= 0) _lastRxUs = micros();
    return c;
}

int SerialPIOHD::peek() {
    service();
    return _driving ? -1 : SerialPIO::peek();
}

int SerialPIOHD::availableForWrite() {
    service();
    if (_driving) {
        if (_txLen && _txPos >= _txLen) return 0;   // Frame fertig, warten aufs Loslassen
        return SerialPIO::availableForWrite();
    }
    // neuer Frame nur bei freier Leitung
    if (SerialPIO::available() > 0) { _lastRxUs = micros(); return 0; }
    uint32_t now = micros();
    if (now - _lastRxUs < HD_GAP_BYTES * _byteUs) return 0;
    if (now - _releaseUs < HD_TURNAROUND_US) return 0;
    return SerialPIO::availableForWrite();
}

size_t SerialPIOHD::write(uint8_t c) {
    if (!*this) return 0;
    if (!_driving) {
        gpio_set_oeover(_pin, GPIO_OVERRIDE_HIGH);   // Pin treiben (Ruhepegel high)
        _driving = true;
        _emptySeen = false;
    }
    if (_txLen && _txPos >= _txLen) _txPos = _txLen = 0;   // naechster Frame direkt dahinter
    if (_txPos == 1) _txLen = (uint8_t)(c + 2);            // Laengenbyte: Rest + Typ/CRC
    _txPos++;
    return SerialPIO::write(c);
}
