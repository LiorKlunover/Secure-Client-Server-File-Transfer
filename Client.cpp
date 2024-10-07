//
// Created by lior3 on 21/09/2024.
//

#include "Client.h"

Client::Client(tcp::socket& socket): socket(socket), client_id(std::vector<uint8_t>(16,0)), version(100), request_code(0), payload_size(0),
                                     client_name(""), client_private_key("") , header_buffer(std::vector<uint8_t>()), file_path(""), payload(std::vector<uint8_t>()),file_name("") {

    try {
        get_data_from_transfer_file();
        if (std::filesystem::exists("me.info")) {
            request_code = RECONNECT;
            get_data_from_me_file();
            std::cout << "Reconnecting..." << std::endl;
        } else {
            request_code = REGISTER;
            std::cout << "Registering..." << std::endl;
        }
        handle_sending_opCode(request_code);
    } catch (const std::exception& e) {
        std::cerr << "Initialization Error: " << e.what() << std::endl;
    }
}

Client::~Client() {
    try {
        socket.close();
    } catch (const boost::system::system_error& e) {
        std::cerr << "Error while closing socket: " << e.what() << std::endl;
    }
}

// Get data from "me.info" file
void Client::get_data_from_me_file() {
    std::vector<std::string> data = get_file_data("me.info");
    if (data.size() < 3) {
        std::cerr << "Error: 'me.info' file is not formatted correctly. Expected 3 lines." << std::endl;
        return;
    }
    client_name = data[0];
    std::cout << "Client name: " << client_name << std::endl;

    try {
        uuid = boost::uuids::string_generator()(data[1]);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing UUID: " << e.what() << std::endl;
        return;
    }
    std::cout << "Client ID: " << data[1] << std::endl;

//    for (std::size_t i = 2; i < data.size(); i++) {
//        client_private_key += data[i];
//    }
    //cryptoKey = CryptoPPKey(client_private_key);
    //cryptoKey = CryptoPPKey(cryptoKey.get_private_key_from_private_file());

    std::cout << "Client private key loaded." << std::endl;
}

void Client::get_data_from_transfer_file() {
    std::vector<std::string> data = get_file_data("transfer.info");

    if (data.size() < 3) {
        std::cerr << "Error: 'transfer.info' file is not formatted correctly. Expected at least 3 lines." << std::endl;
        return;
    } else {
        std::cout << "Reading from transfer file..." << std::endl;
    }

    client_name = data[1];
    file_path = data[2];
    std::cout << "File path: " << file_path << std::endl;
}


//todo: check the function

// Convert a string name to a 255-byte array with padding
std::vector<uint8_t> Client::get_name_255(const std::string& name_str) {
    std::vector<uint8_t> name_bytes(255, 0); // Initialize with zeros
    size_t name_length = name_str.size();

    if (name_length > 254) {
        std::cerr << "Error: Name exceeds 254 characters. Truncating..." << std::endl;
        name_length = 254; // Truncate if too long
    }

    std::copy(name_str.begin(), name_str.begin() + name_length, name_bytes.begin());
    name_bytes[name_length] = 0; // Null-terminate
    return name_bytes;
}


// Get data from a specified file
std::vector<std::string> Client::get_file_data(const std::string& file_name) {
    std::ifstream file(file_name);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file '" << file_name << "'." << std::endl;
        return {};
    }

    std::vector<std::string> data;
    std::string line;

    while (std::getline(file, line)) {
        data.push_back(line);
    }
    file.close();
    return data;
}

void Client::manage_client_flow(){
    std::cout << "Waiting for response" << std::endl;

//    handle_sending_opCode(SENDING_PUBLIC_KEY);
//    handle_sending_opCode(RECONNECT);
//    handle_sending_opCode(SENDING_FILE);
//    handle_sending_opCode(CRC_OK);
//    handle_sending_opCode(CRC_NOT_OK);
//    handle_sending_opCode(CRC_TERMINATION);
}

void Client::start() {
    try {
        std::cout << "Sending header..." << std::endl;
        send_data_by_chunks();
        std::cout << "Header sent successfully." << std::endl;

        std::cout << "Receiving response..." << std::endl;
        std::vector<uint8_t> response = receive_data_by_chunks();
        std::cout << "Response received" << std::endl;

        parse_response(response);
    } catch (const std::exception& e) {
        std::cerr << "Error during client start: " << e.what() << std::endl;
    }
}


void Client::send_data_by_chunks() {
    uint32_t total_bytes_sent = 0;
    uint32_t max_length = 1024;
    try {
        while (total_bytes_sent < header_buffer.size()) {
            uint32_t bytes_to_send = std::min(max_length, uint32_t(header_buffer.size() - total_bytes_sent));
            total_bytes_sent += boost::asio::write(socket, boost::asio::buffer(header_buffer.data() + total_bytes_sent, bytes_to_send));
        }
        std::cout << "Header sent" << std::endl;
    } catch (const boost::system::system_error& e) {
        std::cerr << "Error during write: " << e.what() << std::endl;
        return;
    }
}

std::vector<uint8_t> Client::receive_data_by_chunks() {
    std::vector<uint8_t> response;
    try {
        std::vector<uint8_t> buffer(1024);
        boost::system::error_code error;

        while (true) {
            size_t bytes_transferred = socket.read_some(boost::asio::buffer(buffer), error);

            if (error == boost::asio::error::eof) {
                break;
            } else if (error) {
                throw boost::system::system_error(error);
            }

            response.insert(response.end(), buffer.begin(), buffer.begin() + bytes_transferred);

            if (bytes_transferred < buffer.size()) {
                break;
            }
        }
    } catch (std::exception& e) {
        std::cerr << "Exception in receive_data_by_chunks: " << e.what() << std::endl;
        throw;
    }
    return response;
}

// Parse the response received from the server
void Client::parse_response(const std::vector<uint8_t>& response) {
    if (response.size() < 7) {
        std::cerr << "Error: Response is too short." << std::endl;
        return; // Early exit on invalid response size
    }

    uint8_t version1 = response[0];
    uint16_t op_code = ntohs(*reinterpret_cast<const uint16_t*>(&response[1]));
    payload_size = ntohl(*reinterpret_cast<const uint32_t*>(&response[3]));
    payload = std::vector<uint8_t>(response.begin() + 7, response.end());
    handle_received_opCode(op_code);
}

void Client::handle_received_opCode(uint16_t op_code) {
    switch(op_code) {
        case REGISTER_OK:{
            if (payload.size() != 16) {
                std::cerr << "Error: Invalid UUID length" << std::endl;
                op_code = REGISTER_NOK;
            }else{
                std::copy(payload.begin(), payload.end(), uuid.begin());
                std::cout << "REGISTER OK, UUID: " << uuid << std::endl;

                try{
                    create_me_file();
                    std::cout << "Me file created" << std::endl;
                }catch (const std::exception& e){
                    std::cerr << "Me file creation failed" << std::endl;
                    std::cerr << "Error: " << e.what() << std::endl;
                    op_code = TERMINAT_CONNECTION;
                    break;
                }
                op_code = SENDING_PUBLIC_KEY;
            }
            break;
        }
        case REGISTER_NOK:
            connection_request_count++;
            if (connection_request_count < 4) {
                std::cout << "REGISTER NOK" << std::endl;
                std::cout << "Trying to register again..." << std::endl;
                op_code = REGISTER;
            } else {
                std::cout << "REGISTER NOK after 3 attempts" << std::endl;
                std::cout << "Terminating connection..." << std::endl;
                op_code = TERMINAT_CONNECTION;
            }
            break;
        case RECEIVE_AES_KEY:{
            std::cout << "Receiving AES key..." << std::endl;

            std::vector<uint8_t> encrypted_aes_key(payload.begin()+16, payload.end());
            try {
                cryptoKey.decrypt_aes_key(encrypted_aes_key);
                std::cout << "AES key decrypted successfully" << std::endl;
            } catch (const std::exception& e) {
                std::cout << "AES key decryption failed" << std::endl;
                std::cerr << "Error: " << e.what() << std::endl;
                op_code = TERMINAT_CONNECTION;
                break;
            }
            op_code = SENDING_FILE;
            break;
        }
        case FILE_RECEIVE_OK_AND_CRC:{
            std::cout << "Checking CRC32 checksum..." << std::endl;
            uint32_t checksum = ntohl(*reinterpret_cast<const uint32_t*>(&payload[275]));
            if(cryptoKey.verify_checksum(checksum)){
                std::cout << "File received successfully" << std::endl;
                op_code = CRC_OK;
            }else{
                std::cout << "File not received successfully" << std::endl;
                crc_not_ok_count++;
                if (crc_not_ok_count < 4) {
                    std::cout << "CRC NOT OK" << std::endl;
                    op_code = CRC_NOT_OK;
                } else {
                    std::cout << "CRC NOT OK after 4 attempts" << std::endl;
                    std::cout << "Terminating connection..." << std::endl;
                    op_code = CRC_TERMINATION;
                }
            }
            break;
        }
        case MESSAGE_RECEIVE_OK:
            std::cout << "Message received successfully" << std::endl;
            return;
            break;
        case RECONNECT_OK_SEND_AES:
            handle_received_opCode(RECEIVE_AES_KEY);
            return;
            break;
        case RECONNECT_NOK:
            reconnection_request_count++;
            if (reconnection_request_count < 4) {
                std::cout << "Reconnect failed" << std::endl;
                op_code = RECONNECT;
            } else {
                std::cout << "Reconnect failed after 3 attempts" << std::endl;
                std::cout << "Terminating connection..." << std::endl;
                op_code = TERMINAT_CONNECTION;
            }
            break;
        case GENERAL_ERROR:
            std::cerr << "General error" << std::endl;
            op_code = TERMINAT_CONNECTION;
            break;
    }
    handle_sending_opCode(op_code);
    start();
}

void Client::handle_sending_opCode(uint16_t op_code) {
    payload.clear();
    switch(op_code) {
        case REGISTER:
            add_to_payload(get_name_255(client_name));
           // payload = std::vector<uint8_t>(client_name.begin(), client_name.end());
            std::cout << "Registering client" << std::endl;
            break;
        case SENDING_PUBLIC_KEY:{
            add_to_payload(get_name_255(client_name));
            std::vector<uint8_t> public_key = cryptoKey.get_public_key_base64();

            payload.insert(payload.end(), public_key.begin(), public_key.end());
            std::cout << "Payload content  " << std::string(payload.begin(), payload.end()) << std::endl;
            std::cout << "Sending public key" << std::endl;
            break;
        }
        case RECONNECT:
            add_to_payload(get_name_255(client_name));
            break;
        case SENDING_FILE:{
            laod_file_content();
            std::vector<uint8_t> decrypted_file_size = get_file_size(file_content);
            std::vector<uint8_t> encrypted_file = cryptoKey.encrypt_file(file_content);
            std::vector<uint8_t> encrypted_file_size = get_file_size(encrypted_file);

            add_to_payload(encrypted_file_size);
            add_to_payload(decrypted_file_size);
            add_to_payload(get_name_255(file_name));
            add_to_payload(encrypted_file);

            std::cout << "Preparing to send file" << std::endl;
            break;
        }
        case CRC_OK:
            payload = std::vector<uint8_t>(file_name.begin(), file_name.end());
            std::cout << "CRC OK" << std::endl;
            break;
        case CRC_NOT_OK:
            handle_sending_opCode(SENDING_FILE);
            payload = std::vector<uint8_t>(file_name.begin(), file_name.end());
            std::cout << "CRC NOT OK" << std::endl;
            break;
        case CRC_TERMINATION:
            payload = std::vector<uint8_t>(file_name.begin(), file_name.end());
            std::cout << "CRC termination" << std::endl;
            break;
        case TERMINAT_CONNECTION:
            std::cout << "Termination request" << std::endl;
            break;
        default:
            std::cerr << "Unknown op_code: " << op_code << std::endl;
            break;
    }
    request_code = op_code;
    payload_size = uint32_t(payload.size());

    load_header();
}
void Client::load_header() {

    header_buffer.clear();// Clear any existing data in the header_buffer
    header_buffer.reserve(client_id.size() + sizeof(version) + sizeof(request_code) + sizeof(payload_size) + payload.size());

    // Append client_id (as it's already a string of characters)
    header_buffer.insert(header_buffer.end(), uuid.begin(), uuid.end());

    // Append version (as a raw byte)
    header_buffer.push_back(static_cast<uint8_t>(version));

    // Append op_code (2 bytes, as it's a 16-bit integer)
    header_buffer.push_back(static_cast<uint8_t>((request_code >> 8) & 0xFF));  // High byte
    header_buffer.push_back(static_cast<uint8_t>(request_code & 0xFF));         // Low byte


    // Append payload size (4 bytes, as it's a 32-bit integer)
    header_buffer.push_back(static_cast<uint8_t>((payload_size >> 24) & 0xFF));  // Highest byte
    header_buffer.push_back(static_cast<uint8_t>((payload_size >> 16) & 0xFF));  // High byte
    header_buffer.push_back(static_cast<uint8_t>((payload_size >> 8) & 0xFF));   // Low byte
    header_buffer.push_back(static_cast<uint8_t>(payload_size & 0xFF));          // Lowest byte

    // Append the payload
    header_buffer.insert(header_buffer.end(), payload.begin(), payload.end());
}


void Client::laod_file_content() {

    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open the file" << std::endl;
        return;
    }
    //update the file name
    file_name = file_path.substr(file_path.find_last_of("/\\") + 1);
    file.seekg(0, std::ios::end);
    file_content.resize(file.tellg());
    file.seekg(0, std::ios::beg);

    file.read(reinterpret_cast<char*>(file_content.data()), file_content.size());
    file.close();

}


//void Client::manage_encryption() {
//    std::vector<uint8_t> encrypted_aes_key(payload.begin()+16, payload.end());
//    cryptoKey.decrypt_aes_key(encrypted_aes_key);
//    handle_sending_opCode(SENDING_FILE);
//}

std::vector<uint8_t> Client::get_file_size(std::vector<uint8_t> file_content) {
    uint32_t file_size = file_content.size();
    std::vector<uint8_t> size;
    size.push_back(static_cast<uint8_t>((file_size >> 24) & 0xFF));  // Highest byte
    size.push_back(static_cast<uint8_t>((file_size >> 16) & 0xFF));  // High byte
    size.push_back(static_cast<uint8_t>((file_size >> 8) & 0xFF));   // Low byte
    size.push_back(static_cast<uint8_t>(file_size & 0xFF));          // Lowest byte
    return size;
}

void Client::add_to_payload(std::vector<uint8_t> data) {
    payload.insert(payload.end(), data.begin(), data.end());
}
void Client::create_me_file(){
    std::cout << "Creating me file" << std::endl;
    std::ofstream file("me.info", std::ios::trunc);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open the file" << std::endl;
        throw std::invalid_argument("Error: Could not open the file");

    }

    std::cout << "me file created" << std::endl;

    file << client_name << std::endl;
    file << uuid << std::endl;
    file << cryptoKey.get_private_key() << std::endl;
    file.close();
}

void Client::fatal_error(const std::string& message) {
    std::cerr << "Fatal Error: " << message << std::endl;
    std::exit(EXIT_FAILURE);  // Exit the program with failure code
}