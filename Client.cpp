//
// Created by lior3 on 21/09/2024.
//

#include "Client.h"
Client::Client(tcp::socket& socket):socket(socket), client_id(""), version(100), request_code(0), payload_size(0), client_name("") , client_private_key("") {
    header = std::vector<char>();
    payload = std::vector<char>();
    get_data_from_me_file();
    get_data_from_transfer_file();

}

Client::~Client() {
    socket.close();
}

void Client::get_data_from_me_file() {

    std::vector<std::string> data= get_file_data("me.info");
    if (data.size() < 3) {
        std::cerr << "Error: The file is empty" << std::endl;
        get_data_from_transfer_file();
        return;
    }
    client_name = data[0];
    std::cout << "Client name: " << client_name << std::endl;
    client_id = data[1];
    std::cout << "Client ID: " << client_id << std::endl;

    for (std::size_t i = 2; i < data.size(); i++) {
        client_private_key += data[i];
    }
    std::cout << "Client private key: " << client_private_key << std::endl;

}
std::vector<std::string> Client::get_file_data(std::string file_name) {
    std::ifstream file(file_name);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open the file" << std::endl;
    }
    std::vector<std::string> data;
    while (!file.eof()) {
        std::string line;
        std::getline(file, line);
        data.push_back(line);
    }

    return data;

}
void Client::get_data_from_transfer_file() {

    std::vector<std::string> data= get_file_data("transfer.info");

    if (data.size() < 3) {
        std::cerr << "Error: transfer file is empty" << std::endl;
        return;
    }else
        std::cout << "reading from transfer file" << std::endl;

    if (client_name.empty()){
        client_name = data[1];
        std::cout << "Client name from transfer file: " << client_name << std::endl;
    }

//   handle_op_code(REGISTER);
//    std::ofstream file("me.info");
//    if (!file.is_open()) {
//        std::cerr << "Error: Could not open the file" << std::endl;
//    }
//    file << client_name << std::endl;
//    file << get_key << std::endl;
//    file.close();


    file_name = data[2];
    std::cout << "File name: " << file_name << std::endl;

}
void Client::handle_op_code(uint16_t op_code) {
    payload.clear();
    switch(op_code) {
        case REGISTER:
            payload = std::vector<char>(client_name.begin(), client_name.end());
            std::cout << "Registering client" << std::endl;
            break;
        case SENDING_PUBLIC_KEY:
            payload = std::vector<char>(client_name.begin(), client_name.end());
            payload.insert(payload.end(), client_private_key.begin(), client_private_key.end());
            std::cout << "Sending public key" << std::endl;
            break;
        case RECONNECT:
            payload = std::vector<char>(client_name.begin(), client_name.end());
            std::cout << "Reconnecting" << std::endl;
            break;
        case SENDING_FILE:
            payload = laod_file_content();
            std::cout << "Preparing to send file" << std::endl;
            break;
        case CRC_OK:
            payload = std::vector<char>(file_name.begin(), file_name.end());
            std::cout << "CRC OK" << std::endl;
            break;
        case CRC_NOT_OK:
            payload = std::vector<char>(file_name.begin(), file_name.end());
            std::cout << "CRC NOT OK" << std::endl;
            break;
        case CRC_TERMINATION:
            payload = std::vector<char>(file_name.begin(), file_name.end());
            std::cout << "CRC termination" << std::endl;
            break;
        default:
            std::cerr << "Unknown op_code: " << op_code << std::endl;
            break;
    }
    request_code = op_code;
    payload_size = uint32_t(payload.size());

    load_header();
    send_header_by_chunk();
}
void Client::load_header() {

    header.clear(); // Clear any existing data in the header

    header.reserve(client_id.size() + sizeof(version) + sizeof(request_code) + sizeof(payload_size) + payload.size());

    // Append client_id (as it's already a string of characters)
    header.insert(header.end(), client_id.begin(), client_id.end());

    // Append version (as a raw byte)
    header.push_back(static_cast<char>(version));

    // Append op_code (2 bytes, as it's a 16-bit integer)
    header.push_back(static_cast<char>((request_code >> 8) & 0xFF));  // High byte
    header.push_back(static_cast<char>(request_code & 0xFF));         // Low byte


    // Append payload size (4 bytes, as it's a 32-bit integer)
    header.push_back(static_cast<char>((payload_size >> 24) & 0xFF));  // Highest byte
    header.push_back(static_cast<char>((payload_size >> 16) & 0xFF));  // High byte
    header.push_back(static_cast<char>((payload_size >> 8) & 0xFF));   // Low byte
    header.push_back(static_cast<char>(payload_size & 0xFF));          // Lowest byte

    // Append the payload
    header.insert(header.end(), payload.begin(), payload.end());


//    std::string header_str = client_id;
//    header_str += version;
//    header_str += op_code;
//    header_str += payload_size;
//
//    header = std::vector<char>(header_str.begin(), header_str.end());
//    header.insert( header.end(), payload.begin(), payload.end());


}
void Client::send_header_by_chunk(){

    uint32_t total_bytes_sent = 0;
    uint32_t max_length = 1024;
    while (total_bytes_sent < header.size()) {
        uint32_t bytes_to_send = std::min(max_length, uint32_t(header.size() - total_bytes_sent));
        boost::asio::write(socket,  boost::asio::buffer(header.data() + total_bytes_sent, bytes_to_send));
        total_bytes_sent += bytes_to_send;
    }
    std::cout << "Header sent" << std::endl;

}
std::vector<char> Client::laod_file_content() {

    std::ifstream file(file_name);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open the file" << std::endl;
        return {};
    }
    std::vector<char> file_content;
    file.seekg(0, std::ios::end);
    file_content.resize(file.tellg());
    file.seekg(0, std::ios::beg);

    file.read(file_content.data(), file_content.size());

    return file_content;
}
