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

            if(!m_socket.is_open()) {
                std::error_code ec_open;
                m_socket.open(asio::ip::udp::v4(), ec_open);

                m_socket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), 0));
            
                if(ec_open) {
                    std::cout << "[UDP CLIENT] socket open Error: " << ec_open.message() << "\n";
                    return false;
                }

                std::cout << "[UDP CLIENT] socket open successfully .\n";
            }
            asio::ip::udp::resolver resolver(m_asioContext);
            asio::ip::udp::endpoint serverEndpoint = *resolver.resolve(host, std::to_string(port)).begin();


            m_connection = std::make_shared<udpConnection<T>>( udpConnection<T>::owner::client, m_socket, serverEndpoint, m_qMessagesIn );

            WaitForPacket();

            m_threadContext = std::thread([this]() { m_asioContext.run(); });

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

        auto vBuffer = std::make_shared<std::vector<uint8_t>>(10000); 

        m_socket.async_receive_from(asio::buffer(vBuffer->data(), vBuffer->size()), m_tempEndpoint,
                [this, vBuffer](std::error_code ec, std::size_t length) {
                    if(!ec) {
                        m_connection->OnDataReceived(vBuffer, length);
            
                        WaitForPacket();
                    } else {
                        if(ec == asio::error::message_size) std::cerr << "[UDP Client] Пакет занадто великий! Дані обрізано.\n";
                        std::cout << "[UDP Client] WaitForPacket Error: " << ec.message() << "\n";
                        std::cout << "[DEBUG] Is Open: " << m_socket.is_open() << "\n";
                        std::error_code ec_local;
                        auto local_ep = m_socket.local_endpoint(ec_local);
                        if (ec_local) std::cout << "[DEBUG] Local EP Error: " << ec_local.message() << "\n";
                        else std::cout << "[DEBUG] Local Port: " << local_ep.port() << "\n";



                        std::cout << "[DEBUG] Is Open: " << m_socket.is_open() << "\n";
                    }
                });
    }

private:
    asio::io_context m_asioContext;
    std::thread m_threadContext;
    asio::ip::udp::socket m_socket;
    
    std::shared_ptr<udpConnection<T>> m_connection;
    
    tsqueue<udpOwned_message<T>> m_qMessagesIn;
    asio::ip::udp::endpoint m_tempEndpoint;
};

} // net