#include "networkclient.h"

#include "protocol.h"

#include <QRegularExpression>

namespace {
const int kNetworkTimeoutMs = 30000;

bool validName(const QString &name)
{
    if (name.isEmpty() || name == QStringLiteral(".") ||
        name == QStringLiteral("..") || name.size() > 255) return false;
    for (const QChar &character : name) {
        if (character.isSpace() || character == QLatin1Char('/') ||
            character == QLatin1Char('\\')) return false;
    }
    return true;
}
}

NetworkClient::NetworkClient(QObject *parent) : QObject(parent) {}

NetworkClient::~NetworkClient() { logout(); }

bool NetworkClient::connectSocket(QSslSocket &socket, QString &error) const
{
    // Development certificates are self-signed. Use a trusted CA and remove
    // this ignoreSslErrors connection in production.
    QObject::connect(&socket,
                     SIGNAL(sslErrors(const QList<QSslError> &)),
                     &socket,
                     SLOT(ignoreSslErrors()));
    socket.connectToHostEncrypted(host_, port_);
    if (!socket.waitForEncrypted(kNetworkTimeoutMs)) {
        error = socket.errorString();
        if (error.isEmpty()) error = QStringLiteral("TLS 连接失败");
        socket.abort();
        return false;
    }
    return true;
}

bool NetworkClient::authenticate(const QString &command, QString &error)
{
    if (!qtprotocol::sendText(&socket_, command, kNetworkTimeoutMs, error)) return false;
    QString response;
    if (!qtprotocol::receiveText(&socket_, response, kNetworkTimeoutMs, error)) return false;
    if (!response.startsWith(QStringLiteral("OK "))) return parseErrorResponse(response, error);
    token_ = response.mid(3).trimmed();
    if (token_.isEmpty()) {
        error = QStringLiteral("服务端返回了空 Token");
        return false;
    }
    currentDirectory_ = 0;
    return true;
}

bool NetworkClient::login(const QString &host, quint16 port,
                          const QString &username, const QString &password,
                          QString &error)
{
    logout();
    host_ = host.trimmed();
    port_ = port;
    username_ = username.trimmed();
    if (host_.isEmpty() || username_.isEmpty() || password.isEmpty() ||
        username_.contains(QRegularExpression(QStringLiteral("\\s"))) ||
        password.contains(QRegularExpression(QStringLiteral("\\s")))) {
        error = QStringLiteral("服务器、用户名和密码不能为空，当前协议不支持空格");
        return false;
    }
    if (!connectSocket(socket_, error)) return false;
    return authenticate(QStringLiteral("LOGIN %1 %2").arg(username_, password), error);
}

bool NetworkClient::registerAndLogin(const QString &host, quint16 port,
                                     const QString &username,
                                     const QString &password, QString &error)
{
    logout();
    host_ = host.trimmed();
    port_ = port;
    username_ = username.trimmed();
    if (host_.isEmpty() || username_.isEmpty() || password.isEmpty() ||
        username_.contains(QRegularExpression(QStringLiteral("\\s"))) ||
        password.contains(QRegularExpression(QStringLiteral("\\s")))) {
        error = QStringLiteral("服务器、用户名和密码不能为空，当前协议不支持空格");
        return false;
    }
    if (!connectSocket(socket_, error)) return false;
    if (!qtprotocol::sendText(&socket_,
                              QStringLiteral("REGISTER %1 %2").arg(username_, password),
                              kNetworkTimeoutMs, error)) return false;
    QString response;
    if (!qtprotocol::receiveText(&socket_, response, kNetworkTimeoutMs, error)) return false;
    if (response != QStringLiteral("OK registered")) return parseErrorResponse(response, error);
    return authenticate(QStringLiteral("LOGIN %1 %2").arg(username_, password), error);
}

void NetworkClient::logout()
{
    if (socket_.isOpen()) {
        QString ignored;
        if (!token_.isEmpty()) {
            qtprotocol::sendText(&socket_, QStringLiteral("QUIT %1").arg(token_), 1000, ignored);
        }
        socket_.disconnectFromHost();
        if (socket_.state() != QAbstractSocket::UnconnectedState) socket_.waitForDisconnected(1000);
    }
    token_.clear();
    currentDirectory_ = 0;
}

bool NetworkClient::parseErrorResponse(const QString &response, QString &error)
{
    error = response.startsWith(QStringLiteral("ERR ")) ? response.mid(4) : response;
    if (error.isEmpty()) error = QStringLiteral("服务端返回了未知错误");
    return false;
}

bool NetworkClient::sendCommand(const QString &command, QString &response,
                                QString &error)
{
    if (!isLoggedIn()) {
        error = QStringLiteral("尚未登录");
        return false;
    }
    if (!qtprotocol::sendText(&socket_, command, kNetworkTimeoutMs, error) ||
        !qtprotocol::receiveText(&socket_, response, kNetworkTimeoutMs, error)) return false;
    if (response.startsWith(QStringLiteral("ERR"))) return parseErrorResponse(response, error);
    return true;
}

bool NetworkClient::list(QVector<RemoteEntry> &entries, QString &error)
{
    entries.clear();
    QString response;
    if (!sendCommand(QStringLiteral("LS %1 %2").arg(token_).arg(currentDirectory_),
                     response, error)) return false;
    const QStringList lines = response.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList fields = line.split(QLatin1Char('\t'));
        if (fields.size() != 3) continue;
        bool ok = false;
        const quint64 size = fields[2].toULongLong(&ok);
        if (!ok || fields[0].isEmpty() || fields[1].isEmpty()) continue;
        RemoteEntry entry;
        entry.name = fields[0];
        entry.type = fields[1].at(0);
        entry.size = size;
        entries.append(entry);
    }
    return true;
}

bool NetworkClient::makeDirectory(const QString &name, QString &error)
{
    if (!validName(name)) {
        error = QStringLiteral("目录名不能为空，且不能包含空格或路径分隔符");
        return false;
    }
    QString response;
    return sendCommand(QStringLiteral("MKDIR %1 %2 %3")
                           .arg(token_).arg(currentDirectory_).arg(name), response, error);
}

bool NetworkClient::removeEntry(const QString &name, bool directory, QString &error)
{
    if (!validName(name)) {
        error = QStringLiteral("文件名无效");
        return false;
    }
    QString response;
    const QString command = directory ? QStringLiteral("RMDIR") : QStringLiteral("RM");
    return sendCommand(QStringLiteral("%1 %2 %3 %4")
                           .arg(command).arg(token_).arg(currentDirectory_).arg(name), response, error);
}

bool NetworkClient::changeDirectory(const QString &name, quint64 &newDirectory,
                                    QString &error)
{
    if (name != QStringLiteral("..") && !validName(name)) {
        error = QStringLiteral("目录名无效");
        return false;
    }
    QString response;
    if (!sendCommand(QStringLiteral("CD %1 %2 %3")
                         .arg(token_).arg(currentDirectory_).arg(name), response, error)) return false;
    if (!response.startsWith(QStringLiteral("OK "))) return parseErrorResponse(response, error);
    bool ok = false;
    newDirectory = response.mid(3).trimmed().toULongLong(&ok);
    if (!ok) {
        error = QStringLiteral("服务端返回了无效目录 ID");
        return false;
    }
    currentDirectory_ = newDirectory;
    return true;
}
