#include "../common/connection.h"
#include "../common/crypto.h"
#include "../common/protocol.h"

#include <cassert>
#include <fstream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

int main()
{
    int sockets[2] = {-1, -1};
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    const netdisk::ConnectionPtr sender =
        std::make_shared<netdisk::Connection>(sockets[0]);
    const netdisk::ConnectionPtr receiver =
        std::make_shared<netdisk::Connection>(sockets[1]);

    std::thread writer([&sender] {
        std::string error;
        assert(netdisk::sendFrame(*sender, "protocol round trip", error));
    });
    std::string received;
    std::string error;
    assert(netdisk::receiveText(*receiver, received, error));
    assert(received == "protocol round trip");
    writer.join();

    char path[] = "/tmp/netdisk-test-XXXXXX";
    const int file = mkstemp(path);
    assert(file >= 0);
    close(file);
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << "netdisk";
    }
    std::string digest;
    assert(netdisk::sha256File(path, digest, error));
    assert(digest.size() == 64);
    unlink(path);

    std::string encoded;
    assert(netdisk::hashPassword("secret", encoded, error));
    bool matched = false;
    assert(netdisk::verifyPassword("secret", encoded, matched, error));
    assert(matched);
    assert(netdisk::verifyPassword("wrong", encoded, matched, error));
    assert(!matched);
    return 0;
}
