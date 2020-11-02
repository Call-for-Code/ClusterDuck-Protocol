#include "../MamaDuck.h"

int MamaDuck::setupWithDefaults(std::vector<byte> deviceId, String ssid, String password) {
  int err = Duck::setupWithDefaults(deviceId, ssid, password);

  if (err != DUCK_ERR_NONE) {
    logerr("ERROR setupWithDefaults rc = " + String(err));
    return err;
  }

  err = setupRadio();
  if (err != DUCK_ERR_NONE) {
    logerr("ERROR setupWithDefaults rc = " + String(err));
    return err;
  }

  err = setupWifi();
  if (err != DUCK_ERR_NONE) {
    logerr("ERROR setupWithDefaults rc = " + String(err));
    return err;
  }

  err = setupDns();
  if (err != DUCK_ERR_NONE) {
    logerr("ERROR setupWithDefaults rc = " + String(err));
    return err;
  }

  err = setupWebServer(true);
  if (err != DUCK_ERR_NONE) {
    logerr("ERROR setupWithDefaults rc = " + String(err));
    return err;
  }

  err = setupOTA();
  if (err != DUCK_ERR_NONE) {
    logerr("ERROR setupWithDefaults rc = " + String(err));
    return err;
  }
  duckutils::getTimer().every(CDPCFG_MILLIS_ALIVE, imAlive);
  loginfo("MamaDuck setup done");
  return DUCK_ERR_NONE;
}

void MamaDuck::handleReceivedPacket() {

  loginfo("handleReceivedPacket()...");

  rxPacket->reset();

  std::vector<byte> data;
  int err = duckRadio->getReceivedData(&data);

  if (err != DUCK_ERR_NONE) {
    logerr("ERROR failed to get data from DuckRadio. rc = "+ String(err));
    return;
  }

  bool relay = rxPacket->update(duid, data);

  if (relay) {
    loginfo("====> handleReceivedPacket() packet needs relay.....");

    // NOTE:
    // Ducks will only handle received message one at a time, so there is a chance the
    // packet being sent below will never be received, especially if the cluster is small
    // there are not many alternative paths to reach other mama ducks that could relay the packet.
    // We could add some kind of random delay before the message is sent, but that's not really a generic solution
    // delay(500);
    if (rxPacket->getCdpPacket().topic == reservedTopic::ping) {
      err = sendPong();
      if (err != DUCK_ERR_NONE) {
        logerr("ERROR failed to send pong message. rc = "+ String(err));
        return;
      }
    } else {
      err = duckRadio->relayPacket(rxPacket);
      loginfo("====> handleReceivedPacket() packet sent");

      if (err != DUCK_ERR_NONE) {
        logerr("ERROR failed to send data. rc = "+ String(err));
        return;
      }
    }
  }
}

void MamaDuck::run() {

  handleOtaUpdate();

  if (duckutils::getDuckInterrupt()) {
    duckutils::getTimer().tick();
  }

  // Mama ducks can also receive packets from other ducks
  // For safe processing of the received packet we make sure
  // to disable interrupts, before handling the received packet.
  if (getReceiveFlag()) {
    setReceiveFlag(false);
    duckutils::setDuckInterrupt(false);

    // Here we check whether a packet needs to be relayed or not
    handleReceivedPacket();

    duckutils::setDuckInterrupt(true);
    startReceive();
  }
  processPortalRequest();
}