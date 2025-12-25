#pragma once

#include "net_common.h"
#include "net_tsqueue.h"
#include "net_message.h"
#include "net_udpConnection.h"

namespace net {

template<typename T>
class udp_server {
public:
    udp_server(uint16_t nPort) : m_socket(m_asioContext, asio::ip::udp::endpoint(asio::ip::udp::v4(), nPort)) {}

    virtual ~udp_server() {
        Stop();
    }

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

        if( m_threadContext.joinable() ) m_threadContext.join();

        std::cout << "[UDP SERVER] Stopped!\n";
    }

    void SendTo(const asio::ip::udp::endpoint& client, const message<T>& msg) {
        if(client) {
            client->Send(msg);
        }
    }

    void MessageAll(const message<T>& msg, const asio::ip::udp::endpoint& ignoreClient = asio::ip::udp::endpoint()) {
        std::lock_guard<std::mutex> lock(m_muxClients);
        for(auto& [endpoint, client] : m_clients) {
            if(endpoint == ignoreClient) continue;

            client->Send(msg);
        }
    }

    void Update(size_t nMaxMessages = -1, bool bWait = false) {
        {
            std::lock_guard<std::mutex> lock(m_muxClients);
            for (auto it = m_clients.begin(); it != m_clients.end(); ) {
                if (it->second->IsTimedOut(10.0f)) {
                    std::cout << "[UDP Server] Client " << it->second->GetID() << " timed out.\n";
                
                    //OnClientDisconnect(it->second);
                
                    it = m_clients.erase(it);
                } else {
                    ++it;
                }
            }
        }



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

    virtual void OnMessage(const std::shared_ptr<udpConnection<T>> client, message<T>& msg) {}

	virtual void OnMessage(const client_ref<T>& client, message<T>& msg) {}


protected:
    void WaitForPacket() {

        auto vBuffer = std::make_shared<std::vector<uint8_t>>(1500); 

        m_socket.async_receive_from(asio::buffer(vBuffer->data(), vBuffer->size()), m_tempEndpoint,
                [this, vBuffer](std::error_code ec, std::size_t length) {
                    if(!ec) {

                        std::lock_guard<std::mutex> lock(m_muxClients);
                        auto& conn = m_clients[m_tempEndpoint];

                        if(!conn) { 
                            conn = std::make_shared<udpConnection<T>>(
                                udpConnection<T>::owner::server, 
                                m_socket, 
                                m_tempEndpoint, 
                                m_qMessagesIn
                            );
                        }

                        conn->OnDataReceived(vBuffer, length);
            
                    } else {
                        std::cout << "[UDP Server] WaitForPacket Error: " << ec.message() << "\n";
                        if( !m_socket.is_open() ) return;
                    }

                    WaitForPacket();
                });
    }

private:
    tsqueue<udpOwned_message<T>> m_qMessagesIn;

    asio::io_context m_asioContext;
    std::thread m_threadContext;

    asio::ip::udp::socket m_socket;

    std::mutex m_muxClients;
    std::map<asio::ip::udp::udp::endpoint, std::shared_ptr<udpConnection<T>>> m_clients;
    asio::ip::udp::endpoint m_tempEndpoint;
};

} // net