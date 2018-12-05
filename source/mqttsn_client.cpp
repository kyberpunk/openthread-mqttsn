#include <cstring>

#include "mqttsn_client.hpp"
#include "MQTTSNPacket.h"
#include "MQTTSNConnect.h"
#include "MQTTSNSearch.h"

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
	, mPacketId(0)
	, mConnectCallback(nullptr)
	, mConnectContext(nullptr)
	, mSubscribeCallback(nullptr)
	, mSubscribeContext(nullptr)
	, mPublishReceivedCallback(nullptr)
	, mPublishReceivedContext(nullptr)
	, mAdvertiseCallback(nullptr)
	, mAdvertiseContext(nullptr)
	, mSearchGwCallback(nullptr)
	, mSearchGwContext(nullptr)
	, mRegisterCallback(nullptr)
	, mRegisterContext(nullptr) {
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
	const Ip6::MessageInfo &messageInfo = *static_cast<const Ip6::MessageInfo *>(aMessageInfo);

	uint16_t offset = message.GetOffset();
	uint16_t length = message.GetLength() - message.GetOffset();

	unsigned char* data = new unsigned char[length];
	if (!data) {
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
	case MQTTSN_CONNACK: {
		int connectReturnCode = 0;
		if (MQTTSNDeserialize_connack(&connectReturnCode, data, length) != 1) {
			// TODO: Log error
			break;
		}
		if (client->mConnectCallback) {
			client->mConnectCallback(static_cast<ReturnCode>(connectReturnCode),
					client->mConnectContext);
		}
	}
		break;
	case MQTTSN_SUBACK: {
		int qos;
		unsigned short topicId = 0;
		unsigned short packetId = 0;
		unsigned char subscribeReturnCode = 0;
		if (MQTTSNDeserialize_suback(&qos, &topicId, &packetId,
				&subscribeReturnCode, data, length) != 1) {
			// TODO: Log error
			break;
		}
		if (client->mSubscribeCallback) {
			client->mSubscribeCallback(static_cast<ReturnCode>(subscribeReturnCode), static_cast<TopicId>(topicId),
					client->mSubscribeContext);
		}
	}
		break;
	case MQTTSN_PUBLISH: {
		int qos;
		unsigned short packetId = 0;
		int payloadLength = 0;
		unsigned char* payload;
		unsigned char dup = 0;
		unsigned char retained = 0;
		MQTTSN_topicid topicId;
		if (MQTTSNDeserialize_publish(&dup, &qos, &retained, &packetId,
				&topicId, &payload, &payloadLength, data, length) != 1) {
			// TODO: Log error
			break;
		}
		if (client->mPublishReceivedCallback) {
			client->mPublishReceivedCallback(payload, payloadLength,
					client->mPublishReceivedContext);
		}
	}
		break;
	case MQTTSN_ADVERTISE: {
		unsigned char gatewayId = 0;
		unsigned short duration = 0;
		if (MQTTSNDeserialize_advertise(&gatewayId, &duration, data, length) != 1) {
			// TODO: Log error
			break;
		}
		if (client->mAdvertiseCallback) {
			client->mAdvertiseCallback(messageInfo.GetPeerAddr(), messageInfo.GetPeerPort(),
					static_cast<uint8_t>(gatewayId), static_cast<uint32_t>(duration), client->mAdvertiseContext);
		}
	}
		break;
	case MQTTSN_GWINFO: {
		unsigned char gatewayId = 0;
		unsigned short addressLength = 1;
		unsigned char* addressText = nullptr;
		if (MQTTSNDeserialize_gwinfo(&gatewayId, &addressLength, &addressText, data, length) != 1) {
			// TODO: Log error
			break;
		}
		if (client->mSearchGwCallback) {
			Ip6::Address address;
			if (addressLength > 0) {
				address.FromString(std::string(reinterpret_cast<char *>(addressText), static_cast<size_t>(addressLength)).c_str());
			} else {
				address = messageInfo.GetPeerAddr();
			}
			client->mSearchGwCallback(address, gatewayId, client->mSearchGwContext);
		}
	}
		break;
	case MQTTSN_REGACK: {
		unsigned short topicId;
		unsigned short packetId;
		unsigned char returnCode;
		if (MQTTSNDeserialize_regack(&topicId, &packetId, &returnCode, data, length) != 1) {
			// TODO: Log error
			break;
		}
		if (client->mRegisterCallback) {
			client->mRegisterCallback(static_cast<ReturnCode>(returnCode), static_cast<TopicId>(topicId),
					client->mRegisterContext);
		}
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
	char* clientIdString = nullptr;
	MQTTSNPacket_connectData options = MQTTSNPacket_connectData_initializer;
	int32_t length = 0;

	if (mIsConnected) {
		error = OT_ERROR_INVALID_STATE;
		goto exit;
	}

	clientIdString = new char[config.GetClientId().length() + 1];
	if (clientIdString == nullptr) {
		error = OT_ERROR_NO_BUFS;
		goto exit;
	}
	strcpy(clientIdString, config.GetClientId().c_str());
	MQTTSNString clientId;
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

otError MqttsnClient::Subscribe(const std::string &topic) {
	otError error = OT_ERROR_NONE;
	Ip6::MessageInfo messageInfo;
	int32_t length = 0;
	MQTTSN_topicid topicIdConfig;
	unsigned char buffer[MAX_PACKET_SIZE];

	char* topicString = new char[topic.length() + 1];
	if (!topicString) {
		error = OT_ERROR_NO_BUFS;
		goto exit;
	}
	strcpy(topicString, topic.c_str());
	topicIdConfig.type = MQTTSN_TOPIC_TYPE_NORMAL;
	topicIdConfig.data.long_.name = topicString;
	topicIdConfig.data.long_.len = topic.length();

	length = MQTTSNSerialize_subscribe(buffer, MAX_PACKET_SIZE, 0, 2, mPacketId++, &topicIdConfig);
	delete[] topicString;
	if (length <= 0) {
		error = OT_ERROR_FAILED;
		goto exit;
	}
	SuccessOrExit(error = SendMessage(buffer, length));

exit:
	return error;
}

otError MqttsnClient::Register(const std::string &topic) {
	otError error = OT_ERROR_NONE;
	MQTTSNString topicName;
	unsigned char buffer[MAX_PACKET_SIZE];
	int length = -1;
	char *topicNameStr = new char[topic.length() + 1];
	if (!topicNameStr) {
		error = OT_ERROR_NO_BUFS;
		goto exit;
	}
	topicName.cstring = topicNameStr;
	length = MQTTSNSerialize_register(buffer, MAX_PACKET_SIZE, 0, mPacketId++, &topicName);
	delete[] topicNameStr;
	if (length <= 0) {
		error = OT_ERROR_FAILED;
		goto exit;
	}
	SuccessOrExit(error = SendMessage(buffer, length));

exit:
	return error;
}

otError MqttsnClient::SetConnectCallback(ConnectCallbackFunc callback, void* context) {
	mConnectCallback = callback;
	mConnectContext = context;
	return OT_ERROR_NONE;
}

otError MqttsnClient::SetSubscribeCallback(SubscribeCallbackFunc callback, void* context) {
	mSubscribeCallback = callback;
	mSubscribeContext = context;
	return OT_ERROR_NONE;
}

otError MqttsnClient::SetPublishReceivedCallback(PublishReceivedCallbackFunc callback, void* context) {
	mPublishReceivedCallback = callback;
	mPublishReceivedContext = context;
	return OT_ERROR_NONE;
}

otError MqttsnClient::SetAdvertiseCallback(AdvertiseCallbackFunc callback, void* context) {
	mAdvertiseCallback = callback;
	mAdvertiseContext = context;
	return OT_ERROR_NONE;
}

otError MqttsnClient::SetSearchGwCallback(SearchGwCallbackFunc callback, void* context) {
	mSearchGwCallback = callback;
	mSearchGwContext = context;
	return OT_ERROR_NONE;
}

otError MqttsnClient::SetRegisterCallback(RegisterCallbackFunc callback, void* context) {
	mRegisterCallback = callback;
	mRegisterContext = context;
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
