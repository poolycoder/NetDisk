#pragma once

#include <QThread>
#include <QString>

class TransferWorker : public QThread {
    Q_OBJECT

public:
    enum Mode { Upload, Download };

    TransferWorker(Mode mode, const QString &host, quint16 port,
                   const QString &token, quint64 directoryId,
                   const QString &localPath, const QString &remoteName,
                   bool overwrite, QObject *parent = nullptr);

signals:
    void progress(quint64 completed, quint64 total);
    void status(const QString &text);
    void succeeded(const QString &text);
    void failed(const QString &text);

protected:
    void run() override;

private:
    void runUpload();
    void runDownload();
    bool calculateSha256(const QString &path, QString &sha256,
                         quint64 &size, QString &error);

    Mode mode_;
    QString host_;
    quint16 port_;
    QString token_;
    quint64 directoryId_;
    QString localPath_;
    QString remoteName_;
    bool overwrite_;
};
