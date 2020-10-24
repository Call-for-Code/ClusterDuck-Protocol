#include "include/DuckPacket.h"
#include "include/DuckUtils.h"
#include "DuckError.h"
#include "DuckLogger.h"
#include<string>
bool DuckPacket::update(std::vector<byte> duid, std::vector<byte> dataBuffer) {



  bool relaying;
  byte packet_length = dataBuffer.size();

  logdbg("Updating received packet: " + String(duckutils::convertToHex(dataBuffer.data(), packet_length)));

  if (packet_length < MIN_PACKET_LENGTH) {
    logerr("ERROR Packet size is invalid: (" + String(packet_length) + ") Data may be corrupted.");
    return false;
  }

  byte path_pos = dataBuffer[PATH_OFFSET_POS];

  if (path_pos > (packet_length - 1)) {
    logerr("ERROR Path position is invalid (" + String(path_pos) + ") Data may be corrupted.");
    return false;
  }

  // build an rxPacket with data we have received
  packet.duid.insert(packet.duid.end(), &dataBuffer[0], &dataBuffer[DUID_LENGTH]);
  packet.muid.insert(packet.muid.end(), &dataBuffer[MUID_POS], &dataBuffer[TOPIC_POS]);
  packet.topic = dataBuffer[TOPIC_POS];
  packet.path_offset = dataBuffer[PATH_OFFSET_POS];
  
  packet.reserved.insert(packet.reserved.end(), &dataBuffer[RESERVED_POS], &dataBuffer[DATA_POS]);
  packet.data.insert(packet.data.end(), &dataBuffer[DATA_POS], &dataBuffer[path_pos]);
  packet.path.insert(packet.path.end(), &dataBuffer[path_pos], &dataBuffer[packet_length]);
  // update the rx packet byte buffer
  buffer.insert(buffer.end(), dataBuffer.begin(), dataBuffer.end());
  logdbg("Current path: " + String(duckutils::convertToHex(packet.path.data(), packet.path.size())));

  // check if we need to relay the packet
  relaying = relay(duid);
  logdbg("Updated path: " + String(duckutils::convertToHex(packet.path.data(), packet.path.size())));
  logdbg("Updated buffer: " + String(duckutils::convertToHex(buffer.data(), buffer.size())));
  return relaying;
}

int DuckPacket::buildPacketBuffer(byte topic, std::vector<byte> app_data) {

  uint8_t app_data_length = app_data.size();

  loginfo("buildPacketBuffer: DATA LENGTH: " + String(app_data_length) +
             " TOPIC: " + String(topic));

  if (app_data_length > MAX_DATA_LENGTH) {
    return DUCKPACKET_ERR_SIZE_INVALID;
  }
  byte message_id[MUID_LENGTH];

  duckutils::getRandomBytes(MUID_LENGTH, message_id);

  // ----- insert packet header  -----
  // device uid
  buffer.insert(buffer.end(), duid.begin(), duid.end());
  //logdbg("Duid:      " + String(duckutils::convertToHex(duid.data(), duid.size())));

  // message uid
  buffer.insert(buffer.end(), &message_id[0], &message_id[MUID_LENGTH]);
  //logdbg("Muid:      " + String(duckutils::convertToHex(buffer.data(), buffer.size())));
  
  // topic
  buffer.insert(buffer.end(), topic);
  //logdbg("Topic:     " + String(duckutils::convertToHex(buffer.data(), buffer.size())));
  
  // path offset
  byte offset = HEADER_LENGTH + app_data_length;
  buffer.insert(buffer.end(), offset);
  //logdbg("Offset:    " + String(duckutils::convertToHex(buffer.data(), buffer.size())));
  
  // reserved
  buffer.insert(buffer.end(), 0x00);
  buffer.insert(buffer.end(), 0x00);
  //logdbg("Reserved:  " + String(duckutils::convertToHex(buffer.data(), buffer.size())));

  // ----- insert data -----
  buffer.insert(buffer.end(), app_data.begin(), app_data.end());
  //logdbg("Data:      " + String(duckutils::convertToHex(buffer.data(), buffer.size())));

  // ----- insert path -----
  buffer.insert(buffer.end(), duid.begin(), duid.end());
  //logdbg("Path:      " + String(duckutils::convertToHex(buffer.data(), buffer.size())));

  logdbg("Built packet: " + String(duckutils::convertToHex(buffer.data(), buffer.size())));
  return DUCK_ERR_NONE;
}

// checks the current packet against a duid to determine if the
// packet needs to be send back into the mesh for the next hop.
bool  DuckPacket::relay(std::vector<byte> duid) {
  
  int hops = packet.path.size() / DUID_LENGTH;
  if (hops >= MAX_HOPS) {
    logerr("ERROR Max hops reached. Cannot relay packet.");
    return false;
  }

  // we don't have a contains() method but we can use indexOf()
  // if the result is 0 or greater then the substring was found
  // starting at the returned index value.
  String id = duckutils::convertToHex(duid.data(), duid.size());
  String path = duckutils::convertToHex(packet.path.data(), packet.path.size());
  if (path.indexOf(id) >= 0) {
    logdbg("Packet already seen. ignore it.");
    return false;
  }

  // update the path so we are good to send the packet back into the mesh
  packet.path.insert(packet.path.end(), duid.begin(), duid.end());
  buffer.insert(buffer.end(), duid.begin(), duid.end());
  return true;
}