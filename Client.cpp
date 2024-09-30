//
// Created by lior3 on 21/09/2024.
//

#include "Client.h"

Client::Client(tcp::socket& socket):socket(socket), client_id(std::vector<uint8_t>(16,0)), version(100), request_code(0), payload_size(0),
client_name(""),client_private_key("") ,header_buffer(std::vector<uint8_t>()), file_name(""), payload(std::vector<uint8_t>()) {

    get_data_from_transfer_file();
}

Client::~Client() {
    socket.close();
}

void Client::get_data_from_me_file() {
    std::vector<std::string> data= get_file_data("me.info");
    if (data.size() < 3) {
        return;
    }
    client_name = data[0];
    std::cout << "Client name: " << data[0] << std::endl;
    uuid = boost::uuids::string_generator()(data[1]);
   // client_id = std::vector<uint8_t>(uuid.begin(), uuid.end());
    std::cout << "Client ID: " << data[1] << std::endl;

    for (std::size_t i = 2; i < data.size(); i++) {
        client_private_key += data[i];
    }
    cryptoKey = CryptoPPKey(client_private_key);

    std::cout << "Client private key: " << client_private_key << std::endl;
}




//void Client::get_name_255(std::string name){
//    client_name.clear();
//    client_name.insert(client_name.begin(), name.begin(), name.end());
//    client_name.insert(client_name.end(), "\0", "\0" + 1);
//    client_name.insert(client_name.end(), 255 - name.size(), 0);
//}

void Client::get_data_from_transfer_file() {

    std::vector<std::string> data= get_file_data("transfer.info");

    if (data.size() < 3) {
        std::cerr << "Error: transfer file is empty" << std::endl;
        return;
    }else
        std::cout << "reading from transfer file" << std::endl;

    client_name = data[1];


//   handle_send_opCode(REGISTER);
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

std::vector<uint8_t> Client::get_name_255(std::string name_str){
    std::vector<uint8_t> name_bytes;
    name_bytes.insert(name_bytes.begin(), name_str.begin(), name_str.end());
    name_bytes.insert(name_bytes.end(), "\0", "\0" + 1);
    name_bytes.insert(name_bytes.end(), 255 - name_str.size()-1, 0);
    return name_bytes;
}
std::vector<std::string> Client::get_file_data(std::string file_name) {
    std::ifstream file(file_name);
    if (!file.is_open()) {
        return {};
    }
    std::vector<std::string> data;
    while (!file.eof()) {
        std::string line;
        std::getline(file, line);
        data.push_back(line);
    }
    return data;
}

void Client::start() {
    try {
        send_data_by_chunks();
        std::cout << "Header sent successfully." << std::endl;

        std::vector<uint8_t> response = receive_data_by_chunks();
        std::cout << "Response received: " << std::string(response.begin(), response.end()) << std::endl;
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

void Client::parse_response(std::vector<uint8_t> response){
    uint8_t version1 = static_cast<uint8_t>(response[0]);
    uint16_t op_code = ntohs(*reinterpret_cast<const uint16_t*>(&response[1]));
    payload_size = ntohl(*reinterpret_cast<const uint32_t*>(&response[3]));
    payload = std::vector<uint8_t>(response.begin() + 7, response.end());
    handel_response_opCode(op_code);
}

void Client::handel_response_opCode(uint16_t op_code) {
    switch(op_code) {
        case REGISTER_OK:{
            if (payload.size() != 16) {
                std::cerr << "Error: Invalid UUID length" << std::endl;
                handle_send_opCode(REGISTER_NOK);
            }
            std::copy(payload.begin(), payload.end(), uuid.begin());

            std::cout << "REGISTER OK: " << uuid << std::endl;
            create_me_file();
            handle_send_opCode(SENDING_PUBLIC_KEY);
            break;
        }
        case REGISTER_NOK:
            connection_request_count++;
            if (connection_request_count < 3) {
                std::cout << "REGISTER NOK" << std::endl;
                handle_send_opCode(REGISTER);
            } else {
                std::cout << "REGISTER NOK after 3 attempts" << std::endl;
                return;
            }
            break;
        case RECEIVE_AES_KEY:{
            std::vector<uint8_t> encrypted_aes_key(payload.begin()+16, payload.end());
            cryptoKey.decrypt_aes_key(encrypted_aes_key);
            handle_send_opCode(SENDING_FILE);
            break;
        }
        case CRC_RECEIVE_OK:
            break;
        case MESSAGE_RECEIVE_OK:
            break;
        case RECONNECT_OK_SEND_AES:
            break;
        case RECONNECT_NOK:
            break;
        case GENERAL_ERROR:
            break;

    }
    start();
}

void Client::handle_send_opCode(uint16_t op_code) {
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
            get_data_from_me_file();
            add_to_payload(get_name_255(client_name));
            std::cout << "Reconnecting" << std::endl;
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
            payload = std::vector<uint8_t>(file_name.begin(), file_name.end());
            std::cout << "CRC NOT OK" << std::endl;
            break;
        case CRC_TERMINATION:
            payload = std::vector<uint8_t>(file_name.begin(), file_name.end());
            std::cout << "CRC termination" << std::endl;
            break;
        default:
            std::cerr << "Unknown op_code: " << op_code << std::endl;
            break;
    }
    request_code = op_code;
    payload_size = uint32_t(payload.size());

    load_header();
    //send_data_by_chunks();
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


//    std::string header_str = client_id;
//    header_str += version;
//    header_str += op_code;
//    header_str += payload_size;
//
//    header_buffer = std::vector<char>(header_str.begin(), header_str.end());
//    header_buffer.insert( header_buffer.end(), payload.begin(), payload.end());


}


void Client::laod_file_content() {

    std::ifstream file(file_name, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open the file" << std::endl;
        return;
    }

    file.seekg(0, std::ios::end);
    file_content.resize(file.tellg());
    file.seekg(0, std::ios::beg);

    file.read(reinterpret_cast<char*>(file_content.data()), file_content.size());
    file.close();

}


////every two characters in the string is hex number that represent a byte
//std::vector<uint8_t> Client::parse_client_id(std::string client_id) {
//    std::vector<uint8_t> id;
//    for (std::size_t i = 0; i < client_id.size(); i += 2) {
//        std::string byte_str = client_id.substr(i, 2);
//        id.push_back(static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16)));
//    }
//    return id;
//}

void Client::manage_client_flow(){
    std::cout << "Waiting for response" << std::endl;

//    handle_send_opCode(SENDING_PUBLIC_KEY);
//    handle_send_opCode(RECONNECT);
//    handle_send_opCode(SENDING_FILE);
//    handle_send_opCode(CRC_OK);
//    handle_send_opCode(CRC_NOT_OK);
//    handle_send_opCode(CRC_TERMINATION);
}




void Client::manage_encryption() {
    std::vector<uint8_t> encrypted_aes_key(payload.begin()+16, payload.end());
    cryptoKey.decrypt_aes_key(encrypted_aes_key);
    handle_send_opCode(SENDING_FILE);
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
    std::ofstream file("me.info", std::ios::trunc);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open the file" << std::endl;
        return;
    }

    file << client_name << std::endl;
    file << uuid << std::endl;
    file << cryptoKey.get_private_key() << std::endl;
    file.close();

}