//
// Created by lior3 on 21/09/2024.
//

#ifndef MAMAN14_CLIENT_H
#define MAMAN14_CLIENT_H
#include <vector>
#include <boost/asio.hpp>
#include <filesystem>
#include <iostream>
#include <string>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid_serialize.hpp>
#include <fstream>
#include <winsock2.h>
#include "CryptoPPKey.h"
#include <filesystem>

using boost::asio::ip::tcp;

class Client {
public:
    explicit Client(tcp::socket& socket);
    ~Client();

    // Deleted copy constructor and assignment operator
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    void start();

private:
    enum ClientRequestCode : uint16_t {
        REGISTER = 825,
        SENDING_PUBLIC_KEY = 826,
        RECONNECT = 827,
        SENDING_FILE = 828,
        CRC_OK = 900,
        CRC_NOT_OK = 901,
        CRC_TERMINATION = 902,
        TERMINATE_CONNECTION = 903
    };

    enum ServerResponseCode : uint16_t {
        REGISTER_OK = 1600,
        REGISTER_NOK = 1601,
        RECEIVE_AES_KEY = 1602,
        FILE_RECEIVE_OK_AND_CRC = 1603,
        MESSAGE_RECEIVE_OK = 1604,
        RECONNECT_OK_SEND_AES = 1605,
        RECONNECT_NOK = 1606,
        GENERAL_ERROR = 1607
    };

    static constexpr size_t MAX_RETRIES = 3;
    static constexpr size_t CHUNK_SIZE = 1024;
    static constexpr uint8_t CLIENT_VERSION = 100;
    static constexpr size_t HEADER_SIZE = 23; // 16 (UUID) + 1 (version) + 2 (op code) + 4 (payload size)


    // Core member variables
    tcp::socket& socket;
    boost::uuids::uuid client_uuid;
    CryptoPPKey crypto_key;


    // State variables
    std::string client_name;
    std::string file_path;
    std::string file_name;
    std::vector<uint8_t> file_content;
    std::vector<uint8_t> payload;
    uint16_t request_op_code;
    uint16_t received_op_code;
    uint32_t payload_size;
    std::string fatal_error_message;
    std::vector<uint8_t> header_buffer;
    std::vector<uint8_t> client_id;
    uint8_t version;

    // Retry counters
    int connection_request_count = 1;
    int crc_not_ok_count = 1;
    int reconnection_request_count = 1;



    // Helper functions
    void get_data_from_me_file();
    void get_data_from_transfer_file();
    std::vector<std::string> get_file_data(const std::string& file_name);
    std::vector<uint8_t> pad_string_to_255(const std::string& name_str);
    void laod_file_content();
    std::vector<uint8_t> get_file_size(std::vector<uint8_t> file_content);

    void send_data_by_chunks();
    std::vector<uint8_t> receive_data_by_chunks();

    void handle_sending_opCode(uint16_t op_code);
    bool handle_received_opCode(uint16_t request_code);
    void manage_client_flow();
    void load_header();

    void parse_response(const std::vector<uint8_t>& response);
    void add_to_payload(std::vector<uint8_t> data);
    void create_me_file();
    void fatal_error(const std::string& error_message);
};


#endif //MAMAN14_CLIENT_H
