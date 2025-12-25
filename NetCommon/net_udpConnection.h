#pragma once

#include "net_common.h"
#include "net_tsqueue.h"
#include "net_message.h"

namespace net {

template<typename T>
class udpConnection : public std::enable_shared_from_this<udpConnection<T>> {
public:

	enum class owner {
		server,
		client
	};

public:
    udpConnection(owner parent, asio::ip::udp::socket& sock, asio::ip::udp::endpoint endp, tsqueue<udpOwned_message<T>>& qIn)
                    : m_socket(sock), m_qMessagesIn(qIn) {

        m_remoteEndpoint = std::make_shared<asio::ip::udp::endpoint>(endp);
        m_OwnerType = parent;

        m_lastReceivedTime = std::chrono::steady_clock::now();
    }

    uint32_t GetID() const { return m_id; }

    void SetID(uint32_t id) const { m_id = id; }
    void SetID(uint32_t id) { m_id = id; }

    void Send(const message<T>& msg) {
        auto buffer = std::make_shared<std::vector<uint8_t>>(sizeof(message_header<T>) + msg.body.size());

        std::memcpy(buffer->data(), &msg.header, sizeof(message_header<T>));

        if (!msg.body.empty()) {
            std::memcpy(buffer->data() + sizeof(message_header<T>), msg.body.data(), msg.body.size());
        }

        m_socket.async_send_to(
            asio::buffer(buffer->data(), buffer->size()), 
            *m_remoteEndpoint,
            [buffer](std::error_code ec, std::size_t length) {
                if(ec) {
                    std::cout << "[UDP CONNECTION] Send Error: " << ec.message() << "\n";
                } else {
                    //std::cout << "[UDP CONNECTION] Send successfully: " << "\n";
                }
            }
        );
    }

    void OnDataReceived(std::shared_ptr<std::vector<uint8_t>> vBuffer, std::size_t length) {
        size_t headerSize = sizeof(message_header<T>);

        if(length >= headerSize) {
            message<T> msg;
            std::memcpy(&msg.header, vBuffer->data(), headerSize);

            size_t bodySize = length - headerSize;

            if(msg.header.size == bodySize) {

                if(bodySize > 0) {
                    msg.body.resize(bodySize);
                    std::memcpy(msg.body.data(), vBuffer->data() + headerSize, bodySize);
                }
                
                udpOwned_message<T> udpMsg;
                udpMsg.msg = msg;
                udpMsg.remoteEndpoint = this->shared_from_this();

                m_qMessagesIn.push_back(udpMsg);
        
                m_lastReceivedTime = std::chrono::steady_clock::now();

            } else {
                std::cout << "[UDP CONNECTION] Size mismatch! Header: " << msg.header.size << " Actual: " << bodySize << "\n";
            }
        }
    }

public:
    bool IsTimedOut(float timeoutSeconds) const {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastReceivedTime).count();
        return duration >= timeoutSeconds;
    }

private:
    tsqueue<udpOwned_message<T>>& m_qMessagesIn;

    owner m_OwnerType = owner::server;
    uint32_t m_id = 0;

    asio::ip::udp::socket& m_socket;
    std::shared_ptr<asio::ip::udp::endpoint> m_remoteEndpoint;

    std::chrono::steady_clock::time_point m_lastReceivedTime;
};

} // net
