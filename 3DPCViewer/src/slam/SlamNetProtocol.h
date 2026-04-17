#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <zmq.hpp>

namespace slam {
namespace net {

/**
 * @brief SLAM Network Protocol Commands
 */
enum class Command : uint8_t {
    CMD_START = 0x01,
    CMD_FRAME = 0x02,
    CMD_FINISH = 0x03,
    CMD_ERROR = 0xFF
};

/**
 * @brief Helper to send an empty or single string message parts
 */
inline bool sendStringPart(zmq::socket_t& socket, const std::string& data, zmq::send_flags flags = zmq::send_flags::none) {
    zmq::message_t message(data.size());
    if (data.size() > 0) {
        std::memcpy(message.data(), data.data(), data.size());
    }
    auto res = socket.send(message, flags);
    return res.has_value();
}

/**
 * @brief Helper to receive a string message part
 */
inline bool receiveStringPart(zmq::socket_t& socket, std::string& data, zmq::recv_flags flags = zmq::recv_flags::none) {
    zmq::message_t message;
    auto res = socket.recv(message, flags);
    if (!res.has_value()) {
        return false;
    }
    data.assign(static_cast<const char*>(message.data()), message.size());
    return true;
}

/**
 * @brief Helper to send command as the first part of a multipart message
 */
inline bool sendCommand(zmq::socket_t& socket, Command cmd, zmq::send_flags flags = zmq::send_flags::sndmore) {
    zmq::message_t message(sizeof(cmd));
    *static_cast<Command*>(message.data()) = cmd;
    auto res = socket.send(message, flags);
    return res.has_value();
}

/**
 * @brief Helper to receive a command from a multipart message
 */
inline bool receiveCommand(zmq::socket_t& socket, Command& cmd, zmq::recv_flags flags = zmq::recv_flags::none) {
    zmq::message_t message;
    auto res = socket.recv(message, flags);
    if (!res.has_value() || message.size() != sizeof(cmd)) {
        return false;
    }
    cmd = *static_cast<const Command*>(message.data());
    return true;
}

} // namespace net
} // namespace slam
