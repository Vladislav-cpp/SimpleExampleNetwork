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
                    : m_socket(sock), m_remoteEndpoint(endp), m_qMessagesIn(qIn) {

            m_OwnerType = parent;
        }

    uint32_t GetID() const { return m_id; }

    void Send(const message<T>& msg) {
        auto buffer = std::make_shared<std::vector<uint8_t>>(sizeof(message_header<T>) + msg.body.size());

        std::memcpy(buffer->data(), &msg.header, sizeof(message_header<T>));

        if (!msg.body.empty()) {
            std::memcpy(buffer->data() + sizeof(message_header<T>), msg.body.data(), msg.body.size());
        }

        // Захоплюємо 'buffer' за значенням, щоб збільшити counter shared_ptr
        m_socket.async_send_to(
            asio::buffer(buffer->data(), buffer->size()), 
            m_remoteEndpoint,
            [buffer](std::error_code ec, std::size_t length) {
                if(ec) {
                    std::cout << "[UDP CONNECTION] Send Error: " << ec.message() << "\n";
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

                m_qMessagesIn.push_back({ m_remoteEndpoint, msg });
            } else {
                std::cout << "[UDP CONNECTION] Size mismatch! Header: " << msg.header.size << " Actual: " << bodySize << "\n";
            }
        }
    }

private:
    tsqueue<udpOwned_message<T>>& m_qMessagesIn;

    owner m_OwnerType = owner::server;
    uint32_t m_id = 0;

    asio::ip::udp::socket& m_socket;
    asio::ip::udp::endpoint m_remoteEndpoint;
};

} // net
