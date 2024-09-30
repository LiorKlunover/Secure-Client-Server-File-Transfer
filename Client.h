//
// Created by lior3 on 21/09/2024.
//

#ifndef MAMAN14_CLIENT_H
#define MAMAN14_CLIENT_H
#include <vector>
#include <boost/asio.hpp>
#include <iostream>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <fstream>
#include <winsock2.h>
#include "CryptoPPKey.h"

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
    enum ServerResponseCode {
        REGISTER_OK = 1600,
        REGISTER_NOK = 1601,
        RECEIVE_AES_KEY = 1602,
        CRC_RECEIVE_OK = 1603,
        MESSAGE_RECEIVE_OK = 1604,
        RECONNECT_OK_SEND_AES = 1605,
        RECONNECT_NOK = 1606,
        GENERAL_ERROR = 1607,
    };
    std::vector<uint8_t> header_buffer;
    tcp::socket& socket;
    boost::uuids::uuid uuid;
    std::vector<uint8_t> client_id;
    uint8_t version;
    uint16_t request_code;
    uint32_t payload_size;
    std::string client_name;
    std::vector<uint8_t> file_content;
    std::string file_name;
    std::vector<uint8_t> payload;
    std::string client_private_key;
   // std::vector<uint8_t> client_name;
    CryptoPPKey cryptoKey;

    int connection_request_count = 0;

public:
    Client(tcp::socket& socket);
    ~Client();
    Client operator = (const Client& other);
    void get_data_from_me_file();
    void get_data_from_transfer_file();
    std::vector<std::string> get_file_data(std::string file_name);
    void load_header();
    std::vector<uint8_t> get_name_255(std::string name);
    void laod_file_content();
    std::vector<uint8_t> get_file_size(std::vector<uint8_t> file_content);
    void send_data_by_chunks();
    std::vector<uint8_t> receive_data_by_chunks();
    void handle_send_opCode(uint16_t op_code);
    std::vector<uint8_t> parse_client_id(std::string client_id);
    void manage_client_flow();
    void start();
    void parse_response(std::vector<uint8_t> response);
    void handel_response_opCode(uint16_t op_code);
    void manage_encryption();
    void add_to_payload(std::vector<uint8_t> data);
    void create_me_file();
};


#endif //MAMAN14_CLIENT_H
