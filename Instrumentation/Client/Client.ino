#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <ADC.h>
#include "Wire.h"

#include "PCF8575.h" // relay board 
#include "MAX6675.h" // thermocouples
#include <SparkFun_I2C_Mux_Arduino_Library.h>
#include <SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h>

#define mux_wire Wire
#define relay_wire Wire2

byte mac[] = { 0x04, 0xE9, 0xE5, 0x15, 0xAB, 0x0E };  // MAC of the ethernet adapter
IPAddress ip(192, 168, 5, 5); // IP of the teensy
IPAddress remoteIP(192, 168, 5, 2); // IP of the attached PC
unsigned int remotePort = 65432; // Random port

PCF8575 relay_module(0x27, &Wire2); // i2c addr of the relay board

QWIICMUX mux;

NAU7802 loadcell_1;
NAU7802 loadcell_2;
NAU7802 loadcell_3;
bool loadcell_active = false;

const int nt_dataPin   = 12;
const int nt_clockPin  = 13;
const int nt_selectPin = 10;
MAX6675 NitrousTankThermocouple(nt_selectPin, nt_dataPin, nt_clockPin);

const int et_dataPin   = 39;
const int et_clockPin  = 27;
const int et_selectPin = 37;
MAX6675 EngineThermocouple(et_selectPin, et_dataPin, et_clockPin);

EthernetUDP Udp; 

struct send_dataframe {
  uint16_t analogValues[18]; // Teensy 4.1 has 18 analog pins
  float load_cells[3];
  float thermocouples[2];
  uint32_t timestamp;
};

send_dataframe old_df;

struct receive_dataframe {
  uint8_t relay_states[16]; // 16 relay channels
};

ADC *adc = new ADC();

unsigned long last_lc_read_time = 0;
const unsigned long read_lc_interval = 50; // milliseconds

unsigned long last_tc_read_time = 0;
const unsigned long read_tc_interval = 250; // milliseconds

void setup() {
  Serial.begin(3000000);
  delay(1000);
  Serial.println("Init");
  Ethernet.begin(mac, ip);
  Serial.println("Link is up");
  Udp.begin(remotePort);

  Serial.println("Initializing relay board");
  init_relay_board();

  init_mux();
  init_load_cells();
  init_thermocouples();

  Serial.println("Configuring ADCs");
  adc->adc0->setAveraging(2);
  adc->adc0->setResolution(10);
  adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED);
  adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::HIGH_SPEED);

  adc->adc1->setAveraging(2);
  adc->adc1->setResolution(10);
  adc->adc1->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED);
  adc->adc1->setSamplingSpeed(ADC_SAMPLING_SPEED::HIGH_SPEED);

  Serial.println("Initialized");
  delay(3000);
}

void readAnalogValues(send_dataframe* df) {
  ADC::Sync_result result;
    // Read A0 to A9 in pairs, using both ADCs in sync is faster
    for (int i = 0; i < 10; i += 2) {
        result = adc->analogSyncRead(i, i + 1);
        df->analogValues[i] = result.result_adc0;
        df->analogValues[i + 1] = result.result_adc1;
    }

    // Below section is confusing but it relates to the fact that certain pins are only connected to one ADC instead of both.
    // See: https://github.com/PaulStoffregen/Audio/blob/master/input_adc.cpp#L256
    // Some that are connected to both (ie 16 and 17) are used here as partners for ones that are in synchronous reads.
    result = adc->analogSyncRead(10, 12);
    df->analogValues[10] = result.result_adc0;
    df->analogValues[12] = result.result_adc1;

    result = adc->analogSyncRead(11, 13);
    df->analogValues[11] = result.result_adc0;
    df->analogValues[13] = result.result_adc1;

    result = adc->analogSyncRead(16, 14);
    df->analogValues[16] = result.result_adc0;
    df->analogValues[14] = result.result_adc1;

    result = adc->analogSyncRead(17, 15);
    df->analogValues[17] = result.result_adc0;
    df->analogValues[15] = result.result_adc1;
}

void init_load_cells()
{

  mux.setPort(0);
  if(!loadcell_1.begin(mux_wire))
  {
    Serial.println("Load cell 1 failed to activate");
  }
  else
  {
    //loadcell_1.calculateZeroOffset();
    //Serial.println("Zero'd, place weight");
    //delay(15000);
    //Serial.println("Calibrating");
    //loadcell_1.calculateCalibrationFactor(-4);

    //Serial.println(loadcell_1.getCalibrationFactor());
    //Serial.println(loadcell_1.getZeroOffset());

    //delay(15000);
    loadcell_1.setSampleRate(NAU7802_SPS_320);
    loadcell_1.setCalibrationFactor(13031);
    loadcell_1.setZeroOffset(-79232);
  }

  mux.setPort(1);
  if(!loadcell_2.begin(mux_wire))
  {
    Serial.println("Load cell 2 failed to activate");
  }
  else
  {
    //loadcell_2.calculateZeroOffset();
    //Serial.println("Zero'd, place weight");
    //Serial.println(loadcell_2.getZeroOffset());
    //delay(15000);
    //Serial.println("Calibrating");
    //loadcell_2.calculateCalibrationFactor(-4);

    //Serial.println(loadcell_2.getCalibrationFactor());

    //delay(15000);
    loadcell_2.setSampleRate(NAU7802_SPS_320);
    loadcell_2.setCalibrationFactor(13031);
    loadcell_2.setZeroOffset(-79232);
  }

  mux.setPort(2);
  if(!loadcell_3.begin(mux_wire))
  {
    Serial.println("Load cell 3 failed to activate");
  }
  else
  {
    loadcell_3.setSampleRate(NAU7802_SPS_320);
    loadcell_3.setCalibrationFactor(13031);
    loadcell_3.setZeroOffset(-79232);
  }

  delay(100);

  if(!loadcell_1.isConnected() || !loadcell_2.isConnected() || !loadcell_3.isConnected())
  {
    Serial.println("One or more load cells not connected, disabling...");
    loadcell_active = false;
  }
  else {
    Serial.println("All load cells activated");
    loadcell_active = true;
  }

}

float getFastWeight(NAU7802 loadcell) // the built-in getWeight() is super slow so just do it like this
{
  return ((float)(loadcell.getReading() - loadcell.getZeroOffset())) / loadcell.getCalibrationFactor();
}

void read_load_cells(float* ld1, float* ld2, float* ld3) {


  if(!loadcell_active)
  {
    *ld1 = -1;
    *ld2 = -1;
    *ld3 = -1;
    return;
  }

  mux.setPort(0);
  if (loadcell_1.available()) {
    //*ld1 = loadcell_1.getWeight(true);
    *ld1 = getFastWeight(loadcell_1);
  }

  mux.setPort(1);
  if (loadcell_2.available()) {
    //*ld2 = loadcell_2.getWeight(true);
    *ld2 = getFastWeight(loadcell_2);
  }

  mux.setPort(2);
  if (loadcell_3.available()) {
    //*ld3 = loadcell_3.getWeight(true);
    *ld3 = getFastWeight(loadcell_3);
  }
  Serial.printf("Loadcell 1: %.2f, Loadcell 2: %.2f, Loadcell 3: %.2f\n", *ld1, *ld2, *ld3);
  Serial.println();

}

void init_mux() // the i2c qwiic multiplexer for the load cells
{
  mux_wire.begin();
  Serial.println("Init mux");

  if(!mux.begin())
  {
    Serial.println("Failed to initialize mux");
  }
}

void init_relay_board() // https://www.tindie.com/products/bugrovs2012/16-channel-i2c-electromagnetic-relay-module-iot/
{
  relay_wire.begin();
  if(!relay_module.begin())
  {
    Serial.println("Relay module could not initialized");
  }

  if(!relay_module.isConnected())
  {
    Serial.println("Relay module is not connected");
  }
  unsigned char i;

  for(i=0;i<16;i++)
  {
    relay_module.write(i, LOW); // LOW level relay is ON / HIGH level relay is OFF
  }

  relay_module.begin();
}

void init_thermocouples()
{
  delay(100);

  SPI.begin();

  SPI1.begin();

  NitrousTankThermocouple.begin();
  NitrousTankThermocouple.setSPIspeed(4000000);

  EngineThermocouple.begin();
  EngineThermocouple.setSPIspeed(4000000);
  Serial.println("Thermocouples initialized");
}

void read_thermocouples(float* tc1, float* tc2)
{
  int status = NitrousTankThermocouple.read();
  float temp = NitrousTankThermocouple.getTemperature();

  int status2 = EngineThermocouple.read();
  float temp2 = EngineThermocouple.getTemperature();

  *tc1 = temp;
  *tc2 = temp2;

  //Serial.println("temps");
  //Serial.println(temp);
  //Serial.println(temp2);
}

void receive_command_packet() {
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    //Serial.print("Packet Size: ");
    //Serial.println(packetSize);

    if (packetSize != sizeof(receive_dataframe)) {
      Serial.println("Error: Packet size mismatch");
      return;
    }

    receive_dataframe r_df;
    Udp.read((char*)&r_df, sizeof(r_df));

    //Serial.print("Raw Data: ");
    //for (int i = 0; i < sizeof(r_df.relay_states); i++) {
    //  Serial.print(r_df.relay_states[i], HEX);
    //  Serial.print(" ");
    //}
   // Serial.println();

    //Serial.print("Relay States: ");
    //for (int i = 0; i < 16; i++) {
    //  Serial.print(r_df.relay_states[i] ? "ON" : "OFF");
    //  if (i < 15) {
    //    Serial.print(", ");
    //  }
    //}
    //Serial.println();

    for (int i = 0; i < 16; i++) {
      relay_module.write(i, r_df.relay_states[i] ? LOW : HIGH); // LOW level relay is ON / HIGH level relay is OFF
    }

    //int x = relay_module.read16();
    //Serial.print("Read ");
    //Serial.println(x, HEX);
  }
}

void send_reply_packet()
{
  send_dataframe df;
  readAnalogValues(&df);

  df.timestamp = micros();

  // Read load cell values and store them in the dataframe
  unsigned long current_time = millis();
  if (current_time - last_lc_read_time >= read_lc_interval) {
    last_lc_read_time = current_time;
    read_load_cells(&df.load_cells[0], &df.load_cells[1], &df.load_cells[2]);
  }
  else
  {
    df.load_cells[0] = old_df.load_cells[0];
    df.load_cells[1] = old_df.load_cells[1];
    df.load_cells[2] = old_df.load_cells[2];
  }

  if (current_time - last_tc_read_time >= read_tc_interval) {
    last_tc_read_time = current_time;
    read_thermocouples(&df.thermocouples[0], &df.thermocouples[1]);
  }
  else
  {
    df.thermocouples[0] = old_df.thermocouples[0];
    df.thermocouples[1] = old_df.thermocouples[1];
  }

  if (sizeof(df) > 1400)
  {
    Serial.println("Dataframe may be too large to send!");
  }

  Udp.beginPacket(remoteIP, remotePort);
  Udp.write((byte*)&df, sizeof(df));
  Udp.endPacket();

  old_df = df;
}

void loop() {
  unsigned long start_time, end_time, duration;

  // Measure time for send_reply_packet()
  start_time = micros();
  send_reply_packet();
  end_time = micros();
  duration = end_time - start_time;
  //Serial.print("send_reply_packet() took ");
  //Serial.print(duration);
  //Serial.println(" microseconds");

  // Measure time for receive_command_packet()
  start_time = micros();
  receive_command_packet();
  end_time = micros();
  duration = end_time - start_time;
  //Serial.print("receive_command_packet() took ");
  //Serial.print(duration);
  //Serial.println(" microseconds");

}