#include "crypto.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <fstream>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <vector>

namespace netdisk {

namespace {

std::string toHex(const unsigned char *data, std::size_t length)
{
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < length; ++i) {
        stream << std::setw(2) << static_cast<unsigned int>(data[i]);
    }
    return stream.str();
}

bool fromHex(const std::string &hex, std::vector<unsigned char> &data)
{
    if (hex.size() % 2 != 0) {
        return false;
    }
    data.clear();
    data.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        unsigned int value = 0;
        std::istringstream stream(hex.substr(i, 2));
        stream >> std::hex >> value;
        if (stream.fail()) {
            return false;
        }
        data.push_back(static_cast<unsigned char>(value));
    }
    return true;
}

} // namespace

bool randomHex(std::size_t byteCount, std::string &hex, std::string &error)
{
    std::vector<unsigned char> bytes(byteCount);
    if (byteCount != 0 && RAND_bytes(bytes.data(), static_cast<int>(byteCount)) != 1) {
        error = "RAND_bytes failed";
        return false;
    }
    hex = toHex(bytes.data(), bytes.size());
    return true;
}

bool randomToken(std::string &token, std::string &error)
{
    return randomHex(32, token, error);
}

bool hashPassword(const std::string &password, std::string &encoded,
                  std::string &error)
{
    std::string salt;
    if (!randomHex(16, salt, error)) {
        return false;
    }

    std::vector<unsigned char> saltBytes;
    if (!fromHex(salt, saltBytes)) {
        error = "generated invalid password salt";
        return false;
    }

    constexpr std::uint64_t n = 1ULL << 15;
    constexpr std::uint64_t r = 8;
    constexpr std::uint64_t p = 1;
    unsigned char derived[32] = {0};
    if (EVP_PBE_scrypt(password.c_str(), password.size(), saltBytes.data(),
                       saltBytes.size(), n, r, p, 64ULL * 1024ULL * 1024ULL,
                       derived, sizeof(derived)) != 1) {
        error = "EVP_PBE_scrypt failed";
        return false;
    }

    encoded = "scrypt$32768$8$1$" + salt + "$" + toHex(derived, sizeof(derived));
    return true;
}

bool verifyPassword(const std::string &password, const std::string &encoded,
                    bool &matched, std::string &error)
{
    matched = false;
    const std::string prefix = "scrypt$32768$8$1$";
    if (encoded.compare(0, prefix.size(), prefix) != 0) {
        error = "unsupported password hash format";
        return false;
    }

    const std::size_t separator = encoded.find('$', prefix.size());
    if (separator == std::string::npos) {
        error = "invalid password hash";
        return false;
    }
    const std::string saltHex = encoded.substr(prefix.size(), separator - prefix.size());
    const std::string expectedHex = encoded.substr(separator + 1);
    std::vector<unsigned char> saltBytes;
    std::vector<unsigned char> expected;
    if (!fromHex(saltHex, saltBytes) || !fromHex(expectedHex, expected) ||
        expected.size() != 32) {
        error = "invalid password hash encoding";
        return false;
    }

    unsigned char actual[32] = {0};
    if (EVP_PBE_scrypt(password.c_str(), password.size(), saltBytes.data(),
                       saltBytes.size(), 1ULL << 15, 8, 1,
                       64ULL * 1024ULL * 1024ULL, actual, sizeof(actual)) != 1) {
        error = "EVP_PBE_scrypt failed";
        return false;
    }

    unsigned char difference = 0;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        difference |= static_cast<unsigned char>(actual[i] ^ expected[i]);
    }
    matched = difference == 0;
    return true;
}

bool sha256File(const std::string &path, std::string &hex, std::string &error)
{
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input) {
        error = "cannot open file for SHA-256: " + path;
        return false;
    }

    EVP_MD_CTX *context = EVP_MD_CTX_new();
    if (context == nullptr) {
        error = "EVP_MD_CTX_new failed";
        return false;
    }

    bool success = EVP_DigestInit_ex(context, EVP_sha256(), nullptr) == 1;
    std::vector<char> buffer(64 * 1024);
    while (success && input.good()) {
        input.read(buffer.data(), buffer.size());
        const std::streamsize count = input.gcount();
        if (count > 0) {
            success = EVP_DigestUpdate(context, buffer.data(),
                                       static_cast<std::size_t>(count)) == 1;
        }
    }

    unsigned char digest[EVP_MAX_MD_SIZE] = {0};
    unsigned int digestLength = 0;
    if (success) {
        success = EVP_DigestFinal_ex(context, digest, &digestLength) == 1;
    }
    EVP_MD_CTX_free(context);

    if (!success) {
        error = "SHA-256 calculation failed";
        return false;
    }
    hex = toHex(digest, digestLength);
    return true;
}

} // namespace netdisk
