#pragma once
#include <thread>
#include "net_common.h"

namespace net {

template <typename T>
class udp_client {
public:
    udp_client() : m_socket(m_asioContext) {
        // Сокет відкривається на випадковому порту
    }

    ~udp_client() {
        Disconnect();
    }

    bool Start(const std::string& host, const uint16_t port) {

        try {
            asio::ip::udp::resolver resolver(m_asioContext);
            m_serverEndpoint = *resolver.resolve(host, std::to_string(port)).begin();

            m_threadContext = std::thread([this]() { m_asioContext.run(); });

            WaitForPacket();

            return true;
        } catch (std::exception& e) {
            std::cout << "[UDP CLIENT] Start Error: " << ec.message() << "\n";
            return false;
        }
    }

    void Disconnect() {
        m_asioContext.stop();
        if (m_threadContext.joinable()) m_threadContext.join();
    }

    void Send(const message<T>& msg) {
        auto buffer = std::make_shared<std::vector<uint8_t>>(sizeof(message_header<T>) + msg.body.size());
        std::memcpy(buffer->data(), &msg.header, sizeof(message_header<T>));
        if(!msg.body.empty()) {
            std::memcpy(buffer->data() + sizeof(message_header<T>), msg.body.data(), msg.body.size());
        }

        m_socket.async_send_to(asio::buffer(buffer->data(), buffer->size()), m_serverEndpoint,
            [buffer](std::error_code ec, std::size_t length) {
                if(ec) std::cout << "[UDP CLIENT] Send Error: " << ec.message() << "\n";
            });
    }

    tsqueue<udpOwned_message<T>>& Incoming() {
        return m_qMessagesIn;
    }

private:
    void WaitForPacket() {
        auto vBuffer = std::make_shared<std::vector<uint8_t>>(1500);

        m_socket.async_receive_from(
            asio::buffer(vBuffer->data(), vBuffer->size()), m_fromServerEndpoint,
            [this, vBuffer](std::error_code ec, std::size_t length) {
                if(!ec) {
                    message<T> msg;
                    size_t headerSize = sizeof(message_header<T>);

                    if(length >= headerSize) {
                        std::memcpy(&msg.header, vBuffer->data(), headerSize);
                        size_t bodySize = length - headerSize;

                        if(msg.header.size == bodySize) {

                            if(bodySize > 0) {
                                msg.body.resize(bodySize);
                                std::memcpy(msg.body.data(), vBuffer->data() + headerSize, bodySize);
                            }

                            m_qMessagesIn.push_back({ m_fromServerEndpoint, msg });
                        }
                    }

                    WaitForPacket();

                } else {
                    std::cout << "[UDP CLIENT] WaitForPacket Error: " << ec.message() << "\n";
                }
            });
    }

private:
    asio::io_context m_asioContext;
    std::thread m_threadContext;
    asio::ip::udp::socket m_socket;
    
    asio::ip::udp::endpoint m_serverEndpoint;     // Адреса сервера
    asio::ip::udp::endpoint m_fromServerEndpoint; // Буфер для отримання адреси
    
    tsqueue<udpOwned_message<T>> m_qMessagesIn; // Черга вхідних
};

} // net