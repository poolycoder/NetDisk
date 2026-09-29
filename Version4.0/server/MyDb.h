#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <mysql/mysql.h>

struct FileEntry {
    std::uint64_t id = 0;
    std::uint64_t parentId = 0;
    std::string name;
    char type = 'f';
    std::uint64_t size = 0;
    std::string sha256;
};

class MyDb {
public:
    MyDb();
    ~MyDb();

    MyDb(const MyDb &) = delete;
    MyDb &operator=(const MyDb &) = delete;

    bool initDB(const std::string &host, const std::string &user,
                const std::string &password, const std::string &database,
                unsigned int port, std::string &error);

    bool userExists(const std::string &user, bool &exists, std::string &error);
    bool createUser(const std::string &user, const std::string &passwordHash,
                    std::string &error);
    bool getPasswordHash(const std::string &user, std::string &hash,
                         std::string &error);

    bool listEntries(const std::string &user, std::uint64_t parentId,
                     std::vector<FileEntry> &entries, std::string &error);
    bool findEntry(const std::string &user, std::uint64_t parentId,
                   const std::string &name, FileEntry &entry, std::string &error);
    bool getParentId(const std::string &user, std::uint64_t directoryId,
                     std::uint64_t &parentId, std::string &error);
    bool createDirectory(const std::string &user, std::uint64_t parentId,
                         const std::string &name, std::string &error);
    bool deleteDirectory(const std::string &user, std::uint64_t parentId,
                         const std::string &name, std::string &error);
    bool deleteFile(const std::string &user, std::uint64_t parentId,
                    const std::string &name, FileEntry &deletedEntry,
                    std::string &error);

    bool hasSha256(const std::string &sha256, bool &exists, std::string &error);
    bool hasOtherSha256Reference(const std::string &sha256,
                                 const std::string &user,
                                 std::uint64_t parentId,
                                 const std::string &name,
                                 bool &exists, std::string &error);
    bool insertFile(const std::string &user, std::uint64_t parentId,
                    const std::string &name, std::uint64_t size,
                    const std::string &sha256, std::string &error);
    bool writeLog(const std::string &user, const std::string &operation,
                  std::string &error);

private:
    MYSQL *mysql_;
    bool mysqlThreadInitialized_;

    bool executeStatement(const char *sql, MYSQL_BIND *parameters,
                          unsigned long parameterCount, std::string &error);
};
