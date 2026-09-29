#pragma once

#include <QObject>
#include <QString>
#include <QSslSocket>
#include <QVector>

struct RemoteEntry {
    QString name;
    QChar type;
    quint64 size = 0;
};

class NetworkClient : public QObject {
    Q_OBJECT

public:
    explicit NetworkClient(QObject *parent = nullptr);
    ~NetworkClient() override;

    bool login(const QString &host, quint16 port, const QString &username,
               const QString &password, QString &error);
    bool registerAndLogin(const QString &host, quint16 port,
                          const QString &username, const QString &password,
                          QString &error);
    void logout();

    bool list(QVector<RemoteEntry> &entries, QString &error);
    bool makeDirectory(const QString &name, QString &error);
    bool removeEntry(const QString &name, bool directory, QString &error);
    bool changeDirectory(const QString &name, quint64 &newDirectory,
                         QString &error);

    QString host() const { return host_; }
    quint16 port() const { return port_; }
    QString token() const { return token_; }
    quint64 currentDirectory() const { return currentDirectory_; }
    QString username() const { return username_; }
    bool isLoggedIn() const { return !token_.isEmpty() && socket_.isEncrypted(); }

private:
    bool connectSocket(QSslSocket &socket, QString &error) const;
    bool sendCommand(const QString &command, QString &response,
                     QString &error);
    bool authenticate(const QString &command, QString &error);
    static bool parseErrorResponse(const QString &response, QString &error);

    QSslSocket socket_;
    QString host_;
    quint16 port_ = 2000;
    QString username_;
    QString token_;
    quint64 currentDirectory_ = 0;
};
