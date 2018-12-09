#include <cstring>

#include "mqttsn_client.hpp"
#include "MQTTSNPacket.h"
#include "MQTTSNConnect.h"
#include "MQTTSNSearch.h"
#include "fsl_debug_console.h"

#define MAX_PACKET_SIZE 255
#define MQTTSN_MIN_PACKET_LENGTH 2

namespace ot {

namespace Mqttsn {
// TODO: Implement QoS and DUP behavior
// TODO: Implement OT logging
// TODO: Implement client ping messages
// TODO: Implement timeouts

MqttsnClient::MqttsnClient(Instance& instance)
    : InstanceLocator(instance)
    , mSocket(GetInstance().GetThreadNetif().GetIp6().GetUdp())
	, mIsConnected(false)
	, mConfig()
	, mPacketId(1)
	, mIsSleeping(false)
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
	, mRegisterContext(nullptr)
	, mPublishedCallback(nullptr)
    , mPublishedContext(nullptr)
	, mUnsubscribedCallback(nullptr)
    , mUnsubscribedContext(nullptr)
	, mDisconnectedCallback(nullptr)
	, mDisconnectedContext(nullptr) {
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

// TODO: Verify sender address
void MqttsnClient::HandleUdpReceive(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo) {
	MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
	Message &message = *static_cast<Message *>(aMessage);
	const Ip6::MessageInfo &messageInfo = *static_cast<const Ip6::MessageInfo *>(aMessageInfo);

	uint16_t offset = message.GetOffset();
	uint16_t length = message.GetLength() - message.GetOffset();

	if (length > MAX_PACKET_SIZE) {
		return;
	}

	unsigned char* data = new unsigned char[length];
	if (!data) {
		return;
	}
	message.Read(offset, length, data);

	// Dump packet
	PRINTF("UDP message received:\r\n");
	for (int32_t i = 0; i < length; i++) {
		if (i > 0) {
			PRINTF(" ");
		}
		PRINTF("%02X", data[i]);
	}
	PRINTF("\r\n");

	int32_t decodedPacketType = PacketDecode(data, length);
	if (decodedPacketType == MQTTSNPACKET_READ_ERROR) {
		goto error;
	}
	PRINTF("Packet type: %d\r\n", decodedPacketType);

	// TODO: Refactor switch to use separate handle functions
	switch (decodedPacketType) {
	case MQTTSN_CONNACK: {
		int connectReturnCode = 0;
		if (MQTTSNDeserialize_connack(&connectReturnCode, data, length) != 1) {
			break;
		}
		client->mIsConnected = true;
		if (client->mConnectCallback) {
			client->mConnectCallback(static_cast<ReturnCode>(connectReturnCode),
					client->mConnectContext);
		}
	}
		break;
	case MQTTSN_SUBACK: {
		if (!client->mIsConnected) {
			break;
		}

		int qos;
		unsigned short topicId = 0;
		unsigned short packetId = 0;
		unsigned char subscribeReturnCode = 0;
		if (MQTTSNDeserialize_suback(&qos, &topicId, &packetId, &subscribeReturnCode,
				data, length) != 1) {
			break;
		}
		if (client->mSubscribeCallback) {
			client->mSubscribeCallback(static_cast<ReturnCode>(subscribeReturnCode),
					static_cast<TopicId>(topicId), client->mSubscribeContext);
		}
	}
		break;
	case MQTTSN_PUBLISH: {
		if (!client->mIsConnected) {
			break;
		}

		int qos;
		unsigned short packetId = 0;
		int payloadLength = 0;
		unsigned char* payload;
		unsigned char dup = 0;
		unsigned char retained = 0;
		MQTTSN_topicid topicId;
		if (MQTTSNDeserialize_publish(&dup, &qos, &retained, &packetId,
				&topicId, &payload, &payloadLength, data, length) != 1) {
			break;
		}
		if (client->mPublishReceivedCallback) {
			client->mPublishReceivedCallback(payload, payloadLength, static_cast<Qos>(qos),
					static_cast<TopicId>(topicId.data.id), client->mPublishReceivedContext);
		}
	}
		break;
	case MQTTSN_ADVERTISE: {
		unsigned char gatewayId = 0;
		unsigned short duration = 0;
		if (MQTTSNDeserialize_advertise(&gatewayId, &duration, data, length) != 1) {
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
		if (!client->mIsConnected) {
			break;
		}

		unsigned short topicId;
		unsigned short packetId;
		unsigned char returnCode;
		if (MQTTSNDeserialize_regack(&topicId, &packetId, &returnCode, data, length) != 1) {
			break;
		}
		if (client->mRegisterCallback) {
			client->mRegisterCallback(static_cast<ReturnCode>(returnCode), static_cast<TopicId>(topicId),
					client->mRegisterContext);
		}
	}
		break;
	case MQTTSN_PUBACK: {
		if (!client->mIsConnected) {
			break;
		}

		unsigned short topicId;
		unsigned short packetId;
		unsigned char returnCode;
		if (MQTTSNDeserialize_puback(&topicId, &packetId, &returnCode, data, length) != 1) {
			break;
		}
		if (client->mPublishedCallback) {
			client->mPublishedCallback(static_cast<ReturnCode>(returnCode), static_cast<TopicId>(topicId),
					client->mPublishedContext);
		}
	}
		break;
	case MQTTSN_UNSUBACK: {
		if (!client->mIsConnected) {
			break;
		}

		unsigned short packetId;
		if (MQTTSNDeserialize_unsuback(&packetId, data, length) != 1) {
			break;
		}
		if (client->mUnsubscribedCallback) {
			client->mUnsubscribedCallback(client->mUnsubscribedContext);
		}
	}
		break;
	case MQTTSN_PINGREQ: {
		MQTTSNString clientId;
		if (MQTTSNDeserialize_pingreq(&clientId, data, length) != 1) {
			break;
		}

		int32_t packetLength = -1;
		unsigned char buffer[MAX_PACKET_SIZE];
		packetLength = MQTTSNSerialize_pingresp(buffer, MAX_PACKET_SIZE);
		if (packetLength <= 0) {
			break;
		}
		if (client->SendMessage(buffer, packetLength, messageInfo.GetPeerAddr(), messageInfo.GetPeerPort()) != OT_ERROR_NONE) {
			break;
		}
	}
		break;
	case MQTTSN_DISCONNECT: {
		if (!client->mIsConnected || client->mIsSleeping) {
			break;
		}

		int duration;
		if (MQTTSNDeserialize_disconnect(&duration, data, length) != 1) {
			break;
		}
		client->mIsConnected = false;
		if (client->mDisconnectedCallback) {
			client->mDisconnectedCallback(client->mDisconnectedContext);
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

	SuccessOrExit(error = mSocket.Open(MqttsnClient::HandleUdpReceive, this));
	SuccessOrExit(error = mSocket.Bind(sockaddr));

exit:
	return error;
}

otError MqttsnClient::Stop() {
	return mSocket.Close();
}

otError MqttsnClient::ProcessMessages() {
	return OT_ERROR_NONE;
}

otError MqttsnClient::Connect(MqttsnConfig &config) {
	otError error = OT_ERROR_NONE;
	int32_t length = -1;
	MQTTSNPacket_connectData options = MQTTSNPacket_connectData_initializer;

	if (mIsConnected) {
		error = OT_ERROR_INVALID_STATE;
		goto exit;
	}
	mConfig = config;

	MQTTSNString clientId;
	clientId.cstring = const_cast<char *>(config.GetClientId().c_str());
	options.clientID = clientId;
	options.duration = config.GetDuration();
	options.cleansession = static_cast<unsigned char>(config.GetCleanSession());

	unsigned char buffer[MAX_PACKET_SIZE];
	length = MQTTSNSerialize_connect(buffer, MAX_PACKET_SIZE, &options);
	if (length <= 0) {
		error = OT_ERROR_FAILED;
		goto exit;
	}
	SuccessOrExit(error = SendMessage(buffer, length));

exit:
	return error;
}

otError MqttsnClient::Subscribe(const std::string &topic, Qos qos) {
	otError error = OT_ERROR_NONE;
	int32_t length = -1;
	Ip6::MessageInfo messageInfo;
	MQTTSN_topicid topicIdConfig;
	unsigned char buffer[MAX_PACKET_SIZE];

	if (!mIsConnected) {
		error = OT_ERROR_INVALID_STATE;
		goto exit;
	}

	// TODO: Implement QoS
	if (qos != Qos::MQTTSN_QOS0) {
		error = OT_ERROR_NOT_IMPLEMENTED;
		goto exit;
	}

	topicIdConfig.type = MQTTSN_TOPIC_TYPE_NORMAL;
	topicIdConfig.data.long_.name = const_cast<char *>(topic.c_str());
	topicIdConfig.data.long_.len = topic.length();

	length = MQTTSNSerialize_subscribe(buffer, MAX_PACKET_SIZE, 0, static_cast<int>(qos), mPacketId++, &topicIdConfig);
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
	int32_t length = -1;
	MQTTSNString topicName;
	unsigned char buffer[MAX_PACKET_SIZE];

	if (!mIsConnected) {
		error = OT_ERROR_INVALID_STATE;
		goto exit;
	}

	topicName.cstring = const_cast<char *>(topic.c_str());
	length = MQTTSNSerialize_register(buffer, MAX_PACKET_SIZE, 0, mPacketId++, &topicName);
	if (length <= 0) {
		error = OT_ERROR_FAILED;
		goto exit;
	}
	SuccessOrExit(error = SendMessage(buffer, length));

exit:
	return error;
}

otError MqttsnClient::Publish(const uint8_t* data, int32_t lenght, Qos qos, TopicId topicId) {
	otError error = OT_ERROR_NONE;
	int32_t length = -1;
	unsigned char buffer[MAX_PACKET_SIZE];

	if (!mIsConnected) {
		error = OT_ERROR_INVALID_STATE;
		goto exit;
	}

	// TODO: Implement QoS
	if (qos != Qos::MQTTSN_QOS0) {
		error = OT_ERROR_NOT_IMPLEMENTED;
		goto exit;
	}

	MQTTSN_topicid topic;
	topic.type = MQTTSN_TOPIC_TYPE_NORMAL;
	topic.data.id = static_cast<unsigned short>(topicId);
	length = MQTTSNSerialize_publish(buffer, MAX_PACKET_SIZE, 0, static_cast<int>(qos), 0,
			mPacketId++, topic, const_cast<unsigned char *>(data), lenght);
	if (length <= 0) {
		error = OT_ERROR_FAILED;
		goto exit;
	}
	SuccessOrExit(error = SendMessage(buffer, length));

exit:
	return error;
}

otError MqttsnClient::Unsubscribe(TopicId topicId) {
	otError error = OT_ERROR_NONE;
	int32_t length = -1;
	unsigned char buffer[MAX_PACKET_SIZE];

	if (!mIsConnected) {
		error = OT_ERROR_INVALID_STATE;
		goto exit;
	}

	MQTTSN_topicid topic;
	topic.type = MQTTSN_TOPIC_TYPE_NORMAL;
	topic.data.id = static_cast<unsigned short>(topicId);
	length = MQTTSNSerialize_unsubscribe(buffer, MAX_PACKET_SIZE, mPacketId++, &topic);
	if (length <= 0) {
		error = OT_ERROR_FAILED;
		goto exit;
	}
	SuccessOrExit(error = SendMessage(buffer, length));

exit:
	return error;
}

otError MqttsnClient::Disconnect() {
	otError error = OT_ERROR_NONE;
	int32_t length = -1;
	unsigned char buffer[MAX_PACKET_SIZE];

	if (!mIsConnected) {
		error = OT_ERROR_INVALID_STATE;
		goto exit;
	}

	length = MQTTSNSerialize_disconnect(buffer, MAX_PACKET_SIZE, 0);
	if (length <= 0) {
		error = OT_ERROR_FAILED;
		goto exit;
	}
	SuccessOrExit(error = SendMessage(buffer, length));

exit:
	return error;
}

otError MqttsnClient::Sleep() {
	// TODO: Implement sleep
	return OT_ERROR_NOT_IMPLEMENTED;
}

otError MqttsnClient::SetConnectedCallback(ConnectCallbackFunc callback, void* context) {
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

otError MqttsnClient::SetPublishedCallback(PublishedCallbackFunc callback, void* context) {
	mPublishedCallback = callback;
	mPublishedContext = context;
	return OT_ERROR_NONE;
}

otError MqttsnClient::SetUnsubscribedCallback(UnsubscribedCallbackFunc callback, void* context) {
	mUnsubscribedCallback = callback;
	mUnsubscribedContext = context;
	return OT_ERROR_NONE;
}

otError MqttsnClient::SetDisconnectedCallback(DisconnectedCallbackFunc callback, void* context) {
	mDisconnectedCallback = callback;
	mDisconnectedContext = context;
	return OT_ERROR_NONE;
}

otError MqttsnClient::SendMessage(unsigned char* buffer, int32_t length) {
	return SendMessage(buffer, length, mConfig.GetAddress(), mConfig.GetPort());
}

otError MqttsnClient::SendMessage(unsigned char* buffer, int32_t length, const Ip6::Address &address, uint16_t port) {
	otError error = OT_ERROR_NONE;
	Ip6::MessageInfo messageInfo;
	Message *message = nullptr;

	VerifyOrExit((message = mSocket.NewMessage(0)) != nullptr,
            error = OT_ERROR_NO_BUFS);
	message->Append(buffer, length);

	messageInfo.SetPeerAddr(address);
	messageInfo.SetPeerPort(port);
	messageInfo.SetInterfaceId(OT_NETIF_INTERFACE_ID_THREAD);

	PRINTF("Sending message to %s[:%u]\r\n", messageInfo.GetPeerAddr().ToString().AsCString(), messageInfo.GetPeerPort());
	SuccessOrExit(error = mSocket.SendTo(*message, messageInfo));

exit:
	if (error != OT_ERROR_NONE && message != NULL) {
        message->Free();
    }
	return error;
}

}
}
