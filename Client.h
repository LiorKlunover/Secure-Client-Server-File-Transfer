//
// Created by lior3 on 21/09/2024.
//

#ifndef MAMAN14_CLIENT_H
#define MAMAN14_CLIENT_H
#include <vector>
#include <boost/asio.hpp>
#include <iostream>
#include <fstream>
#include <cryptopp/cryptlib.h>

using boost::asio::ip::tcp;
class Client {

private:
    enum ClientRequestCode {
        REGISTER = 825,
        SENDING_PUBLIC_KEY = 826,
        RECONNECT = 827,
        SENDING_FILE = 828,
        CRC_OK = 900,
        CRC_NOT_OK = 901,
        CRC_TERMINATION = 902,
    };
    std::vector<char> header;
    tcp::socket& socket;
    std::string client_id;
    uint8_t version;
    uint16_t request_code;
    uint32_t payload_size;
    std::string file_name;
    std::vector<char> payload;
    std::string client_name;
    std::string client_private_key;

public:
    Client(tcp::socket& socket);
    ~Client();
    Client operator = (const Client& other);
    void get_data_from_me_file();
    void get_data_from_transfer_file();
    std::vector<std::string> get_file_data(std::string file_name);
    void load_header();
    std::vector<char> laod_file_content();
    void send_header_by_chunk();
    void handle_op_code(uint16_t op_code);
};


#endif //MAMAN14_CLIENT_H
