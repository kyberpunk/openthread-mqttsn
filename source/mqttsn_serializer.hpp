/*
 *  Copyright (c) 2018, Vit Holasek
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MQTTSN_SERIALIZER_HPP_
#define MQTTSN_SERIALIZER_HPP_

#include <stdint.h>

#include "net/ip6_address.hpp"
#include "mqttsn_client.hpp"

namespace ot {

namespace Mqttsn {

enum MessageType
{
    kTypeAdvertise,
    kTypeSearchGw,
    kTypeGwInfo,
    kTypeReserved1,
    kTypeConnect,
    kTypeConnack,
    kTypeWillTopicReq,
    kTypeWillTopic,
    kTypeWillMsqReq,
    kTypeWillMsg,
    kTypeRegister,
    kTypeRegack,
    kTypePublish,
    kTypePuback,
    kTypePubcomp,
    kTypePubrec,
    kTypePubrel,
    kTypeReserved2,
    kTypeSubscribe,
    kTypeSuback,
    kTypeUnsubscribe,
    kTypeUnsuback,
    kTypePinreq,
    kTypePingresp,
    kTypeDisconnect,
    kTypeReserved3,
    kTypeWillTopicUpd,
    kTypeWillTopicResp,
    kTypeWillMsqUpd,
    kTypeWillMsgResp,
    kTypeEncapsulated = 0xfe
};

class MessageBase
{
public:
    MessageBase(MessageType aMessageType)
        : mMessageType(mMessageType)
    {
        ;
    }

    MessageType GetMessageType() { return mMessageType; };

    void SetMessageType(MessageType aMessageType) { mMessageType = aMessageType; };

    virtual otError Serialize(uint8_t* aBuffer, uint8_t aBufferLength, int32_t* aLength) const = 0;

    virtual otError Deserialize(uint8_t* aBuffer, int32_t aBufferLength) = 0;

    static MessageType DeserializeMessageType(uint8_t* aBuffer, int32_t aBufferLength);

private:
    MessageType mMessageType;
};

class AdvertiseMessage : public MessageBase
{
    AdvertiseMessage()
        : MessageBase(kTypeAdvertise)
    {
        ;
    }

    AdvertiseMessage (uint8_t aGatewayId, uint16_t aDuration)
        : AdvertiseMessage()
        , mGatewayId(aGatewayId)
        , mDuration(aDuration)
    {
        ;
    }

    uint8_t GetGatewayId() const { return mGatewayId; }

    void SetGatewayId(uint8_t aGatewayId) { mGatewayId = aGatewayId; }

    uint16_t GetDuration() const { return mDuration; }

    void SetDuration(uint16_t aDuration) { mDuration = aDuration; }

    otError Serialize(uint8_t* aBuffer, uint8_t aBufferLength, int32_t* aLength) const;

    otError Deserialize(uint8_t* aBuffer, int32_t aBufferLength);

private:
    uint8_t mGatewayId;
    uint16_t mDuration;
};

class SearchGwMessage : public MessageBase
{
    SearchGwMessage()
        : MessageBase(kTypeSearchGw)
    {
        ;
    }

    SearchGwMessage (uint8_t aRadius)
        : SearchGwMessage()
        , mRadius(aRadius)
    {
        ;
    }

    uint8_t GetRadius() const { return mRadius; }

    void SetRadius(uint8_t aRadius) { mRadius = aRadius; }

    otError Serialize(uint8_t* aBuffer, uint8_t aBufferLength, int32_t* aLength) const;

    otError Deserialize(uint8_t* aBuffer, int32_t aBufferLength);

private:
    uint8_t mRadius;
};

class GwInfoMessage : public MessageBase
{
    GwInfoMessage()
        : MessageBase(kTypeGwInfo)
    {
        ;
    }

    GwInfoMessage (uint8_t aGatewayId, const Ip6::Address &aAddress, int32_t aAddressLength)
        : GwInfoMessage()
        , mGatewayId(aGatewayId)
        , mAddress(aAddress)
        , mAddressLength(aAddressLength)
    {
        ;
    }

    uint8_t GetGatewayId() const { return mGatewayId; }

    void SetGatewayId(uint8_t aGatewayId) { mGatewayId = aGatewayId; }

    Ip6::Address GetAddress() const { return mAddress; }

    void SetAddress(const Ip6::Address &aAddress) { mAddress = aAddress; }

    int32_t GetAddressLength() const { return mAddressLength; }

    void SetAddressLength(int32_t aAddressLength) { mAddressLength = aAddressLength; }

    otError Serialize(uint8_t* aBuffer, uint8_t aBufferLength, int32_t* aLength) const;

    otError Deserialize(uint8_t* aBuffer, int32_t aBufferLength);

private:
    uint8_t mGatewayId;
    Ip6::Address mAddress;
    int32_t mAddressLength;
};

class ConnectMessage : public MessageBase
{
    ConnectMessage()
        : MessageBase(kTypeConnect)
    {
        ;
    }

    ConnectMessage (bool aCleanSessionFlag, bool aWillFlag, uint16_t aDuration, const char* aClientId)
        : ConnectMessage()
        , mCleanSessionFlag(aCleanSessionFlag)
        , mWillFlag(aWillFlag)
        , mDuration(aDuration)
        , mClientId("%s", aClientId)
    {
        ;
    }

    bool GetCleanSessionFlag() const { return mCleanSessionFlag; }

    void SetCleanSessionFlag(bool aCleanSessionFlag) { mCleanSessionFlag = aCleanSessionFlag; }

    bool GetWillFlag() const { return mWillFlag; }

    void SetWillFlag(bool aWillFlag) { mWillFlag = aWillFlag; }

    uint16_t GetDuration() const { return mDuration; }

    void SetDuration(uint16_t aDuration) { mDuration = aDuration; }

    const ClientIdString &GetClientId() const { return mClientId; }

    void SetClientId(const char* aClientId) { mClientId.Set("%s", aClientId); }

    otError Serialize(uint8_t* aBuffer, uint8_t aBufferLength, int32_t* aLength) const;

    otError Deserialize(uint8_t* aBuffer, int32_t aBufferLength);

private:
    bool mCleanSessionFlag;
    bool mWillFlag;
    uint16_t mDuration;
    ClientIdString mClientId;
};

class ConnackMessage : public MessageBase
{
    ConnackMessage()
        : MessageBase(kTypeConnack)
    {
        ;
    }

    ConnackMessage (ReturnCode aReturnCode)
        : ConnackMessage()
        , mReturnCode(aReturnCode)
    {
        ;
    }

    bool GetReturnCode() const { return mReturnCode; }

    void SetReturnCode(bool aReturnCode) { mReturnCode = aReturnCode; }

    otError Serialize(uint8_t* aBuffer, uint8_t aBufferLength, int32_t* aLength) const;

    otError Deserialize(uint8_t* aBuffer, int32_t aBufferLength);

private:
    ReturnCode mReturnCode;
};

class RegisterMessage : public MessageBase
{
    RegisterMessage()
        : MessageBase(kTypeRegister)
    {
        ;
    }

    RegisterMessage (TopicId aTopicId, uint16_t aMessageId, const char* aTopicName)
        : RegisterMessage()
        , mTopicId(aTopicId)
        , mMessageId(aMessageId)
        , mTopicName("%s", aTopicName)
    {
        ;
    }

    TopicId GetTopicId() const { return mTopicId; }

    void SetTopicId(TopicId aTopicId) { mTopicId = aTopicId; }

    uint16_t GetMessageId() const { return mMessageId; }

    void SetMessageId(uint16_t aMessageId) { mMessageId = aMessageId; }

    const TopicNameString &GetTopicName() const { return mTopicName; }

    void SetTopicName(const char* aTopicName) { mTopicName.Set("%s", aTopicName); }

    otError Serialize(uint8_t* aBuffer, uint8_t aBufferLength, int32_t* aLength) const;

    otError Deserialize(uint8_t* aBuffer, int32_t aBufferLength);

private:
    TopicId mTopicId;
    uint16_t mMessageId;
    TopicNameString mTopicName;
};

class RegackMessage : public MessageBase
{
    RegackMessage()
        : MessageBase(kTypeRegack)
    {
        ;
    }

    RegackMessage (ReturnCode aReturnCode)
        : RegackMessage()
        , mReturnCode(aReturnCode)
        , mTopicId(aTopicId)
        , mMessageId(aMessageId)
    {
        ;
    }

    bool GetReturnCode() const { return mReturnCode; }

    void SetReturnCode(bool aReturnCode) { mReturnCode = aReturnCode; }

    TopicId GetTopicId() const { return mTopicId; }

    void SetTopicId(TopicId aTopicId) { mTopicId = aTopicId; }

    uint16_t GetMessageId() const { return mMessageId; }

    void SetMessageId(uint16_t aMessageId) { mMessageId = aMessageId; }

    otError Serialize(uint8_t* aBuffer, uint8_t aBufferLength, int32_t* aLength) const;

    otError Deserialize(uint8_t* aBuffer, int32_t aBufferLength);

private:
    ReturnCode mReturnCode;
    TopicId mTopicId;
    uint16_t mMessageId;
};

class PublishMessage : public MessageBase
{
    PublishMessage()
        : MessageBase(kTypePublish)
    {
        ;
    }

    PublishMessage(bool aDupFlag, bool aRetainedFlag, Qos aQos, uint16_t aMessageId, TopicId aTopicId, const uint8_t* aPayload, int32_t aPayloadLength)
        : PublishMessage()
        , mDupFlag(aDupFlag)
        , mRetainedFlag(aRetainedFlag)
        , mQos(aQos)
        , mMessageId(aMessageId)
        , mTopicId(aTopicId)
        , mPayload(aPayload)
        , mPayloadLength(aPayloadLength)
    {
        ;
    }

    bool GetDupFlag() const { return mDupFlag; }

    void SetDupFlag(bool aDupFlag) { mDupFlag = aDupFlag; }

    bool GetRetainedFlag() { return mRetainedFlag; }

    void SetRetainedFlag(bool aRetainedFlag) { mRetainedFlag = aRetainedFlag; }

    uint16_t GetMessageId() const { return mMessageId; }

    void SetMessageId(uint16_t aMessageId) { mMessageId = aMessageId; }

    const uint8_t* GetPayload() const { return mPayload; }

    void SetPayload(const uint8_t aPayload) { mPayload = aPayload; }

    int32_t GetPayloadLength() { return mPayloadLength; }

    void SetPayloadLenghth(int32_t aPayloadLenght) { mPayloadLength = aPayloadLenght; }

    otError Serialize(uint8_t* aBuffer, uint8_t aBufferLength, int32_t* aLength) const;

    otError Deserialize(uint8_t* aBuffer, int32_t aBufferLength);

private:
    bool mDupFlag;
    bool mRetainedFlag;
    Qos mQos;
    uint16_t mMessageId;
    TopicId mTopicId;
    const uint8_t* mPayload;
    int32_t mPayloadLength;
};

class PubackMessage : public MessageBase
{
    PubackMessage()
        : MessageBase(kTypePuback)
    {
        ;
    }

    PubackMessage (ReturnCode aReturnCode)
        : PubackMessage()
        , mReturnCode(aReturnCode)
        , mTopicId(aTopicId)
        , mMessageId(aMessageId)
    {
        ;
    }

    bool GetReturnCode() const { return mReturnCode; }

    void SetReturnCode(bool aReturnCode) { mReturnCode = aReturnCode; }

    TopicId GetTopicId() const { return mTopicId; }

    void SetTopicId(TopicId aTopicId) { mTopicId = aTopicId; }

    uint16_t GetMessageId() const { return mMessageId; }

    void SetMessageId(uint16_t aMessageId) { mMessageId = aMessageId; }

    otError Serialize(uint8_t* aBuffer, uint8_t aBufferLength, int32_t* aLength) const;

    otError Deserialize(uint8_t* aBuffer, int32_t aBufferLength);

private:
    ReturnCode mReturnCode;
    TopicId mTopicId;
    uint16_t mMessageId;
};

class PubcompMessage : public MessageBase
{
    PubcompMessage()
        : MessageBase(kTypePubcomp)
    {
        ;
    }

    PubcompMessage (ReturnCode aReturnCode)
        : PubcompMessage()
        , mMessageId(aMessageId)
    {
        ;
    }

    uint16_t GetMessageId() const { return mMessageId; }

    void SetMessageId(uint16_t aMessageId) { mMessageId = aMessageId; }

    otError Serialize(uint8_t* aBuffer, uint8_t aBufferLength, int32_t* aLength) const;

    otError Deserialize(uint8_t* aBuffer, int32_t aBufferLength);

private:
    uint16_t mMessageId;
};

class PubrecMessage : public MessageBase
{
    PubrecMessage()
        : MessageBase(kTypePubrec)
    {
        ;
    }

    PubrecMessage (ReturnCode aReturnCode)
        : PubrecMessage()
        , mMessageId(aMessageId)
    {
        ;
    }

    uint16_t GetMessageId() const { return mMessageId; }

    void SetMessageId(uint16_t aMessageId) { mMessageId = aMessageId; }

    otError Serialize(uint8_t* aBuffer, uint8_t aBufferLength, int32_t* aLength) const;

    otError Deserialize(uint8_t* aBuffer, int32_t aBufferLength);

private:
    uint16_t mMessageId;
};

class PubrelMessage : public MessageBase
{
    PubrelMessage()
        : MessageBase(kTypePubrel)
    {
        ;
    }

    PubrelMessage (ReturnCode aReturnCode)
        : PubrelMessage()
        , mMessageId(aMessageId)
    {
        ;
    }

    uint16_t GetMessageId() const { return mMessageId; }

    void SetMessageId(uint16_t aMessageId) { mMessageId = aMessageId; }

    otError Serialize(uint8_t* aBuffer, uint8_t aBufferLength, int32_t* aLength) const;

    otError Deserialize(uint8_t* aBuffer, int32_t aBufferLength);

private:
    uint16_t mMessageId;
};

class SubscribeMessage : public MessageBase
{
    SubscribeMessage()
        : MessageBase(kTypeSubscribe)
    {
        ;
    }

    SubscribeMessage(bool aDupFlag, bool aRetainedFlag, Qos aQos, uint16_t aMessageId, TopicId aTopicId, const uint8_t* aPayload, int32_t aPayloadLength)
        : SubscribeMessage()
        , mDupFlag(aDupFlag)
        , mQos(aQos)
        , mMessageId(aMessageId)
        , mTopicName("%s", aTopicName)
    {
        ;
    }

    bool GetDupFlag() const { return mDupFlag; }

    void SetDupFlag(bool aDupFlag) { mDupFlag = aDupFlag; }

    uint16_t GetMessageId() const { return mMessageId; }

    void SetMessageId(uint16_t aMessageId) { mMessageId = aMessageId; }

    const TopicNameString &GetTopicName() const { return mTopicName; }

    void SetTopicName(const char* aTopicName) { mTopicName.Set("%s", aTopicName); }

    otError Serialize(uint8_t* aBuffer, uint8_t aBufferLength, int32_t* aLength) const;

    otError Deserialize(uint8_t* aBuffer, int32_t aBufferLength);

private:
    bool mDupFlag;
    Qos mQos;
    uint16_t mMessageId;
    TopicNameString mTopicName;
};

class SubackMessage : public MessageBase
{
    SubackMessage()
        : MessageBase(kTypeSuback)
    {
        ;
    }

    SubackMessage (ReturnCode aReturnCode)
        : SubackMessage()
        , mReturnCode(aReturnCode)
        , mTopicId(aTopicId)
        , mMessageId(aMessageId)
    {
        ;
    }

    bool GetReturnCode() const { return mReturnCode; }

    void SetReturnCode(bool aReturnCode) { mReturnCode = aReturnCode; }

    TopicId GetTopicId() const { return mTopicId; }

    void SetTopicId(TopicId aTopicId) { mTopicId = aTopicId; }

    uint16_t GetMessageId() const { return mMessageId; }

    void SetMessageId(uint16_t aMessageId) { mMessageId = aMessageId; }

    otError Serialize(uint8_t* aBuffer, uint8_t aBufferLength, int32_t* aLength) const;

    otError Deserialize(uint8_t* aBuffer, int32_t aBufferLength);

private:
    ReturnCode mReturnCode;
    TopicId mTopicId;
    uint16_t mMessageId;
};

class UnsubscribeMessage : public MessageBase
{
    UnsubscribeMessage()
        : MessageBase(kTypeUnsubscribe)
    {
        ;
    }

    UnsubscribeMessage (ReturnCode aReturnCode)
        : UnsubscribeMessage()
        , mTopicId(aTopicId)
        , mMessageId(aMessageId)
    {
        ;
    }

    bool GetReturnCode() const { return mReturnCode; }

    void SetReturnCode(bool aReturnCode) { mReturnCode = aReturnCode; }

    TopicId GetTopicId() const { return mTopicId; }

    void SetTopicId(TopicId aTopicId) { mTopicId = aTopicId; }

    uint16_t GetMessageId() const { return mMessageId; }

    void SetMessageId(uint16_t aMessageId) { mMessageId = aMessageId; }

    otError Serialize(uint8_t* aBuffer, uint8_t aBufferLength, int32_t* aLength) const;

    otError Deserialize(uint8_t* aBuffer, int32_t aBufferLength);

private:
    TopicId mTopicId;
    uint16_t mMessageId;
};

class UnsubackMessage : public MessageBase
{
    UnsubackMessage()
        : MessageBase(kTypeUnsuback)
    {
        ;
    }

    UnsubackMessage (ReturnCode aReturnCode)
        : UnsubackMessage()
        , mMessageId(aMessageId)
    {
        ;
    }

    uint16_t GetMessageId() const { return mMessageId; }

    void SetMessageId(uint16_t aMessageId) { mMessageId = aMessageId; }

    otError Serialize(uint8_t* aBuffer, uint8_t aBufferLength, int32_t* aLength) const;

    otError Deserialize(uint8_t* aBuffer, int32_t aBufferLength);

private:
    uint16_t mMessageId;
};

class PingreqMessage : public MessageBase
{
    PingreqMessage()
        : MessageBase(kTypePingreq)
    {
        ;
    }

    PingreqMessage (const char* aClientId)
        : PingreqMessage()
        , mClientId("%s", aClientId)
    {
        ;
    }

    const ClientIdString &GetClientId() const { return mClientId; }

    void SetClientId(const char* aClientId) { mClientId.Set("%s", aClientId); }

    otError Serialize(uint8_t* aBuffer, uint8_t aBufferLength, int32_t* aLength) const;

    otError Deserialize(uint8_t* aBuffer, int32_t aBufferLength);

private:
    ClientIdString mClientId;
};

class PingrespMessage : public MessageBase
{
    PingrespMessage()
        : MessageBase(kTypePingresp)
    {
        ;
    }

    PingrespMessage ()
        : PingrespMessage()
    {
        ;
    }

    otError Serialize(uint8_t* aBuffer, uint8_t aBufferLength, int32_t* aLength) const;

    otError Deserialize(uint8_t* aBuffer, int32_t aBufferLength);
};

class DisconnectMessage : public MessageBase
{
    DisconnectMessage()
        : MessageBase(kTypeDisconnect)
    {
        ;
    }

    DisconnectMessage (uint16_t aDuration)
        : DisconnectMessage()
        , mDuration(aDuration)
    {
        ;
    }

    uint16_t GetDuration() const { return mDuration; }

    void SetDuration(uint16_t aDuration) { mDuration = aDuration; }

    otError Serialize(uint8_t* aBuffer, uint8_t aBufferLength, int32_t* aLength) const;

    otError Deserialize(uint8_t* aBuffer, int32_t aBufferLength);

private:
    uint16_t mDuration;
};

}

}

#endif /* MQTTSN_SERIALIZER_HPP_ */
