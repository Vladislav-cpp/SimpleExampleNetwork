#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <deque>
#include <optional>
#include <vector>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <variant>
#include <set>
#include <map>

//#ifdef _WIN32
//#define _WIN32_WINNT 0x0A00
//#endif

#define ASIO_STANDELONE
#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>
