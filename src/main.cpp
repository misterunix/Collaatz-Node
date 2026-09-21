#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

#define MSG_FREE 0
#define MSG_BUSY 1

#define LED 8

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len);
uint16_t calculate_16_bit_checksum(const uint8_t *data, size_t length);
void set_hardware_wifi_channel(uint8_t channel);

// Replace with your receiver's MAC address
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#define CHANNEL 3

typedef struct now_msg
{
  uint8_t othermax[6];
  uint8_t senderNode;
  uint8_t othernode;
  uint8_t control;
  uint8_t sequence;
  unsigned long long startnumber;
  unsigned long long length;
  unsigned long long result;
  uint8_t status;
  uint16_t checksum;
} now_msg;

#define MSG_COUNT 64
now_msg msg[MSG_COUNT];

bool ledState = false;

void setup()
{
  pinMode(LED, OUTPUT);
  digitalWrite(LED, ledState);

  Serial.begin(115200);

  for (int i = 0; i < MSG_COUNT; i++)
  {
    msg[i].status = MSG_FREE;
    msg[i].sequence = 0;
    msg[i].checksum = 0;
    msg[i].length = 10000000ULL;
    msg[i].result = 0ULL;
    msg[i].startnumber = 0ULL;
    msg[i].control = 0;
    msg[i].othernode = 255;
    msg[i].senderNode = 255; // 0 is Always the server
  }

  WiFi.mode(WIFI_STA);
  set_hardware_wifi_channel(CHANNEL);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }


  // Register peer
  esp_now_peer_info_t peerInfo = {};
  peerInfo.channel = CHANNEL;
  peerInfo.encrypt = false;
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);

  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Failed to add peer");
    return;
  }




  // Register the send callback
  esp_now_register_send_cb(OnDataSent);
  esp_err_t esp_err_t_register_recv = esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  if (esp_err_t_register_recv != ESP_OK)
  {
    Serial.println("Error registering receive callback");
    return;
  }

  

  Serial.println("ESP-NOW Initialized!");
}

void loop()
{
 // Serial.println("Looping...");
  delay(1000);
}

// Calculates a 16-bit checksum by summing all bytes in the buffer.
uint16_t calculate_16_bit_checksum(const uint8_t *data, size_t length)
{
  uint16_t checksum = 0;
  for (size_t i = 0; i < length; i++)
  {
    checksum += data[i];
  }
  return checksum;
}

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{



  // Copy incoming memory buffer directly into our structure variables
  memcpy(&msg[0], incomingData, sizeof(now_msg));

  Serial.println("\n--- New Packet Received ---");

  Serial.printf("Rcv: %02X:%02X:%02X:%02X:%02X:%02X\n", msg[0].othermax[0],
                msg[0].othermax[1], msg[0].othermax[2], msg[0].othermax[3],
                msg[0].othermax[4], msg[0].othermax[5]);
  Serial.printf("Other node: %i\n", msg[0].senderNode);
  Serial.printf("Control: %i\n", msg[0].control);
  Serial.printf("Sequence: %i\n", msg[0].sequence);
  Serial.println(msg[0].startnumber);
  Serial.println(msg[0].length);
  Serial.println(msg[0].result);
  Serial.printf("Status: %i\n", msg[0].status);
  Serial.printf("Checksum: %04X\n", msg[0].checksum);
    uint16_t tmp_checksum = msg[0].checksum;
  msg[0].checksum = 0;
  uint16_t checksum = calculate_16_bit_checksum((const uint8_t *)&msg[0], sizeof(now_msg));
  Serial.printf("Calculated Checksum: %04X\n", checksum);
  if (checksum == tmp_checksum)
  {
      // Toggle the LED state
  ledState = !ledState;
  digitalWrite(LED, ledState);
    Serial.println("Checksum valid");
  }
  else
  {
    Serial.println("Checksum invalid");
  }
}

void set_hardware_wifi_channel(uint8_t channel)
{
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
}