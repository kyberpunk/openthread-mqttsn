#include <cstring>

#include "mqttsn_client.hpp"
#include "MQTTSNPacket.h"
#include "MQTTSNConnect.h"

#define MAX_PACKET_SIZE 100
#define MQTTSN_MIN_PACKET_LENGTH 2

namespace ot {

namespace Mqttsn {

MqttsnClient::MqttsnClient(Instance& aInstance)
    : InstanceLocator(aInstance)
    , mSocket(aInstance.GetThreadNetif().GetIp6().GetUdp())
	, mIsConnected(false)
	, mConfig()
	, mConnectCallback() {
	;
}

static int32_t PacketDecode(unsigned char* data, size_t length) {
	int rc = MQTTSNPACKET_READ_ERROR;
	int lenlen = 0;
	int datalen = 0;

	if (length < MQTTSN_MIN_PACKET_LENGTH) {
		return MQTTSNPACKET_READ_ERROR;
	}

	lenlen = MQTTSNPacket_decode(data, length, &datalen);
	if (datalen != static_cast<int>(length)) {
		return MQTTSNPACKET_READ_ERROR;
	}
	return data[lenlen]; // return the packet type
}

void MqttsnClient::HandleUdpReceive(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo) {
	MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
	Message &message = *static_cast<Message *>(aMessage);

	uint16_t offset = message.GetOffset();
	uint16_t length = message.GetLength() - message.GetOffset();
	unsigned char* data = new unsigned char[length];
	if (data == nullptr) {
		// TODO: Log error
		return;
	}
	message.Read(offset, length, data);

	int32_t decodedPacketType = PacketDecode(data, length);
	if (decodedPacketType == MQTTSNPACKET_READ_ERROR) {
		// TODO: Log error
		goto error;
	}
	switch (decodedPacketType) {
	case MQTTSN_CONNACK:
		int connackCode;
		if (MQTTSNDeserialize_connack(&connackCode, data, length) != 1) {
			// TODO: Log error
			break;
		}
		if (client->mConnectCallback != nullptr) {
			client->mConnectCallback(static_cast<ReturnCode>(connackCode));
		}
		break;
	default:
		break;
	}

error:
	delete[] data;
}

otError MqttsnClient::Start(uint16_t port) {
	otError error = OT_ERROR_NONE;
	Ip6::SockAddr sockaddr;
	sockaddr.mPort = port;

	SuccessOrExit(error = mSocket.Open(&MqttsnClient::HandleUdpReceive, this));
	SuccessOrExit(error = mSocket.Bind(sockaddr));

exit:
	return error;
}

otError MqttsnClient::Stop() {
	return mSocket.Close();
}

otError MqttsnClient::Connect(MqttsnConfig &config) {
	otError error = OT_ERROR_NONE;
	if (mIsConnected) {
		error = OT_ERROR_INVALID_STATE;
		goto error;
	}

	char* clientIdString = new char[config.GetClientId().length() + 1];
	if (clientIdString == nullptr) {
		error = OT_ERROR_FAILED;
	}
	strcpy(clientId.cstring, config.GetClientId().c_str());
	MQTTSNPacket_connectData options = MQTTSNPacket_connectData_initializer;
	MQTTSNString clientId;
	clientId.cstring = clientIdString;
	options.clientID = clientId;
	options.duration = config.GetDuration();
	options.cleansession = static_cast<unsigned char>(config.GetCleanSession());

	unsigned char buffer[MAX_PACKET_SIZE];
	int32_t length = MQTTSNSerialize_connect(buffer, MAX_PACKET_SIZE, &options);
	delete[] clientIdString;
	if (length <= 0) {
		error = OT_ERROR_FAILED;
		goto error;
	}

	otMessageSettings settings;
	settings.mLinkSecurityEnabled = false;
	settings.mPriority = OT_MESSAGE_PRIORITY_NORMAL;
	Message *message = mSocket.NewMessage(length, &settings);
	if (message == nullptr) {
		error = OT_ERROR_FAILED;
		goto error;
	}
	message->Write(0, length, buffer);
	message->SetOffset(0);

	Ip6::MessageInfo messageInfo;
	messageInfo.SetPeerAddr(config.GetAddress());
	messageInfo.SetPeerPort(config.GetPort());
	messageInfo.SetInterfaceId(OT_NETIF_INTERFACE_ID_THREAD);

	SuccessOrExit(error = mSocket.SendTo(*message, messageInfo));

	mConfig = config;
exit:
	return error;
}

otError MqttsnClient::SetConnectCallback(ConnectCallbackFunc callback) {
	mConnectCallback = callback;
	return OT_ERROR_NONE;
}

}
}
