#include "transferworker.h"

#include "protocol.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QSslSocket>

namespace { const int kNetworkTimeoutMs = 30000; }

TransferWorker::TransferWorker(Mode mode, const QString &host, quint16 port,
                               const QString &token, quint64 directoryId,
                               const QString &localPath, const QString &remoteName,
                               bool overwrite, QObject *parent)
    : QThread(parent), mode_(mode), host_(host), port_(port), token_(token),
      directoryId_(directoryId), localPath_(localPath), remoteName_(remoteName),
      overwrite_(overwrite) {}

void TransferWorker::run()
{
    if (mode_ == Upload) runUpload(); else runDownload();
}

bool TransferWorker::calculateSha256(const QString &path, QString &sha256,
                                     quint64 &size, QString &error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("无法打开本地文件：") + file.errorString();
        return false;
    }
    size = static_cast<quint64>(QFileInfo(file).size());
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(qtprotocol::kFileChunkSize);
        if (chunk.isEmpty() && !file.atEnd()) {
            error = QStringLiteral("读取本地文件失败：") + file.errorString();
            return false;
        }
        hash.addData(chunk);
    }
    sha256 = QString::fromLatin1(hash.result().toHex());
    return true;
}

void TransferWorker::runUpload()
{
    emit status(QStringLiteral("正在计算文件 SHA-256…"));
    QString sha256, error;
    quint64 fileSize = 0;
    if (!calculateSha256(localPath_, sha256, fileSize, error)) {
        emit failed(error); return;
    }
    QFile input(localPath_);
    if (!input.open(QIODevice::ReadOnly)) {
        emit failed(QStringLiteral("无法打开待上传文件：") + input.errorString());
        return;
    }

    QSslSocket socket;
    QObject::connect(&socket,
                     SIGNAL(sslErrors(const QList<QSslError> &)),
                     &socket,
                     SLOT(ignoreSslErrors()));
    socket.connectToHostEncrypted(host_, port_);
    if (!socket.waitForEncrypted(kNetworkTimeoutMs)) {
        emit failed(QStringLiteral("上传 TLS 连接失败：") + socket.errorString());
        return;
    }
    if (!qtprotocol::sendText(&socket,
                              QStringLiteral("PUT %1 %2 %3")
                                  .arg(token_).arg(directoryId_).arg(remoteName_),
                              kNetworkTimeoutMs, error) ||
        !qtprotocol::sendText(&socket, sha256, kNetworkTimeoutMs, error)) {
        emit failed(error); return;
    }

    QString response;
    if (!qtprotocol::receiveText(&socket, response, kNetworkTimeoutMs, error)) {
        emit failed(error); return;
    }
    if (response == QStringLiteral("INSTANT")) {
        emit progress(fileSize, fileSize);
        emit succeeded(QStringLiteral("秒传完成：") + remoteName_);
        return;
    }
    if (response != QStringLiteral("READY")) {
        emit failed(response.startsWith(QStringLiteral("ERR ")) ? response.mid(4) : response);
        return;
    }
    if (!qtprotocol::sendUInt64(&socket, fileSize, kNetworkTimeoutMs, error)) {
        emit failed(error); return;
    }
    quint64 completed = 0;
    while (!input.atEnd()) {
        const QByteArray chunk = input.read(qtprotocol::kFileChunkSize);
        if (chunk.isEmpty() && !input.atEnd()) {
            emit failed(QStringLiteral("读取上传文件失败：") + input.errorString());
            return;
        }
        if (!qtprotocol::sendFrame(&socket, chunk, kNetworkTimeoutMs, error)) {
            emit failed(error); return;
        }
        completed += static_cast<quint64>(chunk.size());
        emit progress(completed, fileSize);
    }
    if (!qtprotocol::sendFrame(&socket, QByteArray(), kNetworkTimeoutMs, error) ||
        !qtprotocol::receiveText(&socket, response, kNetworkTimeoutMs, error)) {
        emit failed(error); return;
    }
    if (response != QStringLiteral("OK")) {
        emit failed(response.startsWith(QStringLiteral("ERR ")) ? response.mid(4) : response);
        return;
    }
    emit succeeded(QStringLiteral("上传完成：") + remoteName_);
}

void TransferWorker::runDownload()
{
    const QString partialPath = localPath_ + QStringLiteral(".part");
    const QFileInfo partialInfo(partialPath);
    const quint64 offset = partialInfo.exists() ? static_cast<quint64>(partialInfo.size()) : 0;
    QString error;

    QSslSocket socket;
    QObject::connect(&socket,
                     SIGNAL(sslErrors(const QList<QSslError> &)),
                     &socket,
                     SLOT(ignoreSslErrors()));
    socket.connectToHostEncrypted(host_, port_);
    if (!socket.waitForEncrypted(kNetworkTimeoutMs)) {
        emit failed(QStringLiteral("下载 TLS 连接失败：") + socket.errorString());
        return;
    }
    emit status(offset == 0 ? QStringLiteral("正在准备下载…")
                            : QStringLiteral("正在断点续传，已有 %1 字节").arg(offset));
    if (!qtprotocol::sendText(&socket,
                              QStringLiteral("GET %1 %2 %3 %4")
                                  .arg(token_).arg(directoryId_).arg(remoteName_).arg(offset),
                              kNetworkTimeoutMs, error)) {
        emit failed(error); return;
    }
    QString response;
    if (!qtprotocol::receiveText(&socket, response, kNetworkTimeoutMs, error)) {
        emit failed(error); return;
    }
    if (response == QStringLiteral("NOT_FOUND")) {
        emit failed(QStringLiteral("远程文件不存在")); return;
    }
    if (!response.startsWith(QStringLiteral("FOUND "))) {
        emit failed(response.startsWith(QStringLiteral("ERR ")) ? response.mid(4) : response);
        return;
    }
    const QString expectedSha256 = response.mid(6).trimmed();
    quint64 totalSize = 0;
    if (!qtprotocol::receiveUInt64(&socket, totalSize, kNetworkTimeoutMs, error)) {
        emit failed(error); return;
    }
    if (offset > totalSize) {
        emit failed(QStringLiteral("本地 .part 文件大于服务器文件，无法续传")); return;
    }

    QFile output(partialPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Append)) {
        emit failed(QStringLiteral("无法打开下载临时文件：") + output.errorString());
        return;
    }
    emit progress(offset, totalSize);
    quint64 received = offset;
    while (true) {
        QByteArray chunk;
        if (!qtprotocol::receiveFrame(&socket, chunk, kNetworkTimeoutMs, error,
                                      qtprotocol::kFileChunkSize)) {
            emit failed(error); return;
        }
        if (chunk.isEmpty()) break;
        if (received + static_cast<quint64>(chunk.size()) > totalSize) {
            emit failed(QStringLiteral("服务端发送了超出文件大小的数据")); return;
        }
        if (output.write(chunk) != chunk.size()) {
            emit failed(QStringLiteral("写入下载临时文件失败：") + output.errorString()); return;
        }
        received += static_cast<quint64>(chunk.size());
        emit progress(received, totalSize);
    }
    output.close();

    QFile verifyFile(partialPath);
    if (!verifyFile.open(QIODevice::ReadOnly)) {
        emit failed(QStringLiteral("无法校验下载文件：") + verifyFile.errorString()); return;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!verifyFile.atEnd()) hash.addData(verifyFile.read(qtprotocol::kFileChunkSize));
    verifyFile.close();
    if (QString::fromLatin1(hash.result().toHex()).compare(expectedSha256, Qt::CaseInsensitive) != 0) {
        emit failed(QStringLiteral("下载完成但 SHA-256 校验失败，已保留 .part 文件")); return;
    }
    if (QFile::exists(localPath_)) {
        if (!overwrite_ || !QFile::remove(localPath_)) {
            emit failed(QStringLiteral("本地目标文件已存在，且无法覆盖")); return;
        }
    }
    if (!QFile::rename(partialPath, localPath_)) {
        emit failed(QStringLiteral("无法将 .part 文件提交为正式文件")); return;
    }
    emit succeeded(QStringLiteral("下载完成：") + localPath_);
}
