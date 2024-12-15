//
// Created by lior3 on 25/09/2024.
//

// CryptoPPKey.cpp

#include "CryptoPPKey.h"
#include <iostream>

// Constants for key generation
#define MODULUS_BITS_SIZE 1024 // Size of the RSA modulus in bits
#define DEFAULT_KEY_LENGTH 32   // Default length for AES key

using namespace CryptoPP;

/**
 * @brief Constructor for CryptoPPKey.
 *
 * This constructor initializes the CryptoPPKey object. It first checks if the private key file
 * already exists. If it does, the private key is loaded from the file and decoded from Base64,
 * and the corresponding public key is generated. If the file does not exist, a new RSA key pair
 * is generated, and the private key is saved to a file.
 */
CryptoPPKey::CryptoPPKey() {
    // Check if the private key file already exists
    if (std::filesystem::exists("priv.key")) {
        // Load the private key from the file and decode it from Base64
        StringSource ss(get_private_key_from_private_file(), true, new Base64Decoder);
        privateKey.Load(ss); // Load the private key
        publicKey = CryptoPP::RSA::PublicKey(privateKey); // Generate the corresponding public key
        checksum = 0; // Initialize checksum
        crc32 = boost::crc_32_type(); // Initialize CRC32 calculator
        std::cout << "RSA Key Pair loaded successfully.\n"; // Notify successful key loading
        return; // Exit the constructor
    }

    // Generate a new RSA key pair if the file does not exist
    CryptoPP::AutoSeededRandomPool rng; // Random number generator
    privateKey.Initialize(rng, MODULUS_BITS_SIZE); // Initialize the private key
    publicKey = CryptoPP::RSA::PublicKey(privateKey); // Generate the public key
    std::cout << "RSA Key Pair generated successfully.\n"; // Notify successful key generation
    checksum = 0; // Initialize checksum
    crc32 = boost::crc_32_type(); // Initialize CRC32 calculator
    make_private_file(); // Create a file to store the private key
}

// Destructor for CryptoPPKey
CryptoPPKey::~CryptoPPKey() {

}
/**
 * @brief Retrieves the public key in Base64 format.
 *
 * This function encodes the public key in Base64 format and returns it as a vector of uint8_t.
 * It uses the Crypto++ library's Base64Encoder to perform the encoding.
 *
 * @return A vector of uint8_t containing the Base64-encoded public key.
 */
std::vector<uint8_t> CryptoPPKey::get_public_key_base64() {
    std::string publicKeyStr; // String to hold the Base64-encoded public key

    // Base64 encode the public key (DER format)
    Base64Encoder encoder(new StringSink(publicKeyStr));
    publicKey.DEREncode(encoder);
    encoder.MessageEnd();

    // Convert the Base64 string to a vector of uint8_t
    std::vector<uint8_t> publicKeyBytes(publicKeyStr.begin(), publicKeyStr.end());

    return publicKeyBytes;
}

/**
 * @brief Get the Private Key in Base64 format.
 *
 * This function encodes the private key in Base64 format and returns it as a string.
 * It uses the Crypto++ library's Base64Encoder to perform the encoding.
 *
 * @return A string containing the Base64-encoded private key.
 */
std::string CryptoPPKey::get_private_key() {
    std::string privateKeyStr; // String to hold the Base64-encoded private key
    Base64Encoder encoder(new StringSink(privateKeyStr)); // Base64 encode the private key
    privateKey.DEREncode(encoder); // Encode the private key in DER format
    encoder.MessageEnd(); // Signal the end of the encoding process
    return privateKeyStr; // Return the Base64-encoded private key
}


/**
 * @brief Decrypts an AES key using the private RSA key.
 *
 * This function takes an encrypted AES key, decrypts it using the private RSA key,
 * and extracts the AES key and IV from the decrypted data.
 *
 * @param encrypted_aes_key A vector of uint8_t containing the encrypted AES key.
 * @throws std::runtime_error if the AES key decryption fails.
 */
void CryptoPPKey::decrypt_aes_key(const std::vector<uint8_t>& encrypted_aes_key) {
    AutoSeededRandomPool rng; // Random number generator

    // Decrypt AES key using private RSA key
    RSAES_OAEP_SHA_Decryptor decryptor(privateKey);
    std::string decrypted_aes_key;
    std::cout << "Encrypted AES key length: " << encrypted_aes_key.size() << std::endl;

    try {
        StringSource ss1(
                encrypted_aes_key.data(),
                encrypted_aes_key.size(),
                true,
                new PK_DecryptorFilter(rng, decryptor, new StringSink(decrypted_aes_key))
        );
    } catch (const Exception& e) {
        std::cerr << "Decryption failed: " << e.what() << std::endl;
        throw std::runtime_error("AES key decryption failed");
    }

    // Extract the AES key and IV from the decrypted data
    aes_key = SecByteBlock((const byte*)decrypted_aes_key.data(), DEFAULT_KEY_LENGTH);
    aes_iv = SecByteBlock((const byte*)decrypted_aes_key.data() + DEFAULT_KEY_LENGTH, AES::BLOCKSIZE);
}

/**
 * @brief Encrypts a file using AES and returns the encrypted content.
 *
 * This function encrypts the provided file content using AES encryption in CBC mode.
 * It first ensures that the AES key and IV are set, calculates the CRC32 checksum of the file content,
 * and then performs the encryption. The encrypted content is returned as a vector of uint8_t.
 *
 * @param file_content A vector of uint8_t containing the content of the file to be encrypted.
 * @return A vector of uint8_t containing the encrypted content of the file.
 * @throws std::runtime_error if the AES key or IV is not set, or if an error occurs during encryption.
 */
std::vector<uint8_t> CryptoPPKey::encrypt_file(const std::vector<uint8_t>& file_content) {
    // Ensure the AES key and IV are set
    if (aes_key.size() == 0 || aes_iv.size() == 0) {
        throw std::runtime_error("AES key or IV is not set.");
        return {};
    }
    // Calculate the CRC32 checksum of the file content
    calculate_checksum(file_content);

    AutoSeededRandomPool rng; // Random number generator
    std::vector<uint8_t> encrypted_data; // Vector to hold the encrypted data

    // Set up AES encryption in CBC mode
    CBC_Mode<AES>::Encryption aesEncryptor;
    aesEncryptor.SetKeyWithIV(aes_key, aes_key.size(), aes_iv);

    // Encrypt the file content
    std::string plain(reinterpret_cast<const char*>(file_content.data()), file_content.size()); // Convert file content to string
    std::string cipher; // String to hold the encrypted content

    try {
        StringSource ss(plain, true,
                        new StreamTransformationFilter(aesEncryptor,
                                                       new StringSink(cipher)
                        )
        );
    } catch (const Exception& e) {
        std::cerr << "Error during encryption: " << e.what() << std::endl; // Log encryption error
        throw;
    }

    // Copy encrypted data to std::vector<uint8_t>
    encrypted_data.assign(cipher.begin(), cipher.end());

    return encrypted_data; // Return the encrypted data
}

/**
 * @brief Calculates the CRC32 checksum of the given file content.
 *
 * This function computes the CRC32 checksum for the provided file content
 * using a predefined CRC table. The result is stored in the `checksum` member variable.
 *
 * @param file_content A vector of uint8_t containing the content of the file to be checksummed.
 */
void CryptoPPKey::calculate_checksum(const std::vector<uint8_t>& file_content) {
    size_t n = file_content.size(); // Get the size of the file content
    unsigned int v = 0, c = 0; // Variables for CRC calculation
    unsigned long s = 0; // Variable to hold the checksum
    unsigned int tabidx; // Index for the CRC table

    for (int i = 0; i < n; i++) {
        tabidx = (s >> 24) ^ (unsigned char)file_content[i]; // Calculate table index
        s = UNSIGNED((s << 8)) ^ crctab[0][tabidx]; // Update checksum
    }

    while (n) {
        c = n & 0377; // Get the lowest 8 bits of n
        n = n >> 8; // Shift n right by 8 bits
        s = UNSIGNED(s << 8) ^ crctab[0][(s >> 24) ^ c]; // Update checksum
    }
    checksum = (unsigned long)UNSIGNED(~s); // Finalize checksum
}

/**
 * @brief Verifies the provided checksum against the stored checksum.
 *
 * This function compares the provided checksum with the stored checksum
 * and returns true if they match, false otherwise.
 *
 * @param checksum The checksum to verify.
 * @return True if the provided checksum matches the stored checksum, false otherwise.
 */
bool CryptoPPKey::verify_checksum(uint32_t checksum) {
    return checksum == this->checksum; // Compare provided checksum with stored checksum
}

/**
 * @brief Creates a file to store the private key.
 *
 * This function creates a file named "priv.key" and writes the Base64-encoded
 * private key to it. If the file cannot be opened, an error message is printed.
 */
void CryptoPPKey::make_private_file() {
    std::ofstream priv_file("priv.key"); // Open the private key file for writing
    if (!priv_file.is_open()) { // Check if the file was opened successfully
        std::cerr << "Failed to open file: priv.key" << std::endl; // Log an error message if not
        return; // Exit the function if the file could not be opened
    }
    priv_file << get_private_key(); // Write the Base64-encoded private key to the file
    priv_file.close(); // Close the file after writing
}

/**
 * @brief Retrieves the private key from the private key file.
 *
 * This function reads the private key from the file named "priv.key" and returns it
 * as a string. If the file cannot be opened, an error message is printed and an empty
 * string is returned.
 *
 * @return A string containing the private key read from the file, or an empty string if the file cannot be opened.
 */
std::string CryptoPPKey::get_private_key_from_private_file() {
    std::string privateKeyStr; // String to hold the private key
    std::ifstream priv_file("priv.key"); // Open the private key file
    if (!priv_file.is_open()) { // Check if the file was opened successfully
        std::cerr << "Failed to open file: priv.key" << std::endl; // Log an error message if not
        return ""; // Return an empty string if the file could not be opened
    }
    std::string line; // String to hold each line read from the file
    while (std::getline(priv_file, line)) { // Read the file line by line
        privateKeyStr += line; // Append each line to the private key string
    }
    priv_file.close(); // Close the file after reading
    return privateKeyStr; // Return the private key string
}

