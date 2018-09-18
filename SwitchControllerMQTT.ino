/*--------------------------- Configuration ------------------------------*/
/* Network Settings */
#define ENABLE_DHCP                 true   // true/false
#define ENABLE_MAC_ADDRESS_ROM      true   // true/false
/* CHANGE THIS TO YOUR OWN UNIQUE VALUE.  The MAC number should be
 * different from any other devices on your network or you'll have
 * problems receiving packets. Can be replaced automatically below
 * using a MAC address ROM. */
#define MAC_PREFIX   '#'
#define MAC_START    (1)
#define MAC(idx)     (MAC_START + idx)
#define MAC_LEN      (6)
static uint8_t mac[MAC_LEN] = { 0xDA, 0x00, 0x00, 0x00, 0x00, 0x00 };  // Set if no MAC ROM
IPAddress ip(192,168,1,35);                // Default if DHCP is not used

/* MQTT Settings */
IPAddress broker(192,168,1,50);        // MQTT broker
const char* eventsTopic  = "events";    // MQTT topic to publish status reports
char messageBuffer[100];
char topicBuffer[100];

long lastActivityTime   = 0;

// Panel-specific configuration:
//int panelId = 13;  // East switchboard (old controller)
//int panelId = 14;  // West switchboard
int panelId = 20;    // East switchboard (new rack mount controller)

/*------------------------------------------------------------------------*/

/**
 * MQTT callback
 */
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

// Instantiate MQTT client
//PubSubClient client(broker, 1883, callback);
EthernetClient ethclient;
PubSubClient client(ethclient);

void reconnect() {
  char mqtt_client_id[30];
  sprintf(mqtt_client_id, "switchboard-%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_client_id)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      sprintf(messageBuffer, "Device %s connnected.", mqtt_client_id);
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

/* ************************************************************************************* */
/* Button setup */
/* Button Type: Momentary = 0 GQ16
 *              Latching = 1
 */
#define BUTTON_TYPE_MOMENTARY       0
#define BUTTON_TYPE_LATCHING        1
static byte buttontype[48] =      {   0,  0,  0,  0,    0,  0,  0,  0,
                                      0,  0,  0,  0,    0,  0,  0,  0,
                                      0,  0,  0,  0,    0,  0,  0,  0,
                                      0,  0,  0,  0,    0,  0,  0,  0,
                                      0,  0,  0,  0,    0,  0,  0,  0,
                                      1,  1,  1,  1,    0,  0,  0,  0 };
static byte lastButtonState[48] = {   0,  0,  0,  0,    0,  0,  0,  0,
                                      0,  0,  0,  0,    0,  0,  0,  0,
                                      0,  0,  0,  0,    0,  0,  0,  0,
                                      0,  0,  0,  0,    0,  0,  0,  0,
                                      0,  0,  0,  0,    0,  0,  0,  0,
                                      0,  0,  0,  0,    0,  0,  0,  0 };
static byte buttonArray[48]     = {  54, 55, 56, 57,   58, 59, 60, 61,      // A0-A7
                                     62, 63, 64, 65,   66, 67, 68, 69,      // A8-A15
                                     40, 41, 42, 43,   44, 45, 46, 47,      // D40-D47
                                     32, 33, 34, 35,   36, 37, 38, 39,      // D16-D23
                                     24, 25, 26, 27,   28, 29, 30, 31,      // D24-D31
                                     16, 17, 18, 19,   20, 21, 22, 23 };    // D32-D39

byte lastButtonPressed         = 0;
#define DEBOUNCE_DELAY 50
#define BUTTON_PRESSED      1
#define BUTTON_NOT_PRESSED  0
/* ************************************************************************************* */

/**
 * Initial configuration
 */
void setup()
{
  Serial.begin(9600);  // Use the serial port to report back readings
  
  Wire.begin();        // Wake up I2C bus
  
  if( ENABLE_MAC_ADDRESS_ROM == true )
  {
    Serial.print(F("Getting MAC address from ROM: "));
    get_MAC(mac);
  } else {
    Serial.print(F("Using static MAC address: "));
  }
  // Print the IP address
  char tmpBuf[17];
  sprintf(tmpBuf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println(tmpBuf);
  
  // Set up the Ethernet library to talk to the Wiznet board
  if( ENABLE_DHCP == true )
  {
    Ethernet.begin(mac);      // Use DHCP
  } else {
    Ethernet.begin(mac, ip);  // Use static address defined above
  }
  
  // Print IP address:
  Serial.print(F("Local IP: "));
  Serial.println(Ethernet.localIP());
  
  Serial.println("Setting input pull-ups");
  Serial.print("\n");
  for( byte i = 0; i < 48; i++)
  {
    pinMode(buttonArray[i], INPUT_PULLUP);
    if( buttontype[i] == BUTTON_TYPE_LATCHING)
      lastButtonState[i] = digitalRead( buttonArray[i] );
    Serial.print(buttonArray[i]);
    Serial.print(" ");
    if( (i+1) % 8 == 0)
      Serial.print("\n");
  }
  Serial.println();

  /* Connect to MQTT broker */
  Serial.println("connecting...");
  client.setServer(broker, 1883);
  client.setCallback(callback);
  Serial.println("Ready.");
}


/**
 * Main program loop
 */
void loop()
{
  if (!client.connected()) {
    reconnect();
  }
  
  client.loop();
  
  byte i;
  for( i = 0; i < 48; i++) {
    processButtonDigital( i );
  }
}

/**
 */
void processButtonDigital( byte buttonId )
{
    int sensorReading = digitalRead( buttonArray[buttonId] );
    //Serial.print(buttonId, DEC);
    //Serial.print(": ");
    //Serial.println(sensorReading, DEC);
    if( buttontype[buttonId] == BUTTON_TYPE_LATCHING )
    {
        if( sensorReading != lastButtonState[buttonId] )  // Input pulled low to GND. Button pressed.
        {
            //Serial.println( "Button pressed" );
            //if( ((millis() - lastActivityTime) > DEBOUNCE_DELAY) || (buttonId != lastButtonPressed) )  // Proceed if we haven't seen a recent event on this button
            if( (millis() - lastActivityTime) > DEBOUNCE_DELAY )  // Proceed if we haven't seen a recent event on this button
            {
                lastActivityTime = millis();
        
                lastButtonPressed = buttonId;
                Serial.print( "transition on ");
                Serial.print( buttonId, DEC );
                Serial.print(" (input ");
                Serial.print( buttonArray[buttonId] );
                Serial.println(")");
      
                String messageString = String(panelId) + "-" + String(buttonId);
                messageString.toCharArray(messageBuffer, messageString.length()+1);
      
                //String topicString = "device/" + String(panelId) + "/button";
                String topicString = "buttons";
                topicString.toCharArray(topicBuffer, topicString.length()+1);

                //client.publish(topicBuffer, messageBuffer);
      
                client.publish("buttons", messageBuffer);

                Serial.print(F("Button pressed: "));
                Serial.println(buttonId);
                lastButtonState[buttonId] = sensorReading;
            }
        }
    }
    else
    {
        if( sensorReading == 0 )  // Input pulled low to GND. Button pressed.
        {
            //Serial.println( "Button pressed" );
            if( lastButtonState[buttonId] == 0 )   // The button was previously un-pressed
            {
                if((millis() - lastActivityTime) > DEBOUNCE_DELAY || (buttonId != lastButtonPressed) )  // Proceed if we haven't seen a recent event on this button
                {
                    lastActivityTime = millis();
        
                    lastButtonPressed = buttonId;
                    Serial.print( "transition on ");
                    Serial.print( buttonId, DEC );
                    Serial.print(" (input ");
                    Serial.print( buttonArray[buttonId] );
                    Serial.println(")");
            
                    String messageString = String(panelId) + "-" + String(buttonArray[buttonId]);
                    messageString.toCharArray(messageBuffer, messageString.length()+1);
                
                    //String topicString = "device/" + String(panelId) + "/button";
                    String topicString = "buttons";
                    topicString.toCharArray(topicBuffer, topicString.length()+1);
          
                    //client.publish(topicBuffer, messageBuffer);
                
                    client.publish("buttons", messageBuffer);
                }
            }
            else
            {
                // Transition off
                //digitalWrite(statusArray[buttonId-1], LOW);
                //digitalWrite(13, LOW);
            }
            lastButtonState[buttonId] = 1;
        }
        else 
        {
            lastButtonState[buttonId] = 0;
        }
    }
}

/************************************
 *    Get MAC address from EEPROM   *
 ************************************/
char get_MAC(uint8_t *mac_buf){
  // Random MAC address stored in EEPROM
  if (EEPROM.read(MAC_START) == MAC_PREFIX) {
    for (int ee_idx = MAC(1), buf_idx=0; ee_idx <= (MAC_START + MAC_LEN); ee_idx++, buf_idx++) {
      mac_buf[buf_idx] = EEPROM.read(ee_idx);
    }
  } else {
    randomSeed(analogRead(0));
    EEPROM.write(MAC(1), mac_buf[0] & 0xFE);
    for (int ee_idx = MAC(2), buf_idx=1; ee_idx <= (MAC_START + MAC_LEN); ee_idx++,buf_idx++) {
      mac_buf[buf_idx] = random(0, 255);
      EEPROM.write(ee_idx, mac_buf[buf_idx]);
    }
    // Write prefix
    EEPROM.write(MAC_START, MAC_PREFIX);
  }
  return 0;
}

