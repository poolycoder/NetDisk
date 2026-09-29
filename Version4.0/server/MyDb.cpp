#include "MyDb.h"

#include <cstring>

namespace {

void bindString(MYSQL_BIND &bind, const std::string &value)
{
    std::memset(&bind, 0, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_STRING;
    bind.buffer = const_cast<char *>(value.data());
    bind.buffer_length = static_cast<unsigned long>(value.size());
}

void bindUInt64(MYSQL_BIND &bind, std::uint64_t &value)
{
    std::memset(&bind, 0, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_LONGLONG;
    bind.is_unsigned = 1;
    bind.buffer = &value;
    bind.buffer_length = sizeof(value);
}

bool prepareAndBind(MYSQL *mysql, const char *sql, MYSQL_STMT **statement,
                    MYSQL_BIND *parameters, unsigned long parameterCount,
                    std::string &error)
{
    *statement = mysql_stmt_init(mysql);
    if (*statement == nullptr) {
        error = "mysql_stmt_init failed";
        return false;
    }
    if (mysql_stmt_prepare(*statement, sql, std::strlen(sql)) != 0) {
        error = mysql_stmt_error(*statement);
        mysql_stmt_close(*statement);
        *statement = nullptr;
        return false;
    }
    if (parameterCount != 0 && mysql_stmt_bind_param(*statement, parameters) != 0) {
        error = mysql_stmt_error(*statement);
        mysql_stmt_close(*statement);
        *statement = nullptr;
        return false;
    }
    return true;
}

} // namespace

MyDb::MyDb() : mysql_(nullptr), mysqlThreadInitialized_(false)
{
    mysql_thread_init();
    mysqlThreadInitialized_ = true;
    mysql_ = mysql_init(nullptr);
}

MyDb::~MyDb()
{
    if (mysql_ != nullptr) {
        mysql_close(mysql_);
    }
    if (mysqlThreadInitialized_) {
        mysql_thread_end();
    }
}

bool MyDb::initDB(const std::string &host, const std::string &user,
                  const std::string &password, const std::string &database,
                  unsigned int port, std::string &error)
{
    if (mysql_ == nullptr) {
        error = "mysql_init failed";
        return false;
    }
    if (mysql_real_connect(mysql_, host.c_str(), user.c_str(), password.c_str(),
                           database.c_str(), port, nullptr, 0) == nullptr) {
        error = mysql_error(mysql_);
        return false;
    }
    mysql_set_character_set(mysql_, "utf8mb4");
    return true;
}

bool MyDb::executeStatement(const char *sql, MYSQL_BIND *parameters,
                            unsigned long parameterCount, std::string &error)
{
    MYSQL_STMT *statement = nullptr;
    if (!prepareAndBind(mysql_, sql, &statement, parameters, parameterCount,
                        error)) {
        return false;
    }
    const bool success = mysql_stmt_execute(statement) == 0;
    if (!success) {
        error = mysql_stmt_error(statement);
    }
    mysql_stmt_close(statement);
    return success;
}

bool MyDb::userExists(const std::string &user, bool &exists, std::string &error)
{
    exists = false;
    MYSQL_BIND parameters[1];
    bindString(parameters[0], user);
    MYSQL_STMT *statement = nullptr;
    if (!prepareAndBind(mysql_, "SELECT 1 FROM Shadow WHERE User=? LIMIT 1",
                        &statement, parameters, 1, error)) {
        return false;
    }
    if (mysql_stmt_execute(statement) != 0 || mysql_stmt_store_result(statement) != 0) {
        error = mysql_stmt_error(statement);
        mysql_stmt_close(statement);
        return false;
    }
    exists = mysql_stmt_num_rows(statement) > 0;
    mysql_stmt_free_result(statement);
    mysql_stmt_close(statement);
    return true;
}

bool MyDb::createUser(const std::string &user, const std::string &passwordHash,
                      std::string &error)
{
    MYSQL_BIND parameters[2];
    bindString(parameters[0], user);
    bindString(parameters[1], passwordHash);
    return executeStatement("INSERT INTO Shadow(User, PasswordHash) VALUES(?, ?)",
                            parameters, 2, error);
}

bool MyDb::getPasswordHash(const std::string &user, std::string &hash,
                           std::string &error)
{
    hash.clear();
    MYSQL_BIND parameters[1];
    bindString(parameters[0], user);
    MYSQL_STMT *statement = nullptr;
    if (!prepareAndBind(mysql_, "SELECT PasswordHash FROM Shadow WHERE User=? LIMIT 1",
                        &statement, parameters, 1, error)) {
        return false;
    }
    if (mysql_stmt_execute(statement) != 0) {
        error = mysql_stmt_error(statement);
        mysql_stmt_close(statement);
        return false;
    }

    char buffer[512] = {0};
    unsigned long length = 0;
    MYSQL_BIND result[1];
    std::memset(result, 0, sizeof(result));
    result[0].buffer_type = MYSQL_TYPE_STRING;
    result[0].buffer = buffer;
    result[0].buffer_length = sizeof(buffer) - 1;
    result[0].length = &length;
    if (mysql_stmt_bind_result(statement, result) != 0 ||
        mysql_stmt_store_result(statement) != 0) {
        error = mysql_stmt_error(statement);
        mysql_stmt_close(statement);
        return false;
    }
    if (mysql_stmt_fetch(statement) == 0) {
        hash.assign(buffer, length);
    }
    mysql_stmt_free_result(statement);
    mysql_stmt_close(statement);
    return true;
}

namespace {

bool bindFileResult(MYSQL_STMT *statement, std::uint64_t &id, std::uint64_t &dir,
                    std::uint64_t &size, char *name, unsigned long &nameLength,
                    char *type, char *sha, unsigned long &shaLength,
                    std::string &error)
{
    MYSQL_BIND result[6];
    std::memset(result, 0, sizeof(result));
    result[0].buffer_type = MYSQL_TYPE_LONGLONG;
    result[0].is_unsigned = 1;
    result[0].buffer = &id;
    result[1].buffer_type = MYSQL_TYPE_LONGLONG;
    result[1].is_unsigned = 1;
    result[1].buffer = &dir;
    result[2].buffer_type = MYSQL_TYPE_STRING;
    result[2].buffer = name;
    result[2].buffer_length = 255;
    result[2].length = &nameLength;
    result[3].buffer_type = MYSQL_TYPE_LONGLONG;
    result[3].is_unsigned = 1;
    result[3].buffer = &size;
    result[4].buffer_type = MYSQL_TYPE_STRING;
    result[4].buffer = type;
    result[4].buffer_length = 1;
    result[5].buffer_type = MYSQL_TYPE_STRING;
    result[5].buffer = sha;
    result[5].buffer_length = 64;
    result[5].length = &shaLength;
    if (mysql_stmt_bind_result(statement, result) != 0 ||
        mysql_stmt_store_result(statement) != 0) {
        error = mysql_stmt_error(statement);
        return false;
    }
    return true;
}

} // namespace

bool MyDb::listEntries(const std::string &user, std::uint64_t parentId,
                       std::vector<FileEntry> &entries, std::string &error)
{
    entries.clear();
    MYSQL_BIND parameters[2];
    bindString(parameters[0], user);
    bindUInt64(parameters[1], parentId);
    MYSQL_STMT *statement = nullptr;
    const char *sql = "SELECT FileId, Dir, FileName, FileSize, FileType, COALESCE(SHA256, '') "
                      "FROM Virtual_Dir WHERE User=? AND Dir=? ORDER BY FileType, FileName";
    if (!prepareAndBind(mysql_, sql, &statement, parameters, 2, error)) {
        return false;
    }
    if (mysql_stmt_execute(statement) != 0) {
        error = mysql_stmt_error(statement);
        mysql_stmt_close(statement);
        return false;
    }

    std::uint64_t id = 0, dir = 0, size = 0;
    char name[256] = {0}, type[2] = {0}, sha[65] = {0};
    unsigned long nameLength = 0, shaLength = 0;
    if (!bindFileResult(statement, id, dir, size, name, nameLength, type, sha,
                        shaLength, error)) {
        mysql_stmt_close(statement);
        return false;
    }
    while (mysql_stmt_fetch(statement) == 0) {
        FileEntry entry;
        entry.id = id;
        entry.parentId = dir;
        entry.name.assign(name, nameLength);
        entry.type = type[0];
        entry.size = size;
        entry.sha256.assign(sha, shaLength);
        entries.push_back(entry);
        std::memset(name, 0, sizeof(name));
        std::memset(sha, 0, sizeof(sha));
        nameLength = 0;
        shaLength = 0;
    }
    mysql_stmt_free_result(statement);
    mysql_stmt_close(statement);
    return true;
}

bool MyDb::findEntry(const std::string &user, std::uint64_t parentId,
                     const std::string &name, FileEntry &entry, std::string &error)
{
    MYSQL_BIND parameters[3];
    bindString(parameters[0], user);
    bindUInt64(parameters[1], parentId);
    bindString(parameters[2], name);
    MYSQL_STMT *statement = nullptr;
    const char *sql = "SELECT FileId, Dir, FileName, FileSize, FileType, COALESCE(SHA256, '') "
                      "FROM Virtual_Dir WHERE User=? AND Dir=? AND FileName=? LIMIT 1";
    if (!prepareAndBind(mysql_, sql, &statement, parameters, 3, error)) {
        return false;
    }
    if (mysql_stmt_execute(statement) != 0) {
        error = mysql_stmt_error(statement);
        mysql_stmt_close(statement);
        return false;
    }

    std::uint64_t id = 0, dir = 0, size = 0;
    char nameBuffer[256] = {0}, type[2] = {0}, sha[65] = {0};
    unsigned long nameLength = 0, shaLength = 0;
    if (!bindFileResult(statement, id, dir, size, nameBuffer, nameLength, type,
                        sha, shaLength, error)) {
        mysql_stmt_close(statement);
        return false;
    }
    const bool found = mysql_stmt_fetch(statement) == 0;
    if (found) {
        entry.id = id;
        entry.parentId = dir;
        entry.name.assign(nameBuffer, nameLength);
        entry.type = type[0];
        entry.size = size;
        entry.sha256.assign(sha, shaLength);
    }
    mysql_stmt_free_result(statement);
    mysql_stmt_close(statement);
    return found;
}

bool MyDb::getParentId(const std::string &user, std::uint64_t directoryId,
                       std::uint64_t &parentId, std::string &error)
{
    parentId = 0;
    MYSQL_BIND parameters[2];
    bindString(parameters[0], user);
    bindUInt64(parameters[1], directoryId);
    MYSQL_STMT *statement = nullptr;
    if (!prepareAndBind(mysql_, "SELECT Dir FROM Virtual_Dir WHERE User=? AND FileId=? "
                                "AND FileType='d' LIMIT 1",
                        &statement, parameters, 2, error)) {
        return false;
    }
    if (mysql_stmt_execute(statement) != 0) {
        error = mysql_stmt_error(statement);
        mysql_stmt_close(statement);
        return false;
    }
    MYSQL_BIND result[1];
    std::memset(result, 0, sizeof(result));
    result[0].buffer_type = MYSQL_TYPE_LONGLONG;
    result[0].is_unsigned = 1;
    result[0].buffer = &parentId;
    if (mysql_stmt_bind_result(statement, result) != 0 ||
        mysql_stmt_store_result(statement) != 0) {
        error = mysql_stmt_error(statement);
        mysql_stmt_close(statement);
        return false;
    }
    const bool found = mysql_stmt_fetch(statement) == 0;
    mysql_stmt_free_result(statement);
    mysql_stmt_close(statement);
    return found;
}

bool MyDb::createDirectory(const std::string &user, std::uint64_t parentId,
                           const std::string &name, std::string &error)
{
    MYSQL_BIND parameters[4];
    bindUInt64(parameters[0], parentId);
    bindString(parameters[1], name);
    const std::string type = "d";
    bindString(parameters[2], type);
    bindString(parameters[3], user);
    return executeStatement("INSERT INTO Virtual_Dir(Dir, FileName, FileSize, FileType, SHA256, User) "
                            "VALUES(?, ?, 0, ?, NULL, ?)",
                            parameters, 4, error);
}

bool MyDb::deleteDirectory(const std::string &user, std::uint64_t parentId,
                           const std::string &name, std::string &error)
{
    FileEntry directory;
    if (!findEntry(user, parentId, name, directory, error) || directory.type != 'd') {
        if (error.empty()) {
            error = "directory not found";
        }
        return false;
    }

    MYSQL_BIND childParameters[2];
    bindString(childParameters[0], user);
    bindUInt64(childParameters[1], directory.id);
    MYSQL_STMT *childStatement = nullptr;
    if (!prepareAndBind(mysql_, "SELECT 1 FROM Virtual_Dir WHERE User=? AND Dir=? LIMIT 1",
                        &childStatement, childParameters, 2, error)) {
        return false;
    }
    if (mysql_stmt_execute(childStatement) != 0 ||
        mysql_stmt_store_result(childStatement) != 0) {
        error = mysql_stmt_error(childStatement);
        mysql_stmt_close(childStatement);
        return false;
    }
    const bool hasChildren = mysql_stmt_num_rows(childStatement) > 0;
    mysql_stmt_free_result(childStatement);
    mysql_stmt_close(childStatement);
    if (hasChildren) {
        error = "directory is not empty";
        return false;
    }

    MYSQL_BIND deleteParameters[2];
    bindString(deleteParameters[0], user);
    bindUInt64(deleteParameters[1], directory.id);
    return executeStatement("DELETE FROM Virtual_Dir WHERE User=? AND FileId=? "
                            "AND FileType='d'",
                            deleteParameters, 2, error);
}

bool MyDb::deleteFile(const std::string &user, std::uint64_t parentId,
                      const std::string &name, FileEntry &deletedEntry,
                      std::string &error)
{
    if (!findEntry(user, parentId, name, deletedEntry, error) || deletedEntry.type != 'f') {
        if (error.empty()) {
            error = "file not found";
        }
        return false;
    }
    MYSQL_BIND parameters[2];
    bindString(parameters[0], user);
    bindUInt64(parameters[1], deletedEntry.id);
    return executeStatement("DELETE FROM Virtual_Dir WHERE User=? AND FileId=? "
                            "AND FileType='f'",
                            parameters, 2, error);
}

bool MyDb::hasSha256(const std::string &sha256, bool &exists, std::string &error)
{
    exists = false;
    MYSQL_BIND parameters[1];
    bindString(parameters[0], sha256);
    MYSQL_STMT *statement = nullptr;
    if (!prepareAndBind(mysql_, "SELECT 1 FROM Virtual_Dir WHERE SHA256=? AND FileType='f' LIMIT 1",
                        &statement, parameters, 1, error)) {
        return false;
    }
    if (mysql_stmt_execute(statement) != 0 || mysql_stmt_store_result(statement) != 0) {
        error = mysql_stmt_error(statement);
        mysql_stmt_close(statement);
        return false;
    }
    exists = mysql_stmt_num_rows(statement) > 0;
    mysql_stmt_free_result(statement);
    mysql_stmt_close(statement);
    return true;
}

bool MyDb::hasOtherSha256Reference(const std::string &sha256,
                                   const std::string &user,
                                   std::uint64_t parentId,
                                   const std::string &name,
                                   bool &exists, std::string &error)
{
    exists = false;
    MYSQL_BIND parameters[4];
    bindString(parameters[0], sha256);
    bindString(parameters[1], user);
    bindUInt64(parameters[2], parentId);
    bindString(parameters[3], name);
    MYSQL_STMT *statement = nullptr;
    const char *sql = "SELECT 1 FROM Virtual_Dir WHERE SHA256=? AND NOT "
                      "(User=? AND Dir=? AND FileName=?) LIMIT 1";
    if (!prepareAndBind(mysql_, sql, &statement, parameters, 4, error)) {
        return false;
    }
    if (mysql_stmt_execute(statement) != 0 || mysql_stmt_store_result(statement) != 0) {
        error = mysql_stmt_error(statement);
        mysql_stmt_close(statement);
        return false;
    }
    exists = mysql_stmt_num_rows(statement) > 0;
    mysql_stmt_free_result(statement);
    mysql_stmt_close(statement);
    return true;
}

bool MyDb::insertFile(const std::string &user, std::uint64_t parentId,
                      const std::string &name, std::uint64_t size,
                      const std::string &sha256, std::string &error)
{
    MYSQL_BIND parameters[6];
    bindUInt64(parameters[0], parentId);
    bindString(parameters[1], name);
    bindUInt64(parameters[2], size);
    const std::string type = "f";
    bindString(parameters[3], type);
    bindString(parameters[4], sha256);
    bindString(parameters[5], user);
    return executeStatement("INSERT INTO Virtual_Dir(Dir, FileName, FileSize, FileType, SHA256, User) "
                            "VALUES(?, ?, ?, ?, ?, ?)",
                            parameters, 6, error);
}

bool MyDb::writeLog(const std::string &user, const std::string &operation,
                    std::string &error)
{
    MYSQL_BIND parameters[2];
    bindString(parameters[0], user);
    bindString(parameters[1], operation);
    return executeStatement("INSERT INTO Log(User, Operation, time) VALUES(?, ?, NOW())",
                            parameters, 2, error);
}
