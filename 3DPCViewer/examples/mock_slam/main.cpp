#include <iostream>
#include <chrono>
#include <thread>
#include <zmq.hpp>
#include "../../src/slam/SlamNetProtocol.h"

int main() {
    zmq::context_t context(1);
    zmq::socket_t socket(context, zmq::socket_type::rep);
    socket.bind("tcp://127.0.0.1:5555");

    std::cout << "[Mock SLAM] Listening on tcp://127.0.0.1:5555" << std::endl;

    while (true) {
        slam::net::Command cmd;
        int more;
        size_t more_size = sizeof(more);

        if (!slam::net::receiveCommand(socket, cmd, zmq::recv_flags::none)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (cmd == slam::net::Command::CMD_START) {
            // Must consume all incoming multipart segments!
            while (socket.get(zmq::sockopt::rcvmore)) {
                std::string dummy;
                slam::net::receiveStringPart(socket, dummy);
            }

            std::cout << "[Mock SLAM] Received CMD_START." << std::endl;
            // Acknowledge Start
            slam::net::sendCommand(socket, slam::net::Command::CMD_START, zmq::send_flags::sndmore);
            slam::net::sendStringPart(socket, "STARTED");
        } 
        else if (cmd == slam::net::Command::CMD_FRAME) {
            std::vector<std::string> incoming_parts;
            while (socket.get(zmq::sockopt::rcvmore)) {
                std::string data;
                slam::net::receiveStringPart(socket, data);
                incoming_parts.push_back(data);
            }
            std::cout << "[Mock SLAM] Received CMD_FRAME with " << incoming_parts.size() << " payload parts. Processing..." << std::endl;

            // Simulate processing delay
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // Send back fake pose & cloud data
            slam::net::sendCommand(socket, slam::net::Command::CMD_FRAME, zmq::send_flags::sndmore);
            slam::net::sendStringPart(socket, "FAKE_POSE_AND_CLOUD");
        } 
        else if (cmd == slam::net::Command::CMD_FINISH) {
            while (socket.get(zmq::sockopt::rcvmore)) {
                std::string dummy;
                slam::net::receiveStringPart(socket, dummy);
            }

            std::cout << "[Mock SLAM] Received CMD_FINISH. Shutting down." << std::endl;
            slam::net::sendCommand(socket, slam::net::Command::CMD_FINISH, zmq::send_flags::sndmore);
            slam::net::sendStringPart(socket, "SHUTTING_DOWN");
            break;
        }
    }

    std::cout << "[Mock SLAM] Exited cleanly." << std::endl;
    return 0;
}
