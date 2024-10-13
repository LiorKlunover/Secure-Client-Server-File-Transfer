//#include <iostream>
//#include <boost/asio.hpp>
//#include <string>
//#include <fstream>
//#include "Client.h"
//
//
//
//using boost::asio::ip::tcp;
//std::string get_port_ip();
////connect to the server, and make client class object to send data to the server
//int main() {
//
//    std::string port_ip = get_port_ip();
//    int index = port_ip.find(':');
//    std::string ip = port_ip.substr(0, index);
//    std::string port = port_ip.substr(index + 1);
//    std::cout << "ip: " << ip << " port: " << port << std::endl;
//
//    boost::asio::io_context io_context;
//    tcp::socket socket(io_context);
//    tcp::resolver resolver(io_context);
//    boost::asio::connect(socket, resolver.resolve(ip, port));
//    std::cout << "Connected to server" << std::endl;
//
//    Client client(socket);
//    client.start();
//
//    return 0;
//}
//
////get port from 127.0.0.1:1234
//std::string get_port_ip() {
//    std::ifstream file("transfer.info");
//    if (!file.is_open()) {
//        std::cerr << "Failed to open file: transfer.info" << std::endl;
//        return "";
//    }
//    std::string port;
//    std::getline(file, port);
//    if (port.empty()) {
//        std::cerr << "Failed to read port from file: transfer.info" << std::endl;
//        return "";
//    }
//    file.close();
//    return port;
//}
//
//
////}

#include <iostream>
#include <boost/asio.hpp>
#include <string>
#include <fstream>
#include "Client.h"

using boost::asio::ip::tcp;

// Function to retrieve IP and port from a file
std::string get_port_ip(const std::string& filename = "transfer.info") {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return "";
    }

    std::string port_ip;
    std::getline(file, port_ip);

    if (port_ip.empty()) {
        std::cerr << "Failed to read IP and port from file: " << filename << std::endl;
    }

    file.close();
    return port_ip;
}

// Function to connect to the server
bool connect_to_server(tcp::socket& socket, const std::string& ip, const std::string& port) {
    try {
        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        boost::asio::connect(socket, resolver.resolve(ip, port));
        return true;
    } catch (boost::system::system_error& e) {
        std::cerr << "Connection failed: " << e.what() << std::endl;
        return false;
    }
}

// Main function to run the client
int main() {
    std::string port_ip = get_port_ip();
    if (port_ip.empty()) {
        return 1;  // Exit if we failed to get IP and port
    }

    // Extract IP and port from the string
    int index = port_ip.find(':');
    if (index == std::string::npos) {
        std::cerr << "Invalid format in transfer.info (expected ip:port)" << std::endl;
        return 1;
    }

    std::string ip = port_ip.substr(0, index);
    std::string port = port_ip.substr(index + 1);

    std::cout << "Connecting to server at IP: " << ip << " Port: " << port << std::endl;

    // Set up Boost.Asio
    boost::asio::io_context io_context;
    tcp::socket socket(io_context);

    if (!connect_to_server(socket, ip, port)) {
        std::cerr << "Failed to connect to server." << std::endl;
        return 1;  // Exit if connection failed
    }

    std::cout << "Connected to server successfully." << std::endl;

    // Create the Client object and start communication
    try {
        Client client(socket);
        client.start();  // Assuming `start` is a method in the Client class for communication
    } catch (const std::exception& e) {
        std::cerr << "Client operation failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
