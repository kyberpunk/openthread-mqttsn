#include <cstring>

#include "mqttsn_client.hpp"
#include "MQTTSNPacket.h"
#include "MQTTSNConnect.h"
#include "MQTTSNSearch.h"
#include "fsl_debug_console.h"

#define MAX_PACKET_SIZE 255
#define KEEP_ALIVE_DELAY 5
#define MQTTSN_MIN_PACKET_LENGTH 2

namespace ot {

namespace Mqttsn {
// TODO: Implement QoS and DUP behavior
// TODO: Implement OT logging
// TODO: Implement timeouts

MqttsnClient::MqttsnClient(Instance& instance)
    : InstanceLocator(instance)
    , mSocket(GetInstance().GetThreadNetif().GetIp6().GetUdp())
    , mConfig()
    , mPacketId(1)
    , mPingReqTime(0)
    , mGwTimeout(0)
    , mDisconnectRequested(false)
    , mSleepRequested(false)
    , mClientState(MQTTSN_STATE_DISCONNECTED)
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
    , mDisconnectedContext(nullptr)
{
    ;
}

static int32_t PacketDecode(unsigned char* aData, size_t aLength)
{
    int lenlen = 0;
    int datalen = 0;

    if (aLength < MQTTSN_MIN_PACKET_LENGTH)
    {
        return MQTTSNPACKET_READ_ERROR;
    }

    lenlen = MQTTSNPacket_decode(aData, aLength, &datalen);
    if (datalen != static_cast<int>(aLength))
    {
        return MQTTSNPACKET_READ_ERROR;
    }
    return aData[lenlen]; // return the packet type
}

// TODO: Verify sender address
void MqttsnClient::HandleUdpReceive(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    Message &message = *static_cast<Message *>(aMessage);
    const Ip6::MessageInfo &messageInfo = *static_cast<const Ip6::MessageInfo *>(aMessageInfo);

    uint16_t offset = message.GetOffset();
    uint16_t length = message.GetLength() - message.GetOffset();

    unsigned char data[MAX_PACKET_SIZE];

    if (length > MAX_PACKET_SIZE)
    {
        return;
    }

    if (!data)
    {
        return;
    }
    message.Read(offset, length, data);

    // Dump packet
    PRINTF("UDP message received:\r\n");
    for (int32_t i = 0; i < length; i++)
    {
        if (i > 0)
        {
            PRINTF(" ");
        }
        PRINTF("%02X", data[i]);
    }
    PRINTF("\r\n");

    int32_t decodedPacketType = PacketDecode(data, length);
    if (decodedPacketType == MQTTSNPACKET_READ_ERROR)
    {
        goto error;
    }
    PRINTF("Packet type: %d\r\n", decodedPacketType);

    // TODO: Refactor switch to use separate handle functions
    switch (decodedPacketType)
    {
    case MQTTSN_CONNACK:
        {
        int connectReturnCode = 0;
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }
        if (MQTTSNDeserialize_connack(&connectReturnCode, data, length) != 1)
        {
            break;
        }
        client->mClientState = MQTTSN_STATE_ACTIVE;
        client->mGwTimeout = 0;
        if (client->mConnectCallback)
        {
            client->mConnectCallback(static_cast<ReturnCode>(connectReturnCode),
                client->mConnectContext);
        }
    }
        break;
    case MQTTSN_SUBACK:
        {
        if (client->mClientState != MQTTSN_STATE_ACTIVE)
        {
            break;
        }
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }
        int qos;
        unsigned short topicId = 0;
        unsigned short packetId = 0;
        unsigned char subscribeReturnCode = 0;
        if (MQTTSNDeserialize_suback(&qos, &topicId, &packetId, &subscribeReturnCode,
            data, length) != 1)
        {
            break;
        }
        if (client->mSubscribeCallback)
        {
            client->mSubscribeCallback(static_cast<ReturnCode>(subscribeReturnCode),
                static_cast<TopicId>(topicId), client->mSubscribeContext);
        }
    }
        break;
    case MQTTSN_PUBLISH:
        {
        if (client->mClientState != MQTTSN_STATE_ACTIVE && client->mClientState != MQTTSN_STATE_AWAKE)
        {
            break;
        }
        if (!client->VerifyGatewayAddress(messageInfo))
        {
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
            &topicId, &payload, &payloadLength, data, length) != 1)
        {
            break;
        }
        if (client->mPublishReceivedCallback)
        {
            client->mPublishReceivedCallback(payload, payloadLength, static_cast<Qos>(qos),
                static_cast<TopicId>(topicId.data.id), client->mPublishReceivedContext);
        }
    }
        break;
    case MQTTSN_ADVERTISE:
        {
        unsigned char gatewayId = 0;
        unsigned short duration = 0;
        if (MQTTSNDeserialize_advertise(&gatewayId, &duration, data, length) != 1)
        {
            break;
        }
        if (client->mAdvertiseCallback)
        {
            client->mAdvertiseCallback(messageInfo.GetPeerAddr(), messageInfo.GetPeerPort(),
                static_cast<uint8_t>(gatewayId), static_cast<uint32_t>(duration), client->mAdvertiseContext);
        }
    }
        break;
    case MQTTSN_GWINFO:
        {
        unsigned char gatewayId = 0;
        unsigned short addressLength = 1;
        unsigned char* addressText = nullptr;
        if (MQTTSNDeserialize_gwinfo(&gatewayId, &addressLength, &addressText, data, length) != 1)
        {
            break;
        }
        if (client->mSearchGwCallback)
        {
            if (addressLength > 0)
            {
                // client response not supported
                break;
            }
            Ip6::Address address = messageInfo.GetPeerAddr();
            uint16_t port = messageInfo.GetPeerPort();
            client->mSearchGwCallback(address, port, gatewayId, client->mSearchGwContext);
        }
    }
        break;
    case MQTTSN_REGACK:
        {
        if (client->mClientState != MQTTSN_STATE_ACTIVE)
        {
            break;
        }
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }

        unsigned short topicId;
        unsigned short packetId;
        unsigned char returnCode;
        if (MQTTSNDeserialize_regack(&topicId, &packetId, &returnCode, data, length) != 1)
        {
            break;
        }
        if (client->mRegisterCallback)
        {
            client->mRegisterCallback(static_cast<ReturnCode>(returnCode), static_cast<TopicId>(topicId),
                client->mRegisterContext);
        }
    }
        break;
    case MQTTSN_PUBACK:
        {
        if (client->mClientState != MQTTSN_STATE_ACTIVE)
        {
            break;
        }
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }

        unsigned short topicId;
        unsigned short packetId;
        unsigned char returnCode;
        if (MQTTSNDeserialize_puback(&topicId, &packetId, &returnCode, data, length) != 1)
        {
            break;
        }
        if (client->mPublishedCallback)
        {
            client->mPublishedCallback(static_cast<ReturnCode>(returnCode), static_cast<TopicId>(topicId),
                client->mPublishedContext);
        }
    }
        break;
    case MQTTSN_UNSUBACK:
        {
        if (client->mClientState != MQTTSN_STATE_ACTIVE)
        {
            break;
        }
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }

        unsigned short packetId;
        if (MQTTSNDeserialize_unsuback(&packetId, data, length) != 1)
        {
            break;
        }
        if (client->mUnsubscribedCallback)
        {
            client->mUnsubscribedCallback(client->mUnsubscribedContext);
        }
    }
        break;
    case MQTTSN_PINGREQ:
        {
        MQTTSNString clientId;
        if (client->mClientState != MQTTSN_STATE_ACTIVE)
        {
            break;
        }

        if (MQTTSNDeserialize_pingreq(&clientId, data, length) != 1)
        {
            break;
        }

        int32_t packetLength = -1;
        unsigned char buffer[MAX_PACKET_SIZE];
        packetLength = MQTTSNSerialize_pingresp(buffer, MAX_PACKET_SIZE);
        if (packetLength <= 0)
        {
            break;
        }
        if (client->SendMessage(buffer, packetLength, messageInfo.GetPeerAddr(), messageInfo.GetPeerPort()) != OT_ERROR_NONE)
        {
            break;
        }
    }
        break;
    case MQTTSN_PINGRESP:
        {
        if (MQTTSNDeserialize_pingresp(data, length) != 1)
        {
            break;
        }
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }

        client->mGwTimeout = 0;
        if (client->mClientState == MQTTSN_STATE_AWAKE)
        {
            client->mClientState = MQTTSN_STATE_ASLEEP;
            if (client->mDisconnectedCallback)
            {
                client->mDisconnectedCallback(MQTTSN_DISCONNECT_ASLEEP, client->mDisconnectedContext);
            }
        }
    }
        break;
    case MQTTSN_DISCONNECT:
        {

        int duration;
        if (MQTTSNDeserialize_disconnect(&duration, data, length) != 1)
        {
            break;
        }
        client->OnDisconnected();

        DisconnectType reason = MQTTSN_DISCONNECT_SERVER;
        switch (client->mClientState)
        {
        case MQTTSN_STATE_ACTIVE:
            case MQTTSN_STATE_AWAKE:
            case MQTTSN_STATE_ASLEEP:
            if (client->mDisconnectRequested)
            {
                client->mClientState = MQTTSN_STATE_DISCONNECTED;
                reason = MQTTSN_DISCONNECT_SERVER;
            }
            else if (client->mSleepRequested)
            {
                client->mClientState = MQTTSN_STATE_ASLEEP;
                reason = MQTTSN_DISCONNECT_ASLEEP;
            }
            else
            {
                client->mClientState = MQTTSN_STATE_DISCONNECTED;
                reason = MQTTSN_DISCONNECT_SERVER;
            }
            break;
        default:
            break;
        }
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }

        if (client->mDisconnectedCallback)
        {
            client->mDisconnectedCallback(reason, client->mDisconnectedContext);
        }
    }
        break;
    default:
        break;
    }

error:
    delete[] data;
}

otError MqttsnClient::Start(uint16_t aPort)
{
    otError error = OT_ERROR_NONE;
    Ip6::SockAddr sockaddr;
    sockaddr.mPort = aPort;

    SuccessOrExit(error = mSocket.Open(MqttsnClient::HandleUdpReceive, this));
    SuccessOrExit(error = mSocket.Bind(sockaddr));

exit:
    return error;
}

otError MqttsnClient::Stop()
{
    return mSocket.Close();
    if (mClientState != MQTTSN_STATE_DISCONNECTED && mClientState != MQTTSN_STATE_LOST)
    {
        mClientState = MQTTSN_STATE_DISCONNECTED;
        OnDisconnected();
        if (mDisconnectedCallback)
        {
            mDisconnectedCallback(MQTTSN_DISCONNECT_CLIENT, mDisconnectedContext);
        }
    }
}

otError MqttsnClient::Process()
{
    otError error = OT_ERROR_NONE;

    uint32_t now = TimerMilli::GetNow();

    // Process pingreq
    if (mClientState != MQTTSN_STATE_ACTIVE && mPingReqTime != 0 && mPingReqTime <= now)
    {
        SuccessOrExit(error = PingGateway());
        mGwTimeout = TimerMilli::GetNow() + mConfig.GetGatewayTimeout() * 1000;
    }

    // Handle timeout
    if (mGwTimeout != 0 && mGwTimeout <= now)
    {
        OnDisconnected();
        mClientState = MQTTSN_STATE_LOST;
        if (mDisconnectedCallback)
        {
            mDisconnectedCallback(MQTTSN_DISCONNECT_TIMEOUT, mDisconnectedContext);
        }
    }

exit:
    return error;
}

otError MqttsnClient::Connect(MqttsnConfig &aConfig)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    MQTTSNPacket_connectData options = MQTTSNPacket_connectData_initializer;

    if (mClientState == MQTTSN_STATE_ACTIVE)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }
    mConfig = aConfig;

    MQTTSNString clientId;
    clientId.cstring = const_cast<char *>(aConfig.GetClientId().c_str());
    options.clientID = clientId;
    options.duration = aConfig.GetKeepAlive();
    options.cleansession = static_cast<unsigned char>(aConfig.GetCleanSession());

    unsigned char buffer[MAX_PACKET_SIZE];
    length = MQTTSNSerialize_connect(buffer, MAX_PACKET_SIZE, &options);
    if (length <= 0)
    {
        error = OT_ERROR_FAILED;
        goto exit;
    }
    SuccessOrExit(error = SendMessage(buffer, length));

    mDisconnectRequested = false;
    mSleepRequested = false;
    mGwTimeout = TimerMilli::GetNow() + mConfig.GetGatewayTimeout() * 1000;
    mPingReqTime = TimerMilli::GetNow() + mConfig.GetKeepAlive() * 1000;

exit:
    return error;
}

otError MqttsnClient::Subscribe(const std::string &aTopic, Qos aQos)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Ip6::MessageInfo messageInfo;
    MQTTSN_topicid topicIdConfig;
    unsigned char buffer[MAX_PACKET_SIZE];

    if (mClientState != MQTTSN_STATE_ACTIVE)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    // TODO: Implement QoS
    if (aQos != Qos::MQTTSN_QOS0)
    {
        error = OT_ERROR_NOT_IMPLEMENTED;
        goto exit;
    }

    topicIdConfig.type = MQTTSN_TOPIC_TYPE_NORMAL;
    topicIdConfig.data.long_.name = const_cast<char *>(aTopic.c_str());
    topicIdConfig.data.long_.len = aTopic.length();

    length = MQTTSNSerialize_subscribe(buffer, MAX_PACKET_SIZE, 0, static_cast<int>(aQos), mPacketId++, &topicIdConfig);
    if (length <= 0)
    {
        error = OT_ERROR_FAILED;
        goto exit;
    }
    SuccessOrExit(error = SendMessage(buffer, length));

exit:
    return error;
}

otError MqttsnClient::Register(const std::string &aTopic)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    MQTTSNString topicName;
    unsigned char buffer[MAX_PACKET_SIZE];

    if (mClientState != MQTTSN_STATE_ACTIVE)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    topicName.cstring = const_cast<char *>(aTopic.c_str());
    length = MQTTSNSerialize_register(buffer, MAX_PACKET_SIZE, 0, mPacketId++, &topicName);
    if (length <= 0)
    {
        error = OT_ERROR_FAILED;
        goto exit;
    }
    SuccessOrExit(error = SendMessage(buffer, length));

exit:
    return error;
}

otError MqttsnClient::Publish(const uint8_t* aData, int32_t lenght, Qos aQos, TopicId aTopicId)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    unsigned char buffer[MAX_PACKET_SIZE];

    if (mClientState != MQTTSN_STATE_ACTIVE)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    // TODO: Implement QoS
    if (aQos != Qos::MQTTSN_QOS0)
    {
        error = OT_ERROR_NOT_IMPLEMENTED;
        goto exit;
    }

    MQTTSN_topicid topic;
    topic.type = MQTTSN_TOPIC_TYPE_NORMAL;
    topic.data.id = static_cast<unsigned short>(aTopicId);
    length = MQTTSNSerialize_publish(buffer, MAX_PACKET_SIZE, 0, static_cast<int>(aQos), 0,
        mPacketId++, topic, const_cast<unsigned char *>(aData), lenght);
    if (length <= 0)
    {
        error = OT_ERROR_FAILED;
        goto exit;
    }
    SuccessOrExit(error = SendMessage(buffer, length));

exit:
    return error;
}

otError MqttsnClient::Unsubscribe(TopicId aTopicId)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    unsigned char buffer[MAX_PACKET_SIZE];

    if (mClientState != MQTTSN_STATE_ACTIVE)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    MQTTSN_topicid topic;
    topic.type = MQTTSN_TOPIC_TYPE_NORMAL;
    topic.data.id = static_cast<unsigned short>(aTopicId);
    length = MQTTSNSerialize_unsubscribe(buffer, MAX_PACKET_SIZE, mPacketId++, &topic);
    if (length <= 0)
    {
        error = OT_ERROR_FAILED;
        goto exit;
    }
    SuccessOrExit(error = SendMessage(buffer, length));

exit:
    return error;
}

otError MqttsnClient::Disconnect()
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    unsigned char buffer[MAX_PACKET_SIZE];

    if (mClientState != MQTTSN_STATE_ACTIVE && mClientState != MQTTSN_STATE_AWAKE && mClientState != MQTTSN_STATE_ASLEEP)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    length = MQTTSNSerialize_disconnect(buffer, MAX_PACKET_SIZE, 0);
    if (length <= 0)
    {
        error = OT_ERROR_FAILED;
        goto exit;
    }
    SuccessOrExit(error = SendMessage(buffer, length));

    mDisconnectRequested = true;
    mGwTimeout = TimerMilli::GetNow() + mConfig.GetGatewayTimeout() * 1000;

exit:
    return error;
}

otError MqttsnClient::Sleep(uint16_t aDuration)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    unsigned char buffer[MAX_PACKET_SIZE];

    if (mClientState != MQTTSN_STATE_ACTIVE && mClientState != MQTTSN_STATE_AWAKE && mClientState != MQTTSN_STATE_ASLEEP)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    length = MQTTSNSerialize_disconnect(buffer, MAX_PACKET_SIZE, aDuration);
    if (length <= 0)
    {
        error = OT_ERROR_FAILED;
        goto exit;
    }

    SuccessOrExit(error = SendMessage(buffer, length));

    mSleepRequested = true;
    mGwTimeout = TimerMilli::GetNow() + mConfig.GetGatewayTimeout() * 1000;

exit:
    return error;
}

otError MqttsnClient::Awake(uint32_t aTimeout)
{
    otError error = OT_ERROR_NONE;
    if (mClientState != MQTTSN_STATE_AWAKE && mClientState != MQTTSN_STATE_ASLEEP)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    SuccessOrExit(error = PingGateway());

    mClientState = MQTTSN_STATE_AWAKE;
    mGwTimeout = TimerMilli::GetNow() + aTimeout * 1000;
exit:
    return error;
}

otError MqttsnClient::SearchGateway(const Ip6::Address &aMulticastAddress, uint16_t aPort, uint8_t aRadius)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    unsigned char buffer[MAX_PACKET_SIZE];

    length = MQTTSNSerialize_searchgw(buffer, MAX_PACKET_SIZE, aRadius);
    if (length <= 0)
    {
        error = OT_ERROR_FAILED;
        goto exit;
    }

    SuccessOrExit(error = SendMessage(buffer, length, aMulticastAddress, aPort, aRadius));

exit:
    return error;
}

ClientState MqttsnClient::GetState(ClientState aState)
{
    return mClientState;
}

otError MqttsnClient::SetConnectedCallback(ConnectCallbackFunc aCallback, void* aContext
    )
{
    mConnectCallback = aCallback;
    mConnectContext = aContext;
    return OT_ERROR_NONE;
}

otError MqttsnClient::SetSubscribeCallback(SubscribeCallbackFunc aCallback, void* aContext)
{
    mSubscribeCallback = aCallback;
    mSubscribeContext = aContext;
    return OT_ERROR_NONE;
}

otError MqttsnClient::SetPublishReceivedCallback(PublishReceivedCallbackFunc aCallback, void* aContext)
{
    mPublishReceivedCallback = aCallback;
    mPublishReceivedContext = aContext;
    return OT_ERROR_NONE;
}

otError MqttsnClient::SetAdvertiseCallback(AdvertiseCallbackFunc aCallback, void* aContext)
{
    mAdvertiseCallback = aCallback;
    mAdvertiseContext = aContext;
    return OT_ERROR_NONE;
}

otError MqttsnClient::SetSearchGwCallback(SearchGwCallbackFunc aCallback, void* aContext)
{
    mSearchGwCallback = aCallback;
    mSearchGwContext = aContext;
    return OT_ERROR_NONE;
}

otError MqttsnClient::SetRegisterCallback(RegisterCallbackFunc aCallback, void* aContext)
{
    mRegisterCallback = aCallback;
    mRegisterContext = aContext;
    return OT_ERROR_NONE;
}

otError MqttsnClient::SetPublishedCallback(PublishedCallbackFunc aCallback, void* aContext)
{
    mPublishedCallback = aCallback;
    mPublishedContext = aContext;
    return OT_ERROR_NONE;
}

otError MqttsnClient::SetUnsubscribedCallback(UnsubscribedCallbackFunc aCallback, void* aContext)
{
    mUnsubscribedCallback = aCallback;
    mUnsubscribedContext = aContext;
    return OT_ERROR_NONE;
}

otError MqttsnClient::SetDisconnectedCallback(DisconnectedCallbackFunc aCallback, void* aContext)
{
    mDisconnectedCallback = aCallback;
    mDisconnectedContext = aContext;
    return OT_ERROR_NONE;
}

otError MqttsnClient::SendMessage(unsigned char* aBuffer, int32_t aLength)
{
    return SendMessage(aBuffer, aLength, mConfig.GetAddress(), mConfig.GetPort());
}

otError MqttsnClient::SendMessage(unsigned char* aBuffer, int32_t aLength, const Ip6::Address &aAddress, uint16_t aPort)
{
    SendMessage(aBuffer, aLength, aAddress, aPort, 0);
}

otError MqttsnClient::SendMessage(unsigned char* aBuffer, int32_t aLength, const Ip6::Address &aAddress, uint16_t aPort, uint8_t aHopLimit)
{
    otError error = OT_ERROR_NONE;
    Ip6::MessageInfo messageInfo;
    Message *message = nullptr;

    VerifyOrExit((message = mSocket.NewMessage(0)) != nullptr,
        error = OT_ERROR_NO_BUFS);
    message->Append(aBuffer, aLength);

    messageInfo.SetHopLimit(aHopLimit);
    messageInfo.SetPeerAddr(aAddress);
    messageInfo.SetPeerPort(aPort);
    messageInfo.SetInterfaceId(OT_NETIF_INTERFACE_ID_THREAD);

    PRINTF("Sending message to %s[:%u]\r\n", messageInfo.GetPeerAddr().ToString().AsCString(), messageInfo.GetPeerPort());
    SuccessOrExit(error = mSocket.SendTo(*message, messageInfo));

    if (mClientState == MQTTSN_STATE_ACTIVE)
    {
        mPingReqTime = TimerMilli::GetNow() + (mConfig.GetKeepAlive() - KEEP_ALIVE_DELAY) * 1000;
    }

exit:
    if (error != OT_ERROR_NONE && message != NULL)
    {
        message->Free();
    }
    return error;
}

otError MqttsnClient::PingGateway()
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    unsigned char buffer[MAX_PACKET_SIZE];

    if (mClientState != MQTTSN_STATE_ACTIVE && mClientState != MQTTSN_STATE_AWAKE)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    MQTTSNString clientId;
    clientId.cstring = const_cast<char *>(mConfig.GetClientId().c_str());
    length = MQTTSNSerialize_pingreq(buffer, MAX_PACKET_SIZE, clientId);

    if (length <= 0)
    {
        error = OT_ERROR_FAILED;
        goto exit;
    }
    SuccessOrExit(error = SendMessage(buffer, length));

exit:
    return error;
}

void MqttsnClient::OnDisconnected()
{
    mDisconnectRequested = false;
    mSleepRequested = false;
    mGwTimeout = 0;
    mPingReqTime = 0;
}

bool MqttsnClient::VerifyGatewayAddress(const Ip6::MessageInfo &aMessageInfo)
{
    return aMessageInfo.GetPeerAddr() == mConfig.GetAddress()
        && aMessageInfo.GetPeerPort() == mConfig.GetPort();
}

}
}
