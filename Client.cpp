//
// Created by lior3 on 21/09/2024.
//


#include "Client.h"

Client::Client(tcp::socket& socket): socket(socket), client_id(std::vector<uint8_t>(16,0)), version(100), request_op_code(0), payload_size(0),
                                     client_name("") , header_buffer(std::vector<uint8_t>()), file_path(""), payload(std::vector<uint8_t>()), file_name("") {

    try {
        get_data_from_transfer_file();
        if (std::filesystem::exists("me.info")) {
            std::cout << "Found existing client info, attempting to reconnect" << std::endl;
            request_op_code = RECONNECT;
            get_data_from_me_file();
            std::cout << "Reconnecting..." << std::endl;
        } else {
            std::cout << "No existing client info found, registering as new client" << std::endl;
            request_op_code = REGISTER;
            std::cout << "Registering..." << std::endl;
        }
        handle_sending_opCode(request_op_code);
    } catch (const std::exception& e) {
        std::cerr << "Initialization Error: " << e.what() << std::endl;
    }
}

Client::~Client() {
    try {
        socket.close();
        std::cout << "Socket closed successfully." << std::endl;
    } catch (const boost::system::system_error& e) {
        std::cerr << "Error while closing socket: " << e.what() << std::endl;
    }
    try{
        if (!fatal_error_message.empty()) {
            throw std::runtime_error(fatal_error_message);
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
    }
}

// Get data from "me.info" file
void Client::get_data_from_me_file() {
    std::vector<std::string> data = get_file_data("me.info");
    if (data.size() < 3) {
        throw std::runtime_error("Invalid me.info file format");
    }
    client_name = data[0];
    std::cout << "Client name: " << client_name << std::endl;

    try {
        client_uuid = boost::uuids::string_generator()(data[1]);
        std::cout << "Loaded client info - Client name: " << client_name << ", UUID: " << client_uuid << std::endl;
    } catch (const std::exception& e) {
        throw std::runtime_error("Error parsing UUID from me.info file");
    }

}

void Client::get_data_from_transfer_file() {

    std::cout << "Loading transfer info" << std::endl;
    std::vector<std::string> data = get_file_data("transfer.info");

    if (data.size() < 3) {
        throw std::runtime_error("Invalid transfer.info file format");
    } else {
        std::cout << "Reading from transfer file..." << std::endl;
    }

    client_name = data[1];
    file_path = data[2];

    std::cout << "Loaded transfer info - Client name: " << client_name << ", File path: " << file_path << std::endl;
}


//todo: check the function

// Convert a string name to a 255-byte array with padding
std::vector<uint8_t> Client::pad_string_to_255(const std::string& name_str) {
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



void Client::start() {
    try {
        std::cout << "Sending header to the server -  Op Code: " << request_op_code << std::endl;
        send_data_by_chunks();
        std::cout << "Header sent successfully!" << std::endl;

        std::cout << "Receiving response..." << std::endl;
        std::vector<uint8_t> response = receive_data_by_chunks();
        std::cout << "Response received successfully!" ;

        parse_response(response);
        manage_client_flow();
    } catch (const std::exception& e) {
        std::cerr << "Error during client start: " << e.what() << std::endl;
    }
}

void Client::manage_client_flow(){
    try {
        if (handle_received_opCode(received_op_code)){
            handle_sending_opCode(request_op_code);
            start();
        }

    } catch (const std::exception& e) {
        std::cerr << "Error during client flow: " << e.what() << std::endl;
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
    received_op_code = ntohs(*reinterpret_cast<const uint16_t*>(&response[1]));
    std::cout << " - Op Code: " << received_op_code << std::endl;
    payload_size = ntohl(*reinterpret_cast<const uint32_t*>(&response[3]));
    payload = std::vector<uint8_t>(response.begin() + 7, response.end());

}

bool Client::handle_received_opCode(uint16_t op_code) {
    switch(op_code) {
        case REGISTER_OK:{
            if (payload.size() != 16) {
                std::cerr << "Error: Invalid UUID length" << std::endl;
                request_op_code = REGISTER_NOK;
            }else{
                std::copy(payload.begin(), payload.end(), client_uuid.begin());
                std::cout << "REGISTER OK, UUID: " << client_uuid << std::endl;

                try{
                    create_me_file();
                    std::cout << "Me file created" << std::endl;
                }catch (const std::exception& e){
                    std::cerr << "Me file creation failed" << std::endl;
                    std::cerr << "Error: " << e.what() << std::endl;
                    request_op_code = TERMINATE_CONNECTION;
                    break;
                }
                request_op_code = SENDING_PUBLIC_KEY;
            }
            break;
        }
        case REGISTER_NOK:
            connection_request_count++;
            if (connection_request_count < 4) {
                std::cout << "REGISTER NOK" << std::endl;
                std::cout << "Trying to register again..." << std::endl;
                request_op_code = REGISTER;
            } else {
                std::cout << "REGISTER NOK after 3 attempts" << std::endl;
                std::cout << "Terminating connection..." << std::endl;
                fatal_error_message = "Registration failed after 3 attempts";
                std::string msg = std::string(payload.begin(), payload.end());

                request_op_code = TERMINATE_CONNECTION;
            }
            break;
        case RECEIVE_AES_KEY:{
            std::cout << "Analyzing AES key..." << std::endl;

            std::vector<uint8_t> encrypted_aes_key(payload.begin()+16, payload.end());
            try {
                crypto_key.decrypt_aes_key(encrypted_aes_key);
                std::cout << "AES key decrypted successfully" << std::endl;
            } catch (const std::exception& e) {
                std::cout << "AES key decryption failed" << std::endl;
                std::cerr << "Error: " << e.what() << std::endl;
                request_op_code = TERMINATE_CONNECTION;
                break;
            }
            request_op_code = SENDING_FILE;
            break;
        }

        case FILE_RECEIVE_OK_AND_CRC:{
            std::cout << "Checking CRC32 checksum..." << std::endl;
            uint32_t checksum = ntohl(*reinterpret_cast<const uint32_t*>(&payload[275]));
            if(crypto_key.verify_checksum(checksum)){
                std::cout << "File received successfully" << std::endl;
                request_op_code = CRC_OK;
            }else{
                std::cout << "File not received successfully" << std::endl;
                crc_not_ok_count++;
                if (crc_not_ok_count < 4) {
                    fatal_error_message = "File not received successfully CRC32 not equal to expected";
                    std::cout << "CRC NOT OK" << std::endl;
                    request_op_code = CRC_NOT_OK;
                } else {
                    std::cout << "CRC NOT OK after 4 attempts" << std::endl;
                    std::cout << "Terminating connection..." << std::endl;
                    request_op_code = CRC_TERMINATION;
                }
            }
            break;
        }
        case MESSAGE_RECEIVE_OK:
            if (crc_not_ok_count > 1 && crc_not_ok_count < 4) {
                request_op_code = SENDING_FILE;
            }else{
                fatal_error_message = std::string(payload.begin(), payload.end());
                std::cout << "Message received successfully" << std::endl;
                return false;

            }
            break;
        case RECONNECT_OK_SEND_AES:
            return handle_received_opCode(RECEIVE_AES_KEY);
            break;
        case RECONNECT_NOK:
            reconnection_request_count++;
            if (reconnection_request_count < 4) {
                std::cout << "Reconnect failed" << std::endl;
                request_op_code = RECONNECT;
            } else {
                std::cout << "Reconnect failed after 3 attempts" << std::endl;
                std::cout << "Terminating connection..." << std::endl;
                request_op_code = TERMINATE_CONNECTION;
            }
            break;
        case GENERAL_ERROR:
            std::cerr << "General error" << std::endl;
            request_op_code = TERMINATE_CONNECTION;
            break;
    }

    return true;
}

void Client::handle_sending_opCode(uint16_t op_code) {
    payload.clear();

    switch(op_code) {
        case REGISTER:
            add_to_payload(pad_string_to_255(client_name));
            std::cout << "Preparing registration request for client: " << client_name << std::endl;
            break;

        case SENDING_PUBLIC_KEY:{
            add_to_payload(pad_string_to_255(client_name));
            std::vector<uint8_t> public_key = crypto_key.get_public_key_base64();
            payload.insert(payload.end(), public_key.begin(), public_key.end());

            std::cout << "Sending public key" << std::endl;
            break;
        }

        case RECONNECT:
            add_to_payload(pad_string_to_255(client_name));
            break;

        case SENDING_FILE:{
            laod_file_content();
            std::vector<uint8_t> decrypted_file_size = get_file_size(file_content);
            std::vector<uint8_t> encrypted_file = crypto_key.encrypt_file(file_content);
            std::vector<uint8_t> encrypted_file_size = get_file_size(encrypted_file);

            add_to_payload(encrypted_file_size);
            add_to_payload(decrypted_file_size);
            add_to_payload(pad_string_to_255(file_name));
            add_to_payload(encrypted_file);

            std::cout << "Preparing to send file: " << file_name << std::endl;
            break;
        }

        case CRC_OK:
            payload = std::vector<uint8_t>(file_name.begin(), file_name.end());
            break;

        case CRC_NOT_OK:
            payload = std::vector<uint8_t>(file_name.begin(), file_name.end());
            break;

        case CRC_TERMINATION:
            payload = std::vector<uint8_t>(file_name.begin(), file_name.end());

            break;

        case TERMINATE_CONNECTION:
            std::cout << "Termination request" << std::endl;
            break;

        default:
            std::cerr << "Unknown op_code: " << op_code << std::endl;
            break;
    }

    payload_size = uint32_t(payload.size());

    load_header();
}
void Client::load_header() {

    header_buffer.clear();// Clear any existing data in the header_buffer
    header_buffer.reserve(client_id.size() + sizeof(version) + sizeof(request_op_code) + sizeof(payload_size) + payload.size());

    // Append client_id (as it's already a string of characters)
    header_buffer.insert(header_buffer.end(), client_uuid.begin(), client_uuid.end());

    // Append version (as a raw byte)
    header_buffer.push_back(static_cast<uint8_t>(version));

    // Append op_code (2 bytes, as it's a 16-bit integer)
    header_buffer.push_back(static_cast<uint8_t>((request_op_code >> 8) & 0xFF));  // High byte
    header_buffer.push_back(static_cast<uint8_t>(request_op_code & 0xFF));         // Low byte


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
    file << client_uuid << std::endl;
    file << crypto_key.get_private_key() << std::endl;
    file.close();
}

void Client::fatal_error(const std::string& message) {
    std::cerr << "Fatal Error: " << message << std::endl;
    std::exit(EXIT_FAILURE);  // Exit the program with failure code
}