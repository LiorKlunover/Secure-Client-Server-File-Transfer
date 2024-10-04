//
// Created by lior3 on 25/09/2024.
//

#ifndef MAMAN15_CRYPTOPPKEY_H
#define MAMAN15_CRYPTOPPKEY_H

#include <cryptopp/rsa.h>
#include <cryptopp/osrng.h>
#include <cryptopp/base64.h>
#include <cryptopp/files.h>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/filters.h>
#include <boost/crc.hpp>

#include <string>
#include <vector>

class CryptoPPKey {
public:
    CryptoPPKey();
    CryptoPPKey(const std::string& private_key);
    ~CryptoPPKey();


    std::vector<uint8_t> get_public_key_base64();  // Return public key in Base64 format
    std::string get_private_key(); // Return private key in Base64 format
    void load_key_from_file(const std::string& privateKeyFile);  // Load private key from file
    void save_key_to_file(const std::string& privateKeyFile);    // Save private key to file
    CryptoPP::RSA::PrivateKey get_private_key_object();  // Get RSA Private Key

    // function to receive and decrypt AES key
    void decrypt_aes_key(const std::vector<uint8_t>& encrypted_aes_key);

    // AES Encryption/Decryption functions
    void set_aes_key(const std::string& decrypted_aes_key); // Set AES key and IV from decrypted data
    std::vector<uint8_t> encrypt_file(const std::vector<uint8_t>& file_content);  // Encrypt a file using AES
    std::string decrypt_data(const std::string& encrypted_data); // Decrypt data using AES

    // CRC32 checksum functions
    void calculate_checksum(const std::vector<uint8_t>& data);  // Calculate CRC32 checksum
    bool verify_checksum(uint32_t received_checksum);  // Verify CRC32 checksum

private:
    CryptoPP::RSA::PrivateKey privateKey;
    CryptoPP::RSA::PublicKey publicKey;

    CryptoPP::SecByteBlock aes_key;  // AES key
    CryptoPP::SecByteBlock aes_iv;   // AES initialization vector (IV)
    boost::crc_32_type crc32;       // CRC32 checksum
    uint32_t checksum;             // CRC32 checksum value
};


#endif //MAMAN15_CRYPTOPPKEY_H
