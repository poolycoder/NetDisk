#include "protocol.h"

#include <QIODevice>
#include <QtEndian>

#include <cstring>

namespace qtprotocol {

bool writeAll(QIODevice *device, const QByteArray &data, int timeoutMs,
              QString &error)
{
    if (device == nullptr) {
        error = QStringLiteral("无效的网络设备");
        return false;
    }
    qint64 written = 0;
    while (written < data.size()) {
        const qint64 count = device->write(data.constData() + written,
                                           data.size() - written);
        if (count <= 0) {
            error = device->errorString();
            if (error.isEmpty()) error = QStringLiteral("网络写入失败");
            return false;
        }
        written += count;
        if (!device->waitForBytesWritten(timeoutMs)) {
            error = device->errorString();
            if (error.isEmpty()) error = QStringLiteral("等待网络写入超时");
            return false;
        }
    }
    return true;
}

bool readExact(QIODevice *device, qint64 size, QByteArray &data,
               int timeoutMs, QString &error)
{
    if (device == nullptr || size < 0) {
        error = QStringLiteral("无效的网络读取参数");
        return false;
    }
    data.clear();
    while (data.size() < size) {
        if (device->bytesAvailable() == 0 && !device->waitForReadyRead(timeoutMs)) {
            error = device->errorString();
            if (error.isEmpty()) error = QStringLiteral("网络读取超时或连接已关闭");
            return false;
        }
        const QByteArray part = device->read(size - data.size());
        if (part.isEmpty()) {
            error = QStringLiteral("对端提前关闭连接");
            return false;
        }
        data.append(part);
    }
    return true;
}

bool sendFrame(QIODevice *device, const QByteArray &payload,
               int timeoutMs, QString &error)
{
    if (static_cast<quint64>(payload.size()) > kMaxFrameSize) {
        error = QStringLiteral("数据帧超过最大长度限制");
        return false;
    }
    quint32 length = qToBigEndian<quint32>(static_cast<quint32>(payload.size()));
    const QByteArray header(reinterpret_cast<const char *>(&length), sizeof(length));
    return writeAll(device, header, timeoutMs, error) &&
           (payload.isEmpty() || writeAll(device, payload, timeoutMs, error));
}

bool sendText(QIODevice *device, const QString &text,
              int timeoutMs, QString &error)
{
    return sendFrame(device, text.toUtf8(), timeoutMs, error);
}

bool receiveFrame(QIODevice *device, QByteArray &payload,
                  int timeoutMs, QString &error, quint32 maxSize)
{
    QByteArray header;
    if (!readExact(device, sizeof(quint32), header, timeoutMs, error)) return false;
    quint32 networkLength = 0;
    std::memcpy(&networkLength, header.constData(), sizeof(networkLength));
    const quint32 length = qFromBigEndian<quint32>(networkLength);
    if (length > maxSize || length > kMaxFrameSize) {
        error = QStringLiteral("服务端返回的数据帧过大");
        return false;
    }
    if (length == 0) {
        payload.clear();
        return true;
    }
    return readExact(device, length, payload, timeoutMs, error);
}

bool receiveText(QIODevice *device, QString &text,
                 int timeoutMs, QString &error, quint32 maxSize)
{
    QByteArray payload;
    if (!receiveFrame(device, payload, timeoutMs, error, maxSize)) return false;
    text = QString::fromUtf8(payload);
    return true;
}

bool sendUInt64(QIODevice *device, quint64 value,
                int timeoutMs, QString &error)
{
    quint64 networkValue = qToBigEndian<quint64>(value);
    return sendFrame(device, QByteArray(reinterpret_cast<const char *>(&networkValue),
                                        sizeof(networkValue)), timeoutMs, error);
}

bool receiveUInt64(QIODevice *device, quint64 &value,
                   int timeoutMs, QString &error)
{
    QByteArray payload;
    if (!receiveFrame(device, payload, timeoutMs, error, sizeof(quint64)) ||
        payload.size() != static_cast<int>(sizeof(quint64))) {
        error = QStringLiteral("服务端返回了无效的 64 位整数");
        return false;
    }
    quint64 networkValue = 0;
    std::memcpy(&networkValue, payload.constData(), sizeof(networkValue));
    value = qFromBigEndian<quint64>(networkValue);
    return true;
}

} // namespace qtprotocol
