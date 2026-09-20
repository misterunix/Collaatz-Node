#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

#define MSG_FREE 0
#define MSG_BUSY 1

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len);

// Replace with your receiver's MAC address
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#define CHANNEL 3

typedef struct now_msg
{
  uint8_t othermax[6];
  uint8_t mynode;
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

void setup()
{

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
    msg[i].mynode = 0; // 0 is Always the server
  }

  WiFi.mode(WIFI_STA);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register the send callback
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  // Register peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = CHANNEL;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Failed to add peer");
    return;
  }

  Serial.println("ESP-NOW Initialized!");
}

void loop()
{
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
  Serial.printf("Other node: %i\n", msg[0].othernode);
  Serial.printf("Control: %i\n", msg[0].control);
  Serial.printf("Sequence: %i\n", msg[0].sequence);
  Serial.println(msg[0].startnumber);
  Serial.println(msg[0].length);
  Serial.println(msg[0].result);
  Serial.printf("Status: %i\n", msg[0].status);
  Serial.printf("Checksum: %04X\n", msg[0].checksum);
}
