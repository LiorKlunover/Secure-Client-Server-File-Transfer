#include <iostream>
#include <boost/asio.hpp>
#include <string>
#include <fstream>
#include "Client.h"

using boost::asio::ip::tcp;

// Function to retrieve IP and port from a file
std::string get_port_ip(const std::string& filename = "transfer.info") {
    std::ifstream file(filename); // Open the file
    if (!file.is_open()) { // Check if the file was opened successfully
        std::cerr << "Failed to open file: " << filename << std::endl; // Log an error message if not
        return ""; // Return an empty string if the file could not be opened
    }

    std::string port_ip; // String to hold the IP and port
    std::getline(file, port_ip); // Read the IP and port from the file

    if (port_ip.empty()) { // Check if the IP and port were read successfully
        std::cerr << "Failed to read IP and port from file: " << filename << std::endl; // Log an error message if not
    }

    file.close(); // Close the file
    return port_ip; // Return the IP and port
}

// Function to connect to the server
bool connect_to_server(tcp::socket& socket, const std::string& ip, const std::string& port) {
    try {
        boost::asio::io_context io_context; // Create an I/O context
        tcp::resolver resolver(io_context); // Create a resolver
        boost::asio::connect(socket, resolver.resolve(ip, port)); // Connect to the server
        return true; // Return true if the connection was successful
    } catch (boost::system::system_error& e) { // Catch any exceptions
        std::cerr << "Connection failed: " << e.what() << std::endl; // Log the error message
        return false; // Return false if the connection failed
    }
}

// Main function to run the client
int main() {
    std::string port_ip = get_port_ip(); // Retrieve the IP and port from the file
    if (port_ip.empty()) { // Check if the IP and port were retrieved successfully
        return 1;  // Exit if we failed to get IP and port
    }

    // Extract IP and port from the string
    int index = port_ip.find(':'); // Find the position of the colon
    if (index == std::string::npos) { // Check if the colon was found
        std::cerr << "Invalid format in transfer.info (expected ip:port)" << std::endl; // Log an error message if not
        return 1; // Exit if the format is invalid
    }

    std::string ip = port_ip.substr(0, index); // Extract the IP
    std::string port = port_ip.substr(index + 1); // Extract the port

    std::cout << "Connecting to server at IP: " << ip << " Port: " << port << std::endl; // Log the IP and port

    // Set up Boost.Asio
    boost::asio::io_context io_context; // Create an I/O context
    tcp::socket socket(io_context); // Create a socket

    if (!connect_to_server(socket, ip, port)) { // Connect to the server
        std::cerr << "Failed to connect to server." << std::endl; // Log an error message if the connection failed
        return 1;  // Exit if connection failed
    }

    std::cout << "Connected to server successfully." << std::endl; // Log a success message

    // Create the Client object and start communication
    try {
        Client client(socket); // Create a Client object
        client.start();  // Start communication (assuming `start` is a method in the Client class)
    } catch (const std::exception& e) { // Catch any exceptions
        std::cerr << "Client operation failed: " << e.what() << std::endl; // Log the error message
        return 1; // Exit if an exception was thrown
    }

    return 0; // Return 0 to indicate success
}