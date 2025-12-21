#pragma once

#include "net_common.h"
#include "net_tsqueue.h"
#include "net_message.h"

namespace net {

template<typename T>
class udp_server {
public:
  udp_server(uint16_t nPort) : m_socket(m_asioContext, asio::ip::udp::endpoint(asio::ip::udp::v4(), nPort)) {}

    void Start() {

        try {
            WaitForPacket();

            m_threadContext = std::thread([this]() { m_asioContext.run(); });
        } catch (std::exception& e) {
            std::cerr << "[UDP SERVER] Exception: " << e.what() << "\n";
        }

        std::cout << "[UDP SERVER] Started!\n";
    }

    void Stop() {
        m_asioContext.stop();

        if(m_threadContext.joinable()) m_threadContext.join();

        std::cout << "[UDP SERVER] Stopped!\n";
    }

    void SendTo(const asio::ip::udp::endpoint& target, const message<T>& msg) {

        std::vector<uint8_t> buffer(sizeof(message_header<T>) + msg.body.size());

        std::memcpy(buffer.data(), &msg.header, sizeof(message_header<T>));

        if( !msg.body.empty() ) std::memcpy( buffer.data() + sizeof(message_header<T>), msg.body.data(), msg.body.size() );

        m_socket.async_send_to(asio::buffer(buffer.data(), buffer.size()), target,
            [](std::error_code ec, std::size_t length) {
                if(ec) std::cout << "[UDP] Send Error: " << ec.message() << "\n";
            });
    }

    void MessageAll(const message<T>& msg, const asio::ip::udp::endpoint& ignoreClient = asio::ip::udp::endpoint()) {
        auto sharedBuffer = std::make_shared<std::vector<uint8_t>>(sizeof(message_header<T>) + msg.body.size());
    
        std::memcpy(sharedBuffer->data(), &msg.header, sizeof(message_header<T>));

        if(!msg.body.empty()) std::memcpy(sharedBuffer->data() + sizeof(message_header<T>), msg.body.data(), msg.body.size());

        for(const auto& client : m_clients) {
            if(client == ignoreClient) continue;

            m_socket.async_send_to(asio::buffer(sharedBuffer->data(), sharedBuffer->size()), client,
                [sharedBuffer](std::error_code ec, std::size_t length) {

                    if(ec) {
                        std::cout << "[UDP] Removing dead client: " << client << " Reason: " << ec.message() << "\n";
                        this->m_clients.erase(client);
                    }
                });
            
        }
    }

    void Update(size_t nMaxMessages = -1, bool bWait = false) {
        if(bWait) m_qMessagesIn.wait();

        size_t nMessageCount = 0;
        while(nMessageCount < nMaxMessages && !m_qMessagesIn.empty()) {
            auto msg = m_qMessagesIn.pop_front();

            // Передаємо в обробник (адреса клієнта + саме повідомлення)
            OnMessage(msg.remoteEndpoint, msg.msg);
            OnMessage(client_ref<T>(msg.remoteEndpoint), msg.msg);
            nMessageCount++;
        }
    }

protected:

    virtual void OnMessage(const asio::ip::udp::endpoint& client, message<T>& msg) {}

	virtual void OnMessage(const client_ref<T>& client, message<T>& msg) {}


private:
    void WaitForPacket() {
        auto vBuffer = std::make_shared<std::vector<uint8_t>>(1024);

        if( !m_remoteEndpoint ) m_remoteEndpoint = std::make_shared<asio::ip::udp::endpoint>();


        m_socket.async_receive_from(
            asio::buffer(vBuffer->data(), vBuffer->size()), *m_remoteEndpoint,
            [this, vBuffer](std::error_code ec, std::size_t length) {
                if(!ec) {

                    if (m_clients.find(*m_remoteEndpoint) == m_clients.end()) {
                        std::cout << "[UDP] New client: " << *m_remoteEndpoint << "\n";
                        m_clients.insert(*m_remoteEndpoint);
                    }

                    message<T> msg;

                    size_t headerSize = sizeof(message_header<T>);

                    if(length >= headerSize) {
                        std::memcpy(&msg.header, vBuffer->data(), headerSize);

                        size_t actual_body_length = length - headerSize;

                        if(msg.header.size == actual_body_length) {
        
                            msg.body.resize(actual_body_length);
                            std::memcpy(msg.body.data(), vBuffer->data() + headerSize, actual_body_length);

                            m_qMessagesIn.push_back({ m_remoteEndpoint, msg });

                        } else {
                            std::cout << "[UDP SERVER] Warning: Packet size mismatch! Header says: " << msg.header.size << " Actual: " << actual_body_length << "\n";
                        }
                    

                    }

                } else {
                    std::cout << "[UDP SERVER] New Connection Error: " << ec.message() << "\n";
                }

                WaitForPacket(); 
            });
    }

protected:
    tsqueue<udpOwned_message<T>> m_qMessagesIn;

    asio::io_context m_asioContext;
    std::thread m_threadContext;

    asio::ip::udp::socket m_socket;
    std::shared_ptr<asio::ip::udp::endpoint> m_remoteEndpoint;

    std::set<asio::ip::udp::endpoint> m_clients;

};

} // net