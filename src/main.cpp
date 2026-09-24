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
void pong(uint8_t i);

// Replace with your receiver's MAC address
uint8_t broadcastAddress[] = {0xFF,
                              0xFF,
                              0xFF,
                              0xFF,
                              0xFF,
                              0xFF};
esp_now_peer_info_t peerInfo;
#define CHANNEL 3

uint8_t baseMac[6];

typedef struct now_msg
{
  uint8_t otherMAC[6];            // the mac of a responding device
  uint8_t senderNode;             // the node ID of the sender
  uint8_t recvNodeID;             // the node ID of the responding device
  uint8_t control;                // control flags or commands
  uint8_t sequence;               // sequence number of the message
  unsigned long long startnumber; // starting number for the computation
  unsigned long long length;      // length of the computation range
  unsigned long long result;      // result of the computation
  uint8_t status;                 // status of the message (e.g., MSG_FREE or MSG_BUSY)
  uint16_t checksum;              // 16-bit checksum for data integrity
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
    msg[i].recvNodeID = 255;
    msg[i].senderNode = 255; // 0 is Always the server
  }

  WiFi.mode(WIFI_STA);
  set_hardware_wifi_channel(CHANNEL);

  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  Serial.print("Base MAC: ");
  for (int i = 0; i < 6; i++)
  {
    Serial.printf("%02X", baseMac[i]);
    if (i < 5)
      Serial.print(":");
  }
  Serial.println();

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register peer

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
  uint16_t hldcrc = msg[0].checksum;
  msg[0].checksum = calculate_16_bit_checksum((const uint8_t *)&msg[0], sizeof(now_msg));
  if (hldcrc != msg[0].checksum)
  {
    // drop packet if checksum does not match
    Serial.println("Checksum mismatch, dropping packet");
    return;
  }

  switch (msg[0].control)
  {
  case 1:
    // recv ping
    pong(msg[0].senderNode);
    break;
  }

  Serial.printf("Rcv: %02X:%02X:%02X:%02X:%02X:%02X ", msg[0].otherMAC[0],
                msg[0].otherMAC[1], msg[0].otherMAC[2], msg[0].otherMAC[3],
                msg[0].otherMAC[4], msg[0].otherMAC[5]);
  Serial.printf("Other node: %i ", msg[0].senderNode);
  Serial.printf("Control: %i ", msg[0].control);
  Serial.printf("Sequence: %i ", msg[0].sequence);
  Serial.println(msg[0].startnumber);
  Serial.println(msg[0].length);
  Serial.println(msg[0].result);
  Serial.printf("Status: %i ", msg[0].status);
  Serial.printf("Checksum: %04X\n", msg[0].checksum);
  uint16_t tmp_checksum = msg[0].checksum;
  msg[0].checksum = 0;
  uint16_t checksum = calculate_16_bit_checksum((const uint8_t *)&msg[0], sizeof(now_msg));
  Serial.printf("Calculated Checksum: %04X\n", checksum);
  if (checksum != tmp_checksum)
  {
    Serial.println("Checksum invalid");
    return;
  }
  // Toggle the LED state
  ledState = !ledState;
  digitalWrite(LED, ledState);
  Serial.println("Checksum valid");

  if (msg[0].senderNode != 0)
  {
    Serial.println("Non-zero sender node detected");
    return;
  }

  switch (msg[0].control)
  {
  case 1:
    pong(uint8_t(2));
    break;
  default:
    Serial.println("Unknown control command");
    break;
  }
}

void set_hardware_wifi_channel(uint8_t channel)
{
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
}

void pong(uint8_t i)
{
  delay(random(100, 1000));

  if (i != 0)
  {
    // Not master node, ignore pong
    return;
  }

  memcpy(msg[i].otherMAC, baseMac, 6);
  /*
  msg[i].otherMAC[0] = baseMac[0];
  msg[i].otherMAC[1] = baseMac[1];
  msg[i].otherMAC[2] = baseMac[2];
  msg[i].otherMAC[3] = baseMac[3];
  msg[i].otherMAC[4] = baseMac[4];
  msg[i].otherMAC[5] = baseMac[5];
  */
  msg[i].status = MSG_FREE;
  msg[i].sequence = 0;
  msg[i].checksum = 0;
  msg[i].length = 10000000ULL;
  msg[i].result = 0ULL;
  msg[i].startnumber = 0ULL;
  msg[i].control = 2;
  msg[i].recvNodeID = 0;
  msg[i].senderNode = 1; // 0 is Always the server
  msg[i].checksum = 0;
  uint16_t checksum = calculate_16_bit_checksum((const uint8_t *)&msg[i], sizeof(now_msg));
  msg[i].checksum = checksum;

  esp_err_t result = esp_now_send(peerInfo.peer_addr, (const uint8_t *)&msg[0], sizeof(now_msg));
  if (result == ESP_OK)
  {
    Serial.println("Sent with success");
  }
  else
  {
    Serial.println("Error sending the data");
  }
}