#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>

class QIODevice;

namespace qtprotocol {

static const quint32 kMaxFrameSize = 4U * 1024U * 1024U;
static const quint32 kFileChunkSize = 64U * 1024U;

bool writeAll(QIODevice *device, const QByteArray &data, int timeoutMs,
              QString &error);
bool readExact(QIODevice *device, qint64 size, QByteArray &data,
               int timeoutMs, QString &error);
bool sendFrame(QIODevice *device, const QByteArray &payload,
               int timeoutMs, QString &error);
bool sendText(QIODevice *device, const QString &text,
              int timeoutMs, QString &error);
bool receiveFrame(QIODevice *device, QByteArray &payload,
                  int timeoutMs, QString &error,
                  quint32 maxSize = kMaxFrameSize);
bool receiveText(QIODevice *device, QString &text,
                 int timeoutMs, QString &error,
                 quint32 maxSize = kMaxFrameSize);
bool sendUInt64(QIODevice *device, quint64 value,
                int timeoutMs, QString &error);
bool receiveUInt64(QIODevice *device, quint64 &value,
                   int timeoutMs, QString &error);

} // namespace qtprotocol
