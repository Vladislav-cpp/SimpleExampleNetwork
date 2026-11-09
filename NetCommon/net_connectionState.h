#pragma once

namespace net {

    enum class ConnectionState {
        Idle, // Offline
        Connecting,
        Connected,
        Failed
    };

}