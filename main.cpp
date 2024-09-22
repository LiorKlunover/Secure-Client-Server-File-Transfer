#include <iostream>
#include <boost/asio.hpp>
#include <string>
#include <fstream>
#include "Client.h"


using boost::asio::ip::tcp;
std::string get_port_ip();
//connect to the server, and make client class object to send data to the server
int main() {

    std::string port_ip = get_port_ip();
    int index = port_ip.find(':');
    std::string ip = port_ip.substr(0, index);
    std::string port = port_ip.substr(index + 1);
    std::cout << "ip: " << ip << " port: " << port << std::endl;


    boost::asio::io_context io_context;
    tcp::socket socket(io_context);
    tcp::resolver resolver(io_context);
    boost::asio::connect(socket, resolver.resolve(ip, port));
    std::cout << "Connected to server" << std::endl;

    Client client(socket);
    client.handle_op_code(828);

    return 0;
}

//get port from 127.0.0.1:1234
std::string get_port_ip() {
    std::ifstream file("transfer.info");
    if (!file.is_open()) {
        std::cerr << "Failed to open file: transfer.info" << std::endl;
        return "";
    }
    std::string port;
    std::getline(file, port);
    if (port.empty()) {
        std::cerr << "Failed to read port from file: transfer.info" << std::endl;
        return "";
    }
    file.close();
    return port;
}