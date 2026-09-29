#include "function.h"

#include "../common/crypto.h"
#include "../common/protocol.h"

#include <fcntl.h>
#include <fstream>
#include <cerrno>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace {

std::string baseName(const std::string &path)
{
    const std::size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

bool writeAll(int fd, const unsigned char *data, std::size_t length)
{
    std::size_t written = 0;
    while (written < length) {
        const ssize_t result = write(fd, data + written, length - written);
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            return false;
        }
        written += static_cast<std::size_t>(result);
    }
    return true;
}

} // namespace

bool uploadFile(const ClientContext &context, const std::string &localPath)
{
    struct stat fileStatus;
    if (stat(localPath.c_str(), &fileStatus) != 0 || !S_ISREG(fileStatus.st_mode) ||
        static_cast<std::uint64_t>(fileStatus.st_size) > context.options.maxFileSize) {
        std::cerr << "invalid or oversized local file\n";
        return false;
    }
    const std::string remoteName = baseName(localPath);
    if (remoteName.empty() || remoteName == "." || remoteName == "..") {
        return false;
    }

    std::string error;
    const netdisk::ConnectionPtr connection = connectToServer(context, error);
    if (!connection) {
        std::cerr << error << std::endl;
        return false;
    }
    const std::string request = "PUT " + context.token + " " +
                                std::to_string(context.directoryId) + " " + remoteName;
    if (!netdisk::sendFrame(*connection, request, error)) {
        std::cerr << error << std::endl;
        return false;
    }

    std::string sha256;
    if (!netdisk::sha256File(localPath, sha256, error) ||
        !netdisk::sendFrame(*connection, sha256, error)) {
        std::cerr << error << std::endl;
        return false;
    }
    std::string response;
    if (!netdisk::receiveText(*connection, response, error)) {
        std::cerr << error << std::endl;
        return false;
    }
    if (response == "INSTANT") {
        std::cout << "upload complete (instant)\n";
        return true;
    }
    if (response != "READY") {
        std::cerr << response << std::endl;
        return false;
    }

    if (!netdisk::sendUInt64(*connection,
                            static_cast<std::uint64_t>(fileStatus.st_size), error)) {
        std::cerr << error << std::endl;
        return false;
    }
    std::ifstream input(localPath.c_str(), std::ios::binary);
    std::vector<unsigned char> buffer(netdisk::kFileChunkSize);
    while (input) {
        input.read(reinterpret_cast<char *>(buffer.data()), buffer.size());
        const std::streamsize count = input.gcount();
        if (count <= 0) {
            break;
        }
        if (!netdisk::sendFrame(*connection, buffer.data(),
                                static_cast<std::uint32_t>(count), error)) {
            std::cerr << error << std::endl;
            return false;
        }
    }
    if (!netdisk::sendFrame(*connection, nullptr, 0, error) ||
        !netdisk::receiveText(*connection, response, error)) {
        std::cerr << error << std::endl;
        return false;
    }
    std::cout << response << std::endl;
    return response == "OK";
}

bool downloadFile(const ClientContext &context, const std::string &fileName)
{
    const std::string temporaryPath = fileName + ".part";
    struct stat partStatus;
    std::uint64_t offset = 0;
    if (stat(temporaryPath.c_str(), &partStatus) == 0 && S_ISREG(partStatus.st_mode)) {
        offset = static_cast<std::uint64_t>(partStatus.st_size);
    }

    for (int attempt = 0; attempt < 2; ++attempt) {
        std::string error;
        const netdisk::ConnectionPtr connection = connectToServer(context, error);
        if (!connection) {
            std::cerr << error << std::endl;
            return false;
        }
        const std::string request = "GET " + context.token + " " +
                                    std::to_string(context.directoryId) + " " + fileName +
                                    " " + std::to_string(offset);
        if (!netdisk::sendFrame(*connection, request, error)) {
            std::cerr << error << std::endl;
            return false;
        }

        std::string response;
        if (!netdisk::receiveText(*connection, response, error)) {
            std::cerr << error << std::endl;
            return false;
        }
        if (response == "NOT_FOUND") {
            std::cout << "file not found\n";
            return false;
        }
        if (response.compare(0, 6, "FOUND ") != 0) {
            if (attempt == 0 && response == "ERR invalid offset") {
                unlink(temporaryPath.c_str());
                offset = 0;
                continue;
            }
            std::cerr << response << std::endl;
            return false;
        }
        const std::string expectedSha256 = response.substr(6);
        std::uint64_t fileSize = 0;
        if (!netdisk::receiveUInt64(*connection, fileSize, error) ||
            fileSize > context.options.maxFileSize || offset > fileSize) {
            std::cerr << "invalid remote file size or offset\n";
            return false;
        }

        const int fd = open(temporaryPath.c_str(), O_CREAT | O_WRONLY, 0640);
        // Keep the partial file length equal to the confirmed prefix. If the
        // connection breaks, the next run resumes from the real prefix rather
        // than mistaking a pre-sized sparse file for a completed download.
        if (fd < 0 || ftruncate(fd, static_cast<off_t>(offset)) != 0 ||
            lseek(fd, static_cast<off_t>(offset), SEEK_SET) < 0) {
            if (fd >= 0) {
                close(fd);
            }
            std::cerr << "cannot prepare local partial file\n";
            return false;
        }

        std::uint64_t received = offset;
        while (true) {
            std::vector<unsigned char> chunk;
            if (!netdisk::receiveFrame(*connection, chunk, error,
                                       netdisk::kFileChunkSize)) {
                close(fd);
                std::cerr << error << std::endl;
                return false;
            }
            if (chunk.empty()) {
                break;
            }
            if (received + chunk.size() > fileSize ||
                !writeAll(fd, chunk.data(), chunk.size())) {
                close(fd);
                std::cerr << "invalid or incomplete download\n";
                return false;
            }
            received += chunk.size();
        }
        fsync(fd);
        close(fd);
        if (received != fileSize) {
            std::cerr << "download ended before the expected size\n";
            return false;
        }

        std::string actualSha256;
        if (!netdisk::sha256File(temporaryPath, actualSha256, error) ||
            actualSha256 != expectedSha256) {
            unlink(temporaryPath.c_str());
            std::cerr << "download checksum verification failed\n";
            return false;
        }
        if (rename(temporaryPath.c_str(), fileName.c_str()) != 0) {
            std::cerr << "cannot finalize downloaded file\n";
            return false;
        }
        std::cout << "download complete\n";
        return true;
    }
    return false;
}
