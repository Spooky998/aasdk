#include <aasdk/IO/PromiseLink.hpp>
#include <aasdk/Messenger/MessageOutStream.hpp>


namespace aasdk
{
namespace messenger
{

MessageOutStream::MessageOutStream(asio::io_service& ioService, transport::ITransport::Pointer transport, ICryptor::Pointer cryptor)
    : strand_(ioService)
    , transport_(std::move(transport))
    , cryptor_(std::move(cryptor))
    , offset_(0)
    , remainingSize_(0)
{

}

void MessageOutStream::stream(Message::Pointer message, SendPromise::Pointer promise)
{
    strand_.dispatch([this, self = this->shared_from_this(), message = std::move(message), promise = std::move(promise)]() mutable {
        if(promise_ != nullptr)
        {
            promise->reject(error::Error(error::ErrorCode::OPERATION_IN_PROGRESS));
            return;
        }

        message_ = std::move(message);
        promise_ = std::move(promise);

        if(message_->getPayload().size() >= cMaxFramePayloadSize)
        {
            offset_ = 0;
            remainingSize_ = message_->getPayload().size();
            this->streamSplittedMessage();
        }
        else
        {
            try
            {
                auto data(this->compoundFrame(FrameType::BULK, common::DataConstBuffer(message_->getPayload())));

                auto transportPromise = transport::ITransport::SendPromise::defer(strand_);
                io::PromiseLink<>::forward(*transportPromise, std::move(promise_));
                transport_->send(std::move(data), std::move(transportPromise));
            }
            catch(const error::Error& e)
            {
                promise_->reject(e);
                promise_.reset();
            }

            this->reset();
        }
    });
}

void MessageOutStream::streamSplittedMessage()
{
    try
    {
        const auto& payload = message_->getPayload();
        auto ptr = &payload[offset_];
        auto size = remainingSize_ < cMaxFramePayloadSize ? remainingSize_ : cMaxFramePayloadSize;

        FrameType frameType = offset_ == 0 ? FrameType::FIRST : (remainingSize_ - size > 0 ? FrameType::MIDDLE : FrameType::LAST);
        auto data(this->compoundFrame(frameType, common::DataConstBuffer(ptr, size)));

        auto transportPromise = transport::ITransport::SendPromise::defer(strand_);

        if(frameType == FrameType::LAST)
        {
            this->reset();
            io::PromiseLink<>::forward(*transportPromise, std::move(promise_));
        }
        else
        {
            transportPromise->then([this, self = this->shared_from_this(), size]() mutable {
                    offset_ += size;
                    remainingSize_ -= size;
                    this->streamSplittedMessage();
                },
                [this, self = this->shared_from_this()](const error::Error& e) mutable {
                    this->reset();
                    promise_->reject(e);
                    promise_.reset();
                });
        }

        transport_->send(std::move(data), std::move(transportPromise));
    }
    catch(const error::Error& e)
    {
        this->reset();
        promise_->reject(e);
        promise_.reset();
    }
}

common::Data MessageOutStream::compoundFrame(FrameType frameType, const common::DataConstBuffer& payloadBuffer)
{
    const FrameHeader frameHeader(message_->getChannelId(), frameType, message_->getEncryptionType(), message_->getType());
    common::Data data(frameHeader.getData());
    data.resize(data.size() + FrameSize::getSizeOf(frameType == FrameType::FIRST ? FrameSizeType::EXTENDED : FrameSizeType::SHORT));
    size_t payloadSize = 0;

    if(message_->getEncryptionType() == EncryptionType::ENCRYPTED)
    {
        payloadSize = cryptor_->encrypt(data, payloadBuffer);
    }
    else
    {
        data.insert(data.end(), payloadBuffer.cdata, payloadBuffer.cdata + payloadBuffer.size);
        payloadSize = payloadBuffer.size;
    }

    this->setFrameSize(data, frameType, payloadSize, message_->getPayload().size());
    return data;
}

void MessageOutStream::setFrameSize(common::Data& data, FrameType frameType, size_t payloadSize, size_t totalSize)
{
    const auto& frameSize = frameType == FrameType::FIRST ? FrameSize(payloadSize, totalSize) : FrameSize(payloadSize);
    const auto& frameSizeData = frameSize.getData();
    memcpy(&data[FrameHeader::getSizeOf()], &frameSizeData[0], frameSizeData.size());
}

void MessageOutStream::reset()
{
    offset_ = 0;
    remainingSize_ = 0;
    message_.reset();
}

}
}
