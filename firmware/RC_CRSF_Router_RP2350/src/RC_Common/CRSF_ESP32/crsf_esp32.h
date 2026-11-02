#ifndef CRSF_H
#define CRSF_H

// CRSF_ESP32 - gemeinsame CRSF-Bibliothek fuer Soundmodul und Multiswitch.
// Teil von RC_Common: jedes Projekt traegt eine identische Kopie des
// Ordners RC_Common; Aenderungen immer in allen Kopien nachziehen und
// CRSF_ESP32_VERSION hochzaehlen. Aenderungshistorie: README.md hier.
#define CRSF_ESP32_VERSION "1.1.0"

#include "crsf_platform.h"
#include "crsf_esp32_protocol.h"

#define CRSF_MAX_CHANNELS 16
#define CRSF_PACKET_SIZE 64

#define BAUD_RATE 420000
#define CHANNEL_MASK 0x07FF

// Router fuer Modulketten (seit 1.1.0, aus RC-Multiswitch v4.01)
#define CRSF_ROUTER_QUEUE 8      // Sendewarteschlange je Port (Frames)
#define CRSF_UART_RX_BUF  1024   // UART-Empfangspuffer (Bytes)
#define CRSF_UART_TX_BUF  512    // UART-Sendepuffer (Bytes)

// Ein Port des Routers: Empfangs-Parser + Sendewarteschlange ganzer Frames.
struct CrsfPort {
    Stream* ser = nullptr;
    uint8_t rx[CRSF_PACKET_SIZE];
    uint8_t rxIdx = 0;
    uint8_t rxLen = 0;
    uint8_t q[CRSF_ROUTER_QUEUE][CRSF_PACKET_SIZE];
    uint8_t qLen[CRSF_ROUTER_QUEUE];
    uint8_t qHead  = 0;
    uint8_t qCount = 0;
};

// CRC8 ueber Frame-Typ + Payload (Polynom 0xD5, Haupt-CRC jedes Frames)
uint8_t crc8(const uint8_t* ptr, uint8_t len);
// CRC8 fuer Command-Frames 0x32 (Polynom 0xBA, steht vor der Haupt-CRC)
uint8_t crc8_ba(const uint8_t* ptr, size_t len, uint8_t init = 0x00);

class CRSF
{
public:

    CRSF();
#if defined(ARDUINO) && defined(ESP32)   // RP2350-Kopie: nur ESP32
    // 4. Parameter "baud" (Default = BAUD_RATE/420000). serialPort ==
    // nullptr faellt auf Serial1 zurueck (der ESP32-C3 hat kein Serial2).
    // Seit 1.1.0 mit groesseren UART-Puffern (vor begin()).
    void init_crsf(HardwareSerial *serialPort, uint8_t rxPin, uint8_t txPin, uint32_t baud = BAUD_RATE);

    // Router fuer die Modulkette (seit 1.1.0): Port A (init_crsf) ist der
    // Eingang vom Empfaenger, B und C fuehren zu weiteren Modulen. nullptr =
    // Port nicht in der Kette. Nach init_crsf() aufrufen.
    void init_router(HardwareSerial* portB, uint8_t bRx, uint8_t bTx,
                     HardwareSerial* portC, uint8_t cRx, uint8_t cTx,
                     uint32_t bBaud = BAUD_RATE, uint32_t cBaud = BAUD_RATE);
#endif
    // Bereits geoeffneten Datenstrom verwenden (Host-Tests, andere UARTs).
    void attach_stream(Stream* stream) { serial_data = stream; portA_.ser = stream; }
    // Router mit bereits geoeffneten Datenstroemen (Host-Tests).
    void attach_router_streams(Stream* portB, Stream* portC);

    // B/C lesen, nach A weiterleiten, Warteschlangen abarbeiten. Jeden Loop
    // nach read_packets() aufrufen; ohne aktive Kette ohne Wirkung.
    void route_packets();
    bool     isRouterActive() const { return routerActive; }
    uint32_t getFwdDown()    const { return fwdDown; }    // A -> B/C
    uint32_t getFwdUp()      const { return fwdUp; }      // B/C -> A
    uint32_t getFwdDropped() const { return fwdDropped; } // Warteschlange voll
    uint32_t getDownCrcErrors() const { return downCrcErrors; }
    // Ein Modul weiter hinten in der Kette meldet unsere Adresse
    bool     isDuplicateAddress() const { return duplicateAddress; }

    void read_packets(uint8_t debug);
    void send_packets(byte* buffer, size_t length, uint8_t debug);

    void send_rc_channels_packed();
    void send_ping();
    // paramVersion: CRSF "Parameter Version Number". Die Projekte uebergeben
    // die Firmware-Version, damit Konfiguratoren (z. B. TBS Agent) eine
    // geaenderte Parameterstruktur erkennen und neu einlesen.
    void send_device_info(const char* name, uint8_t parameter, uint8_t paramVersion = 0);
    void send_param_response_CRSF_UINT8(uint8_t param_id, uint8_t parent, const char* name, uint8_t val, uint8_t min, uint8_t max, const char* unit);
    void send_param_response_CRSF_FLOAT(uint8_t param_id, uint8_t parent,  const char* name, uint32_t val, uint32_t min, uint32_t max, uint32_t default_val, uint8_t decimal_point, uint32_t step_size, const char* unit);
    void send_param_response_CRSF_TEXT_SELECTION(uint8_t param_id, uint8_t parent,  const char* name, const char* options, uint8_t val, uint8_t min, uint8_t max);
    void send_param_response_CRSF_STRING(uint8_t param_id, uint8_t parent,  const char* name, const char* value, uint8_t val, uint8_t max_length);
    void send_param_response_CRSF_FOLDER(uint8_t param_id, uint8_t parent, const char* name, std::initializer_list<uint8_t> children);
    // Kinderliste als Feld (seit 1.1.0, fuer das tabellengesteuerte Lua-Menue)
    void send_param_response_CRSF_FOLDER(uint8_t param_id, uint8_t parent, const char* name, const uint8_t* children, uint8_t count);
    void send_param_response_CRSF_INFO(uint8_t param_id, uint8_t parent,  const char* name, const char* info);

    void send_tele_GPS(int32_t latitude, int32_t longitude, uint16_t groundspeed, uint16_t heading, uint16_t altitude, uint8_t satellites);
    void send_tele_Battery_Sensor(uint16_t voltage, uint16_t current, uint32_t capacity_used, uint8_t remaining);
    void send_tele_Vario(uint16_t v_speed);
    void send_tele_Baro_Altitude(int32_t altitude_dm, int16_t vertical_speed_cm_s);
    void send_tele_Airspeed(uint16_t speed);
    void send_tele_RPM(uint8_t rpm_source_id, std::initializer_list<int32_t> rpm_value);
    void send_tele_Temp(uint8_t temp_source_id, std::initializer_list<int32_t> temperature);
    void send_tele_Voltages(uint8_t voltage_source_id, std::initializer_list<int32_t> voltage_values);
    void send_tele_Attitude(int16_t pitch, int16_t roll, int16_t yaw);

    void send_command_MWSET(uint8_t adress, uint8_t state);
    void send_command_MWSET4(uint8_t adress, uint8_t stateH, uint8_t stateL);
    void send_command_MWSET4M(uint8_t adress_1, uint8_t stateH_1, uint8_t stateL_1, uint8_t adress_2, uint8_t stateH_2, uint8_t stateL_2);
    void send_command_MWPROP(uint8_t adress, uint8_t channel, uint8_t duty );

    void send_param(uint8_t parameter_number, uint8_t data);
    void read_param(uint8_t parameter_number, uint8_t parameter_chunk_number);

    uint16_t get_crfs_channels(uint8_t ch) const { return channels[ch]; }
    uint8_t get_crfs_buffer(uint8_t ch) const { return crfs_buffer[ch]; }
    // Separater Puffer fuer MWSET-Kommandos: wird beim Empfang gesichert, damit die
    // spaetere Auswertung in der .ino nicht durch nachfolgende Frames (PING/PARAM) ueberschrieben wird
    uint8_t get_cmd_buffer(uint8_t ch) const { return cmdBuffer[ch]; }

    // Zuletzt empfangene Link-Statistik (0x14) und DEVICE_INFO (0x29)
    const LinkStatistics& getLinkStatistics() const { return linkStats; }
    const DeviceInfo&     getDeviceInfo()     const { return info; }

    bool getDeviceInfoReplyPending() const { return deviceInfoReplyPending; }
    bool getDeviceEntryReplyPending() const { return deviceEntryReplyPending; }
    bool getDeviceReadReplyPending() const { return deviceReadReplyPending; }
    bool getDeviceWriteReplyPending() const { return deviceWriteReplyPending; }
    bool getDeviceCommandReplyPending() const { return deviceCommandReplyPending; }
    uint8_t getPingSource()      const { return pingSource; }
    uint8_t getParamReadIndex()  const { return paramReadIndex; }
    uint8_t getParamWriteIndex() const { return paramWriteIndex; }
    uint8_t getParamWriteValue() const { return paramWriteValue; }
    uint8_t getParamReadSource() const { return paramReadSource; }
    // Eigene CRSF-Geraeteadresse (0xC0..0xCF, abgeleitet aus der WM-Adresse)
    void          setDeviceAddress(uint8_t a) { deviceAddress = a; }
    uint8_t       getDeviceAddress() const { return deviceAddress; }
    unsigned long getPingTime()      const { return pingReceivedTime; }

    void setDeviceInfoReplyPending(bool newValue);
    void setDeviceEntryReplyPending(bool newValue);
    void setDeviceReadReplyPending(bool newValue);
    void setDeviceWriteReplyPending(bool newValue);
    void setDeviceCommandReplyPending(bool newValue);

    void set_crsf_channel(uint8_t ch, uint16_t value);

    // ── Diagnose-Zaehler (immer aktiv) ──────────────────────────────────
    // Machen "geht nicht" auf der Debug-Seite direkt sichtbar (kommen
    // ueberhaupt Bytes an? gueltige Frames? CRC-Fehler? wird gepingt?).
    uint32_t getRawBytesRx()    const { return rawBytesRx; }
    uint32_t getValidFrames()   const { return validFramesRx; }
    uint32_t getCrcErrors()     const { return crcErrorsRx; }
    uint32_t getDevicePings()   const { return devicePingsRx; }
    uint32_t getParamReads()    const { return paramReadsRx; }
    uint32_t getParamWrites()   const { return paramWritesRx; }
    // Zeitpunkt (millis) des letzten RC-Kanal-Frames, 0 = noch keiner.
    // Basis fuer das LED-Muster "Frames da, aber kein RC-Link".
    unsigned long getLastRcFrameMs() const { return lastRcFrameMs; }

private:
    void updateChannels();
    void updateLink_Statistics();
    void updateDevice_Info();
    void print_channels();
    void crsfDataReceive();

    // Router
    static bool isSyncByte(uint8_t b);
    bool parseByte(CrsfPort& p, uint8_t data);   // true = gueltiger Frame in p.rx
    void routeFromA(const uint8_t* f, uint8_t n);
    void routeFromDown(CrsfPort& p);
    void enqueue(CrsfPort& p, const uint8_t* f, uint8_t n);
    void serviceQueue(CrsfPort& p);
    CrsfPort portA_, portB_, portC_;   // portA_ nur Sendewarteschlange
    bool     routerActive  = false;
    uint32_t fwdDown       = 0;
    uint32_t fwdUp         = 0;
    uint32_t fwdDropped    = 0;
    uint32_t downCrcErrors = 0;
    bool     duplicateAddress = false;
    unsigned long lastRcFrameMs = 0;

    // serial_data/linkStats/info sind Member (nicht global), damit mehrere
    // CRSF-Instanzen (Port A und Port B) sich nicht gegenseitig stoeren.
    Stream*         serial_data = nullptr;
    LinkStatistics  linkStats;
    DeviceInfo      info{};

    bool deviceInfoReplyPending;
    bool deviceEntryReplyPending;
    bool deviceReadReplyPending;
    bool deviceWriteReplyPending;
    bool deviceCommandReplyPending;

    uint8_t crfs_buffer[CRSF_PACKET_SIZE];
    // Parser-Zustand bleibt ueber read_packets()-Aufrufe erhalten: ein Frame,
    // der beim Aufruf erst halb im UART-Puffer liegt, wird beim naechsten
    // Aufruf fertig gelesen statt verworfen.
    uint8_t rxIndex  = 0;
    uint8_t rxLength = 0;
    uint8_t cmdBuffer[CRSF_PACKET_SIZE] = {0}; // gesicherte Kopie des letzten MWSET-Kommandos
    uint8_t pingSource    = 0xEA;  // Source des letzten DEVICE_PING
    uint8_t paramReadIndex  = 0;   // Angefragter Parameter-Index
    uint8_t paramWriteIndex = 0;   // Zu schreibender Parameter-Index
    uint8_t paramWriteValue = 0;   // Zu schreibender Wert
    uint8_t paramReadSource  = 0xEF; // Source des PARAMETER_READ -> Destination der Antwort
    uint8_t deviceAddress = CRSF_ADDRESS_FLIGHT_CONTROLLER; // eigene CRSF-Adresse (0xC0..0xCF aus WM-Adresse; Default 0xC8)
    unsigned long pingReceivedTime = 0; // Zeitpunkt des letzten Broadcast-Ping (fuer Slot-Delay)
    uint16_t channels[CRSF_MAX_CHANNELS] = {0};

    // Diagnose-Zaehler (siehe Getter oben)
    uint32_t rawBytesRx    = 0;
    uint32_t validFramesRx = 0;
    uint32_t crcErrorsRx   = 0;
    uint32_t devicePingsRx = 0;
    uint32_t paramReadsRx  = 0;
    uint32_t paramWritesRx = 0;
};

#endif
