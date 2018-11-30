#include <cstring>

#include "mqttsn_client.hpp"
#include "MQTTSNPacket.h"
#include "MQTTSNConnect.h"

#define MAX_PACKET_SIZE 100
#define MQTTSN_MIN_PACKET_LENGTH 2

namespace ot {

namespace Mqttsn {
// TODO: Implement QoS

MqttsnClient::MqttsnClient(Instance& aInstance)
    : InstanceLocator(aInstance)
    , mSocket(aInstance.GetThreadNetif().GetIp6().GetUdp())
	, mIsConnected(false)
	, mConfig()
	, mConnectCallback(nullptr)
	, mConnectContext(nullptr)
	, mSubscribeCallback(nullptr)
	, mSubscribeContext(nullptr)
	, mPacketId(0)
	, mDataReceivedCallback(nullptr)
	, mDataReceivedContext(nullptr) {
	;
}

static int32_t PacketDecode(unsigned char* data, size_t length) {
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
		int connectReturnCode;
		if (MQTTSNDeserialize_connack(&connectReturnCode, data, length) != 1) {
			// TODO: Log error
			break;
		}
		if (client->mConnectCallback != nullptr) {
			client->mConnectCallback(static_cast<ReturnCode>(connectReturnCode),
					client->mConnectContext);
		}
		break;
	case MQTTSN_SUBACK: {
		int qos;
		unsigned short topicId;
		unsigned short packetId;
		unsigned char subscribeReturnCode;
		if (MQTTSNDeserialize_suback(&qos, &topicId, &packetId,
				&subscribeReturnCode, data, length) != 1) {
			// TODO: Log error
			break;
		}
		if (client->mSubscribeCallback != nullptr) {
			client->mSubscribeCallback(
					static_cast<ReturnCode>(subscribeReturnCode),
					client->mSubscribeContext);
		}
		break;
	}
	case MQTTSN_PUBLISH: {
		int qos;
		unsigned short packetId;
		int payloadLength;
		unsigned char* payload;
		unsigned char dup;
		unsigned char retained;
		MQTTSN_topicid topicId;
		if (MQTTSNDeserialize_publish(&dup, &qos, &retained, &packetId,
				&topicId, &payload, &payloadLength, data, length) != 1) {
			// TODO: Log error
			break;
		}
		if (client->mDataReceivedCallback != nullptr) {
			client->mDataReceivedCallback(payload, payloadLength,
					client->mDataReceivedContext);
		}
		break;
	}
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
	char* clientIdString = nullptr;
	MQTTSNPacket_connectData options = MQTTSNPacket_connectData_initializer;
	int32_t length = 0;

	if (mIsConnected) {
		error = OT_ERROR_INVALID_STATE;
		goto exit;
	}

	clientIdString = new char[config.GetClientId().length() + 1];
	if (clientIdString == nullptr) {
		error = OT_ERROR_FAILED;
		goto exit;
	}
	MQTTSNString clientId;
	strcpy(clientId.cstring, config.GetClientId().c_str());
	clientId.cstring = clientIdString;
	options.clientID = clientId;
	options.duration = config.GetDuration();
	options.cleansession = static_cast<unsigned char>(config.GetCleanSession());

	unsigned char buffer[MAX_PACKET_SIZE];
	length = MQTTSNSerialize_connect(buffer, MAX_PACKET_SIZE, &options);
	delete[] clientIdString;
	if (length <= 0) {
		error = OT_ERROR_FAILED;
		goto exit;
	}
	SuccessOrExit(error = SendMessage(buffer, length));

	mConfig = config;
exit:
	return error;
}

otError MqttsnClient::SetConnectCallback(ConnectCallbackFunc callback, void* context) {
	mConnectCallback = callback;
	mConnectContext = context;
	return OT_ERROR_NONE;
}

otError MqttsnClient::Subscribe(const std::string &topic) {
	otError error = OT_ERROR_NONE;
	Ip6::MessageInfo messageInfo;
	int32_t length = 0;
	MQTTSN_topicid topicIdConfig;

	char* topicString = new char[topic.length() + 1];
	if (topicString == nullptr) {
		error = OT_ERROR_FAILED;
		goto exit;
	}
	strcpy(topicString, topic.c_str());
	topicIdConfig.type = MQTTSN_TOPIC_TYPE_NORMAL;
	topicIdConfig.data.long_.name = topicString;
	topicIdConfig.data.long_.len = topic.length();

	unsigned char buffer[MAX_PACKET_SIZE];
	length = MQTTSNSerialize_subscribe(buffer, MAX_PACKET_SIZE, 0, 2, mPacketId++, &topicIdConfig);
	delete[] topicString;
	if (length <= 0) {
		error = OT_ERROR_FAILED;
		goto exit;
	}
	SuccessOrExit(error =SendMessage(buffer, length));

exit:
	return error;
}

otError MqttsnClient::SetSubscribeCallback(SubscribeCallbackFunc callback, void* context) {
	mSubscribeCallback = callback;
	mSubscribeContext = context;
	return OT_ERROR_NONE;
}

otError MqttsnClient::SetDataReceivedCallback(DataReceivedCallbackFunc callback, void* context) {
	mDataReceivedCallback = callback;
	mDataReceivedContext = context;
	return OT_ERROR_NONE;
}

otError MqttsnClient::SendMessage(unsigned char* buffer, int32_t length) {
	return SendMessage(buffer, length, mConfig.GetAddress(), mConfig.GetPort());
}

otError MqttsnClient::SendMessage(unsigned char* buffer, int32_t length, const Ip6::Address &address, uint16_t port) {
	Ip6::MessageInfo messageInfo;
	Message *message = nullptr;

	otError error = OT_ERROR_NONE;
	message->Write(0, length, buffer);
	message->SetOffset(0);

	messageInfo.SetPeerAddr(address);
	messageInfo.SetPeerPort(port);
	messageInfo.SetInterfaceId(OT_NETIF_INTERFACE_ID_THREAD);

	SuccessOrExit(error = mSocket.SendTo(*message, messageInfo));

	otMessageSettings settings;
	settings.mLinkSecurityEnabled = false;
	settings.mPriority = OT_MESSAGE_PRIORITY_NORMAL;
	VerifyOrExit((message = mSocket.NewMessage(length, &settings)) != nullptr,
            error = OT_ERROR_NO_BUFS);
	SuccessOrExit(error = mSocket.SendTo(*message, messageInfo));

exit:
	if (error != OT_ERROR_NONE && message != NULL) {
        message->Free();
    }
	return error;
}

}
}
