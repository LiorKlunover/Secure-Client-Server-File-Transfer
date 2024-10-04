//
// Created by lior3 on 25/09/2024.
//

// CryptoPPKey.cpp

#include "CryptoPPKey.h"
#include <iostream>

#define MODULUS_BITS_SIZE 1024
#define DEFAULT_KEY_LENGTH 32

using namespace CryptoPP;

CryptoPPKey::CryptoPPKey() {
    CryptoPP::AutoSeededRandomPool rng;
    privateKey.Initialize(rng, MODULUS_BITS_SIZE);
    publicKey = CryptoPP::RSA::PublicKey(privateKey);
    std::cout << "RSA Key Pair generated successfully.\n";
    checksum = 0;
    crc32 = boost::crc_32_type();
}
CryptoPPKey::CryptoPPKey(const std::string& private_key) {
    // Load private key from string
    StringSource ss(private_key, true, new Base64Decoder);
    privateKey.Load(ss);
    publicKey = CryptoPP::RSA::PublicKey(privateKey);
    checksum = 0;
    crc32 = boost::crc_32_type();
}

CryptoPPKey::~CryptoPPKey() {
    // Destructor
}

std::vector<uint8_t> CryptoPPKey::get_public_key_base64() {
    // String to hold the Base64-encoded public key
    std::string publicKeyStr;

    // Base64 encode the public key (DER format)
    Base64Encoder encoder(new StringSink(publicKeyStr));
    publicKey.DEREncode(encoder);
    encoder.MessageEnd();

    // Convert the Base64 string to a vector of uint8_t
    std::vector<uint8_t> publicKeyBytes(publicKeyStr.begin(), publicKeyStr.end());

    return publicKeyBytes;
}


void CryptoPPKey::save_key_to_file(const std::string& privateKeyFile) {
    FileSink privFile(privateKeyFile.c_str());
    privateKey.DEREncode(privFile);
    privFile.MessageEnd();
}

void CryptoPPKey::load_key_from_file(const std::string& privateKeyFile) {
    FileSource privFile(privateKeyFile.c_str(), true);
    privateKey.BERDecode(privFile);
}

// Get Private Key in Base64 format
std::string CryptoPPKey::get_private_key() {
    std::string privateKeyStr;
    Base64Encoder encoder(new StringSink(privateKeyStr));
    privateKey.DEREncode(encoder);
    encoder.MessageEnd();
    return privateKeyStr;
}
CryptoPP::RSA::PrivateKey CryptoPPKey::get_private_key_object() {
    return privateKey;
}

//// Set AES key and IV from the decrypted AES key
//void CryptoPPKey::set_aes_key(const std::string& decrypted_aes_key) {
//    aes_key = SecByteBlock((const byte*)decrypted_aes_key.data(), AES::DEFAULT_KEY_LENGTH);
//    aes_iv = SecByteBlock((const byte*)decrypted_aes_key.data() + AES::DEFAULT_KEY_LENGTH, AES::BLOCKSIZE);
//}


// Decrypt data using AES
std::string CryptoPPKey::decrypt_data(const std::string& encrypted_data) {
    AutoSeededRandomPool rng;
    CBC_Mode<AES>::Decryption aesDecryptor;
    aesDecryptor.SetKeyWithIV(aes_key, aes_key.size(), aes_iv);

    std::string decrypted_data;

    StringSource ss1(encrypted_data, true,
                     new StreamTransformationFilter(aesDecryptor,
                                                    new StringSink(decrypted_data)
                     )
    );

    return decrypted_data;
}

// Function to receive and decrypt AES key
void CryptoPPKey::decrypt_aes_key(const std::vector<uint8_t>& encrypted_aes_key) {
    AutoSeededRandomPool rng;

    // Decrypt AES key using private RSA key
    RSAES_OAEP_SHA_Decryptor decryptor(privateKey);
    std::string decrypted_aes_key;

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

    std::cout << "AES Key and IV received and decrypted successfully.\n";
}

// Function to encrypt a file using AES and return the encrypted content as std::vector<uint8_t>
std::vector<uint8_t> CryptoPPKey::encrypt_file(const std::vector<uint8_t>& file_content) {
    // Ensure the AES key and IV are set
    if (aes_key.size() == 0 || aes_iv.size() == 0) {
        throw std::runtime_error("AES key or IV is not set.");
        return {};
    }
    // Calculate the CRC32 checksum of the file content
    calculate_checksum(file_content);

    AutoSeededRandomPool rng;
    std::vector<uint8_t> encrypted_data;

    // Set up AES encryption in CBC mode
    CBC_Mode<AES>::Encryption aesEncryptor;
    aesEncryptor.SetKeyWithIV(aes_key, aes_key.size(), aes_iv);

    // Encrypt the file content
    std::string plain(reinterpret_cast<const char*>(file_content.data()), file_content.size());
    std::string cipher;

    try {
        StringSource ss(plain, true,
                        new StreamTransformationFilter(aesEncryptor,
                                                       new StringSink(cipher)
                        )
        );
    } catch (const Exception& e) {
        std::cerr << "Error during encryption: " << e.what() << std::endl;
        throw;
    }

    // Copy encrypted data to std::vector<uint8_t>
    encrypted_data.assign(cipher.begin(), cipher.end());

    return encrypted_data;
}
void CryptoPPKey::calculate_checksum(const std::vector<uint8_t>& file_content) {
    crc32.process_bytes(file_content.data(), file_content.size());
    checksum = crc32.checksum();
    std::cout << "Checksum calculated successfully.\n";
    std::cout << "Checksum: " << checksum << std::endl;
}
bool CryptoPPKey::verify_checksum(uint32_t checksum){
    return checksum == this->checksum;
}

//int main(){
//    CryptoPPKey key;
//
//    std::string aes_key = "6\\x14\\xcc\\x06\\x1f\\xf9\\xe8<UTDk\\xe97\\xb05\\xe0C\\x9c4\\xbdjM\\xf8\\xc2\\x88\\x1anF\\xe6\\xbd\\x94\"fz\\x9f\\x1dZ\\xe1\\xfeq\\x9d\\xd7\\rC\\xb6\\xff?\\x06$\\x14QQ\\xe0\\t\\xd8w\\xc5\\x12\\xe2\\xe8\\xabl\\xd5\\x9f\\x0ft\\xfeM\\xf3}6t\\xcf`a\\xab\\xdc\\x02@D\\x1c)#\\x0e\\xa1I\\xfei|\\xe8\\x890|\\x8d\\xfc\\x8a\\xfc\\xa2\\x9a\\x82\\xe3\\x113\\xfb\\x88=\\xef\\xcb\\x0fX?N\\x033\\xac\\xa5\\xccxjd\\x12mYClv\\x14\\x96c\\t\\xe5e+\\x1f%\\xe1\\xa9\\xf1\\xc1\\xd7\\xc8\\xb2tI\\xb7\\x9f\\xb3\\x1b\\xaa\\xdb\\xd1\\xdc\\xa9S.\\x88\\xe4x\\xbe?J\\xb8[\\xf3\\x1f\\xefJ4\\xaei1\\xf7\\x0fm\\xcc\\xe0\\xa4\\x99\\xd0\\x91\\x10\\x80A\\x02\\xe1\\xbb\\x0b\\x03H\\xd4\\x8c\\xe2\\x19\\n\\x90D)$0`\\x1d\\t\\xf3\\xf3WC\\x03\\xd2\"!\\xfe\\xef\\xc4\\xb3\\x1a\\xc8\\xb1\\xbf\\x95<\\x88\\xf4\\x82\\xe7\\tB\\x97\\n\\x0eY\\xeb:\\\\L\\xfe^\\x89\\xc6\\xec@\\xec\\xa1\\xcf\\xd7\\x18\\xf8\\xc7\\xff\\xf7\\x7f.\\x1e\\x01Jy";
//    std::vector<uint8_t> aes_key_vector(aes_key.begin(), aes_key.end());
//    key.generate_RSA_key_pair();
//    key.decrypt_aes_key(aes_key_vector);
//    return 0;
//
//}



