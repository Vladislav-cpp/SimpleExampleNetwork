#pragma once
#include "net_common.h"
#include "net_udpConnection.h"

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
            asio::ip::udp::endpoint serverEndpoint = *resolver.resolve(host, std::to_string(port)).begin();


            m_connection = std::make_shared<udpConnection<T>>( udpConnection<T>::owner::client, m_socket, serverEndpoint, m_qMessagesIn );

            m_threadContext = std::thread([this]() { m_asioContext.run(); });

            WaitForPacket();

            return true;
        } catch (std::exception& ec) {
            std::cout << "[UDP CLIENT] Start Error: " << ec.what() << "\n";
            return false;
        }
    }

    void Disconnect() {
        m_asioContext.stop();

        if( m_threadContext.joinable() ) m_threadContext.join();
    }

    void Send(const message<T>& msg) {
        if(m_connection) m_connection->Send(msg);
    }

    tsqueue<udpOwned_message<T>>& Incoming() {
        return m_qMessagesIn;
    }

private:
    void WaitForPacket() {

        auto vBuffer = std::make_shared<std::vector<uint8_t>>(1500); 

        auto tempEndpoint = std::make_shared<asio::ip::udp::endpoint>();

        m_socket.async_receive_from(asio::buffer(vBuffer->data(), vBuffer->size()), tempEndpoint,
                [this, vBuffer](std::error_code ec, std::size_t length) {
                    if(!ec) {
                        // У клієнта зазвичай один m_connection до сервера
                        m_connection->OnDataReceived(vBuffer, length);
            
                        WaitForPacket();
                    } else {
                        std::cout << "[UDP Client] WaitForPacket Error: " << ec.message() << "\n";
                    }
                });
    }

private:
    asio::io_context m_asioContext;
    std::thread m_threadContext;
    asio::ip::udp::socket m_socket;
    
    std::shared_ptr<udpConnection<T>> m_connection;
    
    tsqueue<udpOwned_message<T>> m_qMessagesIn;
};

} // net