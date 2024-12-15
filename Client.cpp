//
// Created by lior3 on 21/09/2024.
//


#include "Client.h"
// Constructor to initialize the Client class
// This constructor takes a reference to a TCP socket and initializes various client-related variables such as client_id, version, request_op_code, etc.
// It attempts to read data from "transfer.info" and check if "me.info" exists for reconnection, otherwise registers a new client.

Client::Client(tcp::socket& socket)
        : socket(socket), client_id(std::vector<uint8_t>(16,0)), version(3), request_op_code(0), payload_size(0),
          client_name("") , header_buffer(std::vector<uint8_t>()), file_path(""), payload(std::vector<uint8_t>()), file_name("") {

    try {
        // Read the transfer info to initialize client name and file path
        get_data_from_transfer_file();

        // Check if "me.info" exists (used for reconnecting an existing client)
        if (std::filesystem::exists("me.info")) {
            std::cout << "Found existing client info, attempting to reconnect" << std::endl;
            request_op_code = RECONNECT;  // Set request to reconnect
            get_data_from_me_file();      // Get saved client data (name, UUID)
            std::cout << "Reconnecting..." << std::endl;
        } else {
            // If no previous client data, register as a new client
            std::cout << "No existing client info found, registering as new client" << std::endl;
            request_op_code = REGISTER;  // Set request to register
            std::cout << "Registering..." << std::endl;
        }
        // Send request op_code (either REGISTER or RECONNECT) to the server
        handle_sending_opCode(request_op_code);
    } catch (const std::exception& e) {
        std::cerr << "Initialization Error: " << e.what() << std::endl;
    }
}

// Destructor for the Client class
// Closes the socket and checks for any fatal errors during client operations.
Client::~Client() {
    try {
        // Close the TCP socket connection
        socket.close();
        std::cout << "Socket closed successfully." << std::endl;
    } catch (const boost::system::system_error& e) {
        std::cerr << "Error while closing socket: " << e.what() << std::endl;
    }

    // Handle any fatal error messages
    try {
        if (!fatal_error_message.empty()) {
            throw std::runtime_error(fatal_error_message);
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
    }
}

// Reads data from the "me.info" file (used for reconnecting clients).
// The file contains client information such as name and UUID.
void Client::get_data_from_me_file() {
    std::vector<std::string> data = get_file_data("me.info");
    if (data.size() < 3) {
        throw std::runtime_error("Invalid me.info file format");
    }

    // First line is the client name
    client_name = data[0];
    std::cout << "Client name: " << client_name << std::endl;

    // Second line contains the client UUID in string format
    try {
        client_uuid = boost::uuids::string_generator()(data[1]);
        std::cout << "Loaded client info - Client name: " << client_name << ", UUID: " << client_uuid << std::endl;
    } catch (const std::exception& e) {
        throw std::runtime_error("Error parsing UUID from me.info file");
    }
}

// Reads data from the "transfer.info" file (used for file transfer).
// Extracts client name and file path for the file to be transferred.
void Client::get_data_from_transfer_file() {

    std::cout << "Loading transfer info" << std::endl;
    std::vector<std::string> data = get_file_data("transfer.info");

    if (data.size() < 3) {
        throw std::runtime_error("Invalid transfer.info file format");
    } else {
        std::cout << "Reading from transfer file..." << std::endl;
    }

    // The second line is the client name
    client_name = data[1];
    // The third line is the file path
    file_path = data[2];

    std::cout << "Loaded transfer info - Client name: " << client_name << ", File path: " << file_path << std::endl;
}


// Pads a string to a fixed length of 255 bytes for consistency in packet transmission.
// Returns a byte array padded with zeroes to match the required size.
std::vector<uint8_t> Client::pad_string_to_255(const std::string& name_str) {
    std::vector<uint8_t> name_bytes(255, 0);  // Initialize the byte array with zeros
    size_t name_length = name_str.size();

    // Ensure the string is not longer than 254 characters
    if (name_length > 254) {
        std::cerr << "Error: Name exceeds 254 characters. Truncating..." << std::endl;
        name_length = 254;  // Truncate the string if too long
    }

    // Copy the characters of the string into the byte array
    std::copy(name_str.begin(), name_str.begin() + name_length, name_bytes.begin());
    name_bytes[name_length] = 0;  // Null-terminate the string
    return name_bytes;
}



// Reads data from the specified file and returns the contents as a vector of strings.
// Each line of the file is treated as a separate string.
std::vector<std::string> Client::get_file_data(const std::string& file_name) {
    std::ifstream file(file_name);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file '" << file_name << "'." << std::endl;
        return {};
    }

    std::vector<std::string> data;
    std::string line;

    // Read each line of the file and store it in the data vector
    while (std::getline(file, line)) {
        data.push_back(line);
    }
    file.close();
    return data;
}



// Starts the client workflow.
// Sends the header to the server, receives the response, and manages further steps based on the server's response.
void Client::start() {
    try {
        std::cout << "Sending header to the server - Op Code: " << request_op_code << std::endl;
        send_data_by_chunks();  // Send header in chunks
        std::cout << "Header sent successfully!" << std::endl;

        std::cout << "Receiving response..." << std::endl;
        std::vector<uint8_t> response = receive_data_by_chunks();  // Receive response from server
        std::cout << "Response received successfully!" ;

        // Parse the response from the server
        parse_response(response);

        // Continue client workflow based on server response
        manage_client_flow();
    } catch (const std::exception& e) {
        std::cerr << "Error during client start: " << e.what() << std::endl;
    }
}

// Manages the client workflow after parsing the server's response.
// Sends the next request operation code based on the server's response.
void Client::manage_client_flow() {
    try {
        // If the received operation code is handled successfully, continue to send another request
        if (handle_received_opCode(received_op_code)) {
            handle_sending_opCode(request_op_code);  // Send the next request op code
            start();  // Restart the client workflow
        }

    } catch (const std::exception& e) {
        std::cerr << "Error during client flow: " << e.what() << std::endl;
    }
}

// Sends data in chunks over the TCP socket to the server.
// Uses a fixed chunk size to ensure consistent data transmission.
void Client::send_data_by_chunks() {
    uint32_t total_bytes_sent = 0;
    uint32_t max_length = 1024;  // Maximum chunk size

    try {
        // Continue sending data until all data in the header_buffer is sent
        while (total_bytes_sent < header_buffer.size()) {
            // Calculate how many bytes to send in the current chunk
            uint32_t bytes_to_send = std::min(max_length, uint32_t(header_buffer.size() - total_bytes_sent));
            total_bytes_sent += boost::asio::write(socket, boost::asio::buffer(header_buffer.data() + total_bytes_sent, bytes_to_send));
        }
    } catch (const boost::system::system_error& e) {
        std::cerr << "Error during write: " << e.what() << std::endl;
        return;
    }
}

// Receives data from the server in chunks and returns the data as a vector of bytes.
// Handles potential errors during data reception.
std::vector<uint8_t> Client::receive_data_by_chunks() {
    std::vector<uint8_t> response;
    try {
        std::vector<uint8_t> buffer(1024);  // Buffer to hold received data
        boost::system::error_code error;

        // Keep reading data until there is no more data to receive
        while (true) {
            size_t bytes_transferred = socket.read_some(boost::asio::buffer(buffer), error);

            if (error == boost::asio::error::eof) {
                break;  // End of file reached, stop receiving data
            } else if (error) {
                throw boost::system::system_error(error);
            }

            // Append received data to the response vector
            response.insert(response.end(), buffer.begin(), buffer.begin() + bytes_transferred);

            if (bytes_transferred < buffer.size()) {
                break;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error during data reception: " << e.what() << std::endl;
        throw;
    }
    return response;
}

// Parses the response received from the server to extract relevant information
void Client::parse_response(const std::vector<uint8_t>& response) {
    // Check if the response is long enough to contain the required data
    if (response.size() < 7) {
        std::cerr << "Error: Response is too short." << std::endl;
        return; // Early exit on invalid response size
    }

    // Extract version, operation code, and payload size from the response
    uint8_t version1 = response[0];
    received_op_code = ntohs(*reinterpret_cast<const uint16_t*>(&response[1]));
    std::cout << " - Op Code: " << received_op_code << std::endl;
    payload_size = ntohl(*reinterpret_cast<const uint32_t*>(&response[3]));
    // Extract the payload from the response, starting from byte 7
    payload = std::vector<uint8_t>(response.begin() + 7, response.end());
}

// Handles the operation code received from the server and determines the next steps
bool Client::handle_received_opCode(uint16_t op_code) {
    switch(op_code) {
        case REGISTER_OK: { // Handle successful registration
            // Validate the payload length for UUID
            if (payload.size() != 16) {
                std::cerr << "Error: Invalid UUID length" << std::endl;
                request_op_code = REGISTER_NOK; // Set request code for failed registration
            } else {
                // Copy the received UUID from the payload
                std::copy(payload.begin(), payload.end(), client_uuid.begin());
                std::cout << "REGISTER OK, UUID: " << client_uuid << std::endl;

                try {
                    // Create a file to store client information
                    create_me_file();
                    std::cout << "Me file created" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "Me file creation failed" << std::endl;
                    std::cerr << "Error: " << e.what() << std::endl;
                    request_op_code = TERMINATE_CONNECTION; // Set termination request due to error
                    break;
                }
                request_op_code = SENDING_PUBLIC_KEY; // Proceed to send public key
            }
            break;
        }
        case REGISTER_NOK: // Handle failed registration
            connection_request_count++;
            if (connection_request_count < 4) {
                std::cout << "REGISTER NOK" << std::endl;
                std::cout << "Trying to register again..." << std::endl;
                request_op_code = REGISTER; // Retry registration
            } else {
                std::cout << "REGISTER NOK after 3 attempts" << std::endl;
                std::cout << "Terminating connection..." << std::endl;
                fatal_error_message = "Registration failed after 3 attempts"; // Log fatal error
                request_op_code = TERMINATE_CONNECTION; // Set termination request
            }
            break;
        case RECEIVE_AES_KEY: { // Handle received AES key
            std::cout << "Analyzing AES key..." << std::endl;

            // Extract the encrypted AES key from the payload
            std::vector<uint8_t> encrypted_aes_key(payload.begin() + 16, payload.end());
            try {
                // Decrypt the AES key using the crypto key
                crypto_key.decrypt_aes_key(encrypted_aes_key);
                std::cout << "AES key decrypted successfully" << std::endl;
            } catch (const std::exception& e) {
                std::cout << "AES key decryption failed" << std::endl;
                std::cerr << "Error: " << e.what() << std::endl;
                request_op_code = TERMINATE_CONNECTION; // Set termination request due to error
                break;
            }
            request_op_code = SENDING_FILE; // Proceed to send the file
            break;
        }
        case FILE_RECEIVE_OK_AND_CRC: { // Handle file receipt confirmation and CRC check
            std::cout << "Checking CRC32 checksum..." << std::endl;
            uint32_t checksum = ntohl(*reinterpret_cast<const uint32_t*>(&payload[275])); // Extract checksum
            // Verify the checksum using the crypto key
            if (crypto_key.verify_checksum(checksum)) {
                std::cout << "File received successfully" << std::endl;
                request_op_code = CRC_OK; // Set request code for successful CRC
            } else {
                std::cout << "File not received successfully" << std::endl;
                crc_not_ok_count++;
                if (crc_not_ok_count < 4) {
                    fatal_error_message = "File not received successfully CRC32 not equal to expected"; // Log error
                    std::cout << "CRC NOT OK" << std::endl;
                    request_op_code = CRC_NOT_OK; // Set request code for CRC failure
                } else {
                    std::cout << "CRC NOT OK after 4 attempts" << std::endl;
                    fatal_error_message = "File not received successfully CRC32 not equal to expected after 4 attempts"; // Log fatal error
                    std::cout << "Terminating connection..." << std::endl;
                    request_op_code = CRC_TERMINATION; // Set termination request
                }
            }
            break;
        }
        case MESSAGE_RECEIVE_OK: // Handle message receipt confirmation
            if (crc_not_ok_count > 1 && crc_not_ok_count < 4) {
                request_op_code = SENDING_FILE; // Retry sending the file
            } else {
                // Store the error message received in the payload
                fatal_error_message = std::string(payload.begin() + 16, payload.end());
                std::cout << "Message received successfully" << std::endl;
                return false; // Indicate end of processing
            }
            break;
        case RECONNECT_OK_SEND_AES: // Handle reconnection and send AES key
            return handle_received_opCode(RECEIVE_AES_KEY); // Reuse AES key handling logic
            break;
        case RECONNECT_NOK: // Handle failed reconnection
            reconnection_request_count++;
            if (reconnection_request_count < 4) {
                std::cout << "Reconnect failed" << std::endl;
                request_op_code = RECONNECT; // Retry reconnection
            } else {
                std::cout << "Reconnect failed after 3 attempts" << std::endl;
                std::cout << "Terminating connection..." << std::endl;
                request_op_code = TERMINATE_CONNECTION; // Set termination request
            }
            break;
        case GENERAL_ERROR: // Handle general error
            std::cerr << "General error" << std::endl;
            request_op_code = TERMINATE_CONNECTION; // Set termination request
            break;
    }

    return true; // Indicate successful handling of the operation code
}

// Prepares the data to be sent based on the current operation code
void Client::handle_sending_opCode(uint16_t op_code) {
    payload.clear(); // Clear any existing payload data

    switch(op_code) {
        case REGISTER: // Prepare data for registration
            add_to_payload(pad_string_to_255(client_name)); // Add client name to payload
            std::cout << "Preparing registration request for client: " << client_name << std::endl;
            break;

        case SENDING_PUBLIC_KEY: { // Prepare data for sending the public key
            add_to_payload(pad_string_to_255(client_name)); // Add client name to payload
            std::vector<uint8_t> public_key = crypto_key.get_public_key_base64(); // Get public key
            payload.insert(payload.end(), public_key.begin(), public_key.end()); // Add public key to payload

            std::cout << "Sending public key" << std::endl;
            break;
        }

        case RECONNECT: // Prepare data for reconnection
            add_to_payload(pad_string_to_255(client_name)); // Add client name to payload
            break;

        case SENDING_FILE: { // Prepare data for sending a file
            laod_file_content(); // Load the file content into memory
            std::vector<uint8_t> decrypted_file_size = get_file_size(file_content); // Get size of the file
            std::vector<uint8_t> encrypted_file = crypto_key.encrypt_file(file_content); // Encrypt the file content
            std::vector<uint8_t> encrypted_file_size = get_file_size(encrypted_file); // Get size of the encrypted file

            add_to_payload(encrypted_file_size); // Add encrypted file size to payload
            add_to_payload(decrypted_file_size); // Add decrypted file size to payload
            add_to_payload(pad_string_to_255(file_name)); // Add file name to payload
            add_to_payload(encrypted_file); // Add encrypted file to payload

            std::cout << "Preparing to send file: " << file_name << std::endl;
            break;
        }

        case CRC_OK: // Handle successful CRC check
            payload = std::vector<uint8_t>(file_name.begin(), file_name.end()); // Add file name to payload
            break;

        case CRC_NOT_OK: // Handle failed CRC check
            payload = std::vector<uint8_t>(file_name.begin(), file_name.end()); // Add file name to payload
            break;

        case CRC_TERMINATION: // Handle termination due to CRC failure
            payload = std::vector<uint8_t>(file_name.begin(), file_name.end()); // Add file name to payload
            break;

        case TERMINATE_CONNECTION: // Handle connection termination request
            std::cout << "Termination request" << std::endl;
            break;

        default: // Handle unknown operation codes
            std::cerr << "Unknown op_code: " << op_code << std::endl;
            break;
    }

    // Update the payload size after all data has been added
    payload_size = uint32_t(payload.size());

    // Load the header with the current request information
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


// Loads the content of the specified file into memory for sending
void Client::laod_file_content() {
    std::ifstream file(file_path, std::ios::binary); // Open the file in binary mode
    if (!file.is_open()) { // Check if the file was opened successfully
        std::cerr << "Error: Could not open the file" << std::endl; // Log an error message if not
        return; // Exit the function if the file could not be opened
    }

    // Update the file name based on the file path
    file_name = file_path.substr(file_path.find_last_of("/\\") + 1);
    file.seekg(0, std::ios::end); // Seek to the end of the file to determine its size
    file_content.resize(file.tellg()); // Resize the vector to hold the file content
    file.seekg(0, std::ios::beg); // Seek back to the beginning of the file

    // Read the file content into the vector
    file.read(reinterpret_cast<char*>(file_content.data()), file_content.size());
    file.close(); // Close the file after reading
}


// Returns the size of the given file content as a vector of bytes
std::vector<uint8_t> Client::get_file_size(std::vector<uint8_t> file_content) {
    uint32_t file_size = file_content.size(); // Get the size of the file content
    std::vector<uint8_t> size; // Vector to hold the size in byte format

    // Convert the file size into a 4-byte representation
    size.push_back(static_cast<uint8_t>((file_size >> 24) & 0xFF));  // Highest byte
    size.push_back(static_cast<uint8_t>((file_size >> 16) & 0xFF));  // High byte
    size.push_back(static_cast<uint8_t>((file_size >> 8) & 0xFF));   // Low byte
    size.push_back(static_cast<uint8_t>(file_size & 0xFF));          // Lowest byte

    return size; // Return the byte representation of the file size
}

// Adds a vector of data to the payload for sending
void Client::add_to_payload(std::vector<uint8_t> data) {
    payload.insert(payload.end(), data.begin(), data.end()); // Append the data to the payload
}

// Creates a file containing the client's information (name, UUID, private key)
void Client::create_me_file() {
    std::cout << "Creating me file" << std::endl; // Log the start of the file creation
    std::ofstream file("me.info", std::ios::trunc); // Create or truncate the file for writing

    if (!file.is_open()) { // Check if the file was opened successfully
        std::cerr << "Error: Could not open the file" << std::endl; // Log an error message if not
        throw std::invalid_argument("Error: Could not open the file"); // Throw an exception if the file cannot be opened
    }

    std::cout << "me file created" << std::endl; // Log that the file has been created

    // Write the client information to the file
    file << client_name << std::endl; // Write the client name
    file << client_uuid << std::endl; // Write the client UUID
    file << crypto_key.get_private_key() << std::endl; // Write the client's private key
    file.close(); // Close the file after writing
}
