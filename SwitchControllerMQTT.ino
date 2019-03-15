/*------------------------- Pre-Configuration ----------------------------*/
#define W5500                       5500
#define W5100                       5100
#define ETHER_TYPE                  W5500
#define ENABLE_DHCP                 false
#define ENABLE_EXTERNAL_WATCHDOG    true
#define _DEBUG_LEVEL                1

#define BROKER_IP                   10,90,72,88

#define HOST_IP                     10,90,72,46
#define HOST_NETMASK                255,0,0,0
#define HOST_DNS                    172,19,10,100
#define HOST_GATEWAY                10,1,1,254

#define MAC_REFRESH                 false
#define MAC_0                       0xDA
#define MAC_1                       0x00
#define MAC_2                       0x00
#define MAC_3                       0x00
#define MAC_4                       0x00
#define MAC_5                       0x00

/*------------------------------- Start ----------------------------------*/
#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#if (ETHER_TYPE == W5500)
#include <Ethernet2.h>
#else
#include <Ethernet.h>
#endif

/*----------------------------- Watch dog --------------------------------*/
#define WATCHDOG_PIN                    7
#define WATCHDOG_PULSE_LENGTH           50        // Milliseconds
#define WATCHDOG_RESET_INTERVAL         12000      // Milliseconds. Also the period for sensor reports.
long watchdogLastResetTime = 0;

/*-------------------------- Get MAC address -----------------------------*/
/* CHANGE THIS TO YOUR OWN UNIQUE VALUE.  The MAC number should be
 * different from any other devices on your network or you'll have
 * problems receiving packets. Can be replaced automatically below
 * using a MAC address ROM. */
#define MAC_PREFIX   '#'
#define MAC_START    (1)
#define MAC(idx)     (MAC_START + idx)
#define MAC_LEN      (6)
static uint8_t mac[MAC_LEN] = { 0xDA, 0x00, 0x00, 0x00, 0x00, 0x00 };  // Set if no MAC ROM
char get_MAC(uint8_t *mac_buf, bool mac_refresh);

/*----------------------- Define IP information --------------------------*/
#define DEF_IP           192,168,1,196
#define DEF_NETMASK      255,255,255,0
#define DEF_DNS          8,8,8,8
#define DEF_GATEWAY      192,168,1,1

IPAddress broker(BROKER_IP);

#if (defined HOST_IP) && defined (HOST_NETMASK)
IPAddress ip(HOST_IP);
IPAddress submask(HOST_NETMASK);
#else
IPAddress ip(DEF_IP);
IPAddress submask(DEF_NETMASK);
#endif

#if (defined HOST_DNS) && defined (HOST_GATEWAY)
IPAddress _dns(HOST_DNS);
IPAddress gateway(HOST_GATEWAY);
#else
IPAddress _dns(DEF_DNS);
IPAddress gateway(DEF_GATEWAY);
#endif

/*------------------- Define Switch controller information --------------------*/
/* Button setup */
/* Button Type: Momentary = 0 GQ16
 *              Latching = 1
 */
#define BUTTON_MAX                  48
#define BUTTON_TYPE_MOMENTARY       0
#define BUTTON_TYPE_LATCHING        1

static byte buttontype[BUTTON_MAX] =
                        { 0,  0,  0,  0,    0,  0,  0,  0,
                          0,  0,  0,  0,    0,  0,  0,  0,
                          0,  0,  0,  0,    0,  0,  0,  0,
                          0,  0,  0,  0,    0,  0,  0,  0,
                          0,  0,  0,  0,    0,  0,  0,  0,
                          1,  1,  1,  1,    0,  0,  0,  0 };
static byte lastButtonState[BUTTON_MAX] =
                        { 1,  1,  1,  1,    1,  1,  1,  1,
                          1,  1,  1,  1,    1,  1,  1,  1,
                          1,  1,  1,  1,    1,  1,  1,  1,
                          1,  1,  1,  1,    1,  1,  1,  1,
                          1,  1,  1,  1,    1,  1,  1,  1,
                          1,  1,  1,  1,    1,  1,  1,  1 };
static byte buttonArray[BUTTON_MAX] =
                        {54, 55, 56, 57,   58, 59, 60, 61,      // A0-A7
                         62, 63, 64, 65,   66, 67, 68, 69,      // A8-A15
                         40, 41, 42, 43,   44, 45, 46, 47,      // D40-D47
                         32, 33, 34, 35,   36, 37, 38, 39,      // D16-D23
                         24, 25, 26, 27,   28, 29, 30, 31,      // D24-D31
                         16, 17, 18, 19,   20, 21, 22, 23 };    // D32-D39
static long lastActivityTime_array[BUTTON_MAX] = {0};

byte lastButtonPressed         = 0;
#define DEBOUNCE_DELAY         100
#define BUTTON_PRESSED         0
#define BUTTON_NOT_PRESSED     1


/* MQTT define */
const char * const eventsTopic PROGMEM = "events";    // MQTT topic to publish status reports
const char * const client_str PROGMEM = "switchboard-%02x%02x%02x%02x%02x%02x";
const char * const cmd_topic PROGMEM = "buttons";
const char * const button_msg_str PROGMEM = "%02x%02x%02x-%d";
const char * const mqtt_msg_str PROGMEM = "%s is connected.";
char messageBuffer[100];
char topicBuffer[100];

long lastActivityTime = 0;

/**
   MQTT callback
*/
void callback(char* topic, byte* payload, unsigned int length);

/*------------------------------- Setup ---------------------------------*/
EthernetClient ethclient;
PubSubClient client(ethclient);

/**
 * Initial configuration
 */
void setup()
{
  delay(250);
  Wire.begin();        // Wake up I2C bus
  Serial.begin(9600);  // Use the serial port to report back readings

  /* Set up the watchdog timer */ 
  if(ENABLE_EXTERNAL_WATCHDOG == true)
  {
    pinMode(WATCHDOG_PIN, OUTPUT);
    digitalWrite(WATCHDOG_PIN, LOW);
  }

  while (!Serial && millis() < 2000) {
    ;
  }

  Serial.print(F("Getting MAC address from ROM: "));
  get_MAC(mac, MAC_REFRESH);
  char tmpBuf[17];
  sprintf(tmpBuf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println(tmpBuf);

  // Set up the Ethernet library to talk to the Wiznet board
  if ( ENABLE_DHCP == true )
  {
    Ethernet.begin(mac);      // Use DHCP
  } else {
    Ethernet.begin(mac, ip);  // Use static address defined above
    Ethernet.begin(mac, ip, _dns, gateway, submask);  // Use static address defined above
  }

  // Print IP address:
  Serial.print(F("Local IP: "));
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    if ( thisByte < 3 )
    {
      Serial.print(F("."));
    }
  }
  Serial.println();
  Serial.print(F("MQTT broker IP: "));
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(broker[thisByte], DEC);
    if ( thisByte < 3 )
    {
      Serial.print(F("."));
    }
  }
  Serial.println();

  /* Set up the switch controller */
  Serial.println(F("Setting input pull-ups"));
  Serial.print(F("\n"));
  for( byte i = 0; i < BUTTON_MAX; i++)
  {
    pinMode(buttonArray[i], INPUT_PULLUP);
    if( buttontype[i] == BUTTON_TYPE_LATCHING)
      lastButtonState[i] = digitalRead( buttonArray[i] );
    Serial.print(buttonArray[i]);
    Serial.print(" ");
    if( (i+1) % 8 == 0)
      Serial.print(F("\n"));
  }
  Serial.println();

  /* Connect to MQTT broker */
  Serial.println(F("connecting..."));
  client.setServer(broker, 1883);
  client.setCallback(callback);
  Serial.println(F("Ready."));
}


/**
 * Main program loop
 */
void loop()
{
  if (!client.connected()) {
    reconnect();
  }

  runHeartbeat();

  client.loop();

  for(byte i = 0; i < BUTTON_MAX; i++) {
    processButtonDigital( i );
  }
}

/**
 */
void processButtonDigital( byte buttonId )
{
    int sensorReading = digitalRead( buttonArray[buttonId] );

#if (_DEBUG_LEVEL > 5)
    Serial.print(buttonId, DEC);
    Serial.print(": ");
    Serial.println(sensorReading, DEC);
#endif

    if( buttontype[buttonId] == BUTTON_TYPE_LATCHING )
    {
        if( sensorReading != lastButtonState[buttonId] )  // Input is not eaual last state. Button pressed.
        {
            if( (millis() - lastActivityTime) > DEBOUNCE_DELAY )  // Proceed if we haven't seen a recent event on this button
            {
                lastActivityTime = millis();
                button_pressed(buttonId);
                lastButtonState[buttonId] = sensorReading;
            }
        }
    }
    else  // GQ16
    {
        if (sensorReading == BUTTON_PRESSED){
            if( (millis() - lastActivityTime_array[buttonId] > DEBOUNCE_DELAY) && lastButtonState[buttonId] == BUTTON_NOT_PRESSED)
            {
                lastButtonState[buttonId] = BUTTON_PRESSED;
                lastActivityTime_array[buttonId] = millis();
                button_pressed(buttonId);
            }
        }
        else {
          lastButtonState[buttonId] = BUTTON_NOT_PRESSED;
        }
    }
}

void button_pressed(int buttonId) {

  sprintf(messageBuffer, button_msg_str, mac[3], mac[4], mac[5], buttonId);
  sprintf(topicBuffer, cmd_topic, mac[3], mac[4], mac[5]);

  Serial.print( "transition on ");
  Serial.print( buttonId, DEC );
  Serial.print(" (input ");
  Serial.print( buttonArray[buttonId] );
  Serial.println(")");

  client.publish(topicBuffer, messageBuffer);

  Serial.print(F("Button pressed: "));
  Serial.println(buttonId);
}

void reconnect() {
  char mqtt_client_id[30];
  sprintf(mqtt_client_id, client_str, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print(F("Attempting MQTT connection..."));
    // Attempt to connect
    if (client.connect(mqtt_client_id)) {
      Serial.println(F("connected"));
      // Once connected, publish an announcement...
      sprintf(messageBuffer, mqtt_msg_str, mqtt_client_id);
      client.publish(eventsTopic, messageBuffer);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

/*---------------------- Get MAC address function -----------------------*/
char get_MAC(uint8_t *mac_buf, bool mac_refresh) {
  // Random MAC address stored in EEPROM
  if (mac_refresh == false && EEPROM.read(MAC_START) == MAC_PREFIX) {
    for (int ee_idx = MAC(1), buf_idx = 0; ee_idx <= (MAC_START + MAC_LEN); ee_idx++, buf_idx++) {
      mac_buf[buf_idx] = EEPROM.read(ee_idx);
    }
  } else {
    randomSeed(analogRead(0));
    EEPROM.write(MAC(1), mac_buf[0] & 0xFE);
    for (int ee_idx = MAC(2), buf_idx = 1; ee_idx <= (MAC_START + MAC_LEN); ee_idx++, buf_idx++) {
      mac_buf[buf_idx] = random(0, 255);
      EEPROM.write(ee_idx, mac_buf[buf_idx]);
    }
    // Write prefix
    EEPROM.write(MAC_START, MAC_PREFIX);
  }
  return 0;
}

/**
 * The heartbeat takes care of both patting the watchdog and reporting sensor values
 */
void runHeartbeat()
{
  if((millis() - watchdogLastResetTime) > WATCHDOG_RESET_INTERVAL)  // Is it time to run yet?
  {
      patWatchdog();  // Only pat the watchdog if we successfully published to MQTT
    // The interval timer is updated inside patWatchdog()
  }
}

/**
 * Pulse the hardware watchdog timer pin to reset it
 */
void patWatchdog()
{
  if( ENABLE_EXTERNAL_WATCHDOG )
  {
    digitalWrite(WATCHDOG_PIN, HIGH);
    delay(WATCHDOG_PULSE_LENGTH);
    digitalWrite(WATCHDOG_PIN, LOW);
  }
  watchdogLastResetTime = millis();
}
