#pragma once

#include "networkclient.h"

#include <QMainWindow>
#include <QPointer>

class QLineEdit;
class QPushButton;
class QLabel;
class QProgressBar;
class QSpinBox;
class QStackedWidget;
class QTableWidget;
class QButtonGroup;
class TransferWorker;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void login();
    void registerUser();
    void showHomePage();
    void showFilesPage();
    void showTransfersPage();
    void refreshRemote();
    void refreshLocal();
    void makeDirectory();
    void goParent();
    void tableDoubleClicked(int row, int column);
    void localTableDoubleClicked(int row, int column);
    void deleteSelected();
    void selectLocalDirectory();
    void uploadFile();
    void downloadFile();
    void transferProgress(quint64 completed, quint64 total);
    void transferStatus(const QString &text);
    void transferSucceeded(const QString &text);
    void transferFailed(const QString &text);

private:
    void buildLoginPage();
    void buildWorkspacePage();
    void buildFilePage();
    void buildHomePage();
    void buildTransferPage();
    void setLoggedInUi(bool loggedIn);
    void showError(const QString &message);
    void appendHistory(const QString &operation, const QString &detail,
                       const QString &status = QStringLiteral("完成"));
    void updateHomeSummary();
    void resetLoginMode();
    QString selectedRemoteName(bool *isDirectory = nullptr) const;
    void startTransfer(TransferWorker *worker, const QString &title);
    QString formatSize(quint64 size) const;
    void updatePathLabel();

    QStackedWidget *pages_ = nullptr;
    QWidget *loginPage_ = nullptr;
    QWidget *workspacePage_ = nullptr;
    QStackedWidget *contentPages_ = nullptr;
    QWidget *homePage_ = nullptr;
    QWidget *filePage_ = nullptr;
    QWidget *transferPage_ = nullptr;
    QLineEdit *hostEdit_ = nullptr;
    QSpinBox *portEdit_ = nullptr;
    QLineEdit *usernameEdit_ = nullptr;
    QLineEdit *passwordEdit_ = nullptr;
    QLineEdit *confirmPasswordEdit_ = nullptr;
    QPushButton *loginButton_ = nullptr;
    QPushButton *registerButton_ = nullptr;
    QPushButton *homeNavButton_ = nullptr;
    QPushButton *filesNavButton_ = nullptr;
    QPushButton *transfersNavButton_ = nullptr;
    QPushButton *loginPageBackButton_ = nullptr;
    QLabel *loginFormTitle_ = nullptr;
    QLabel *loginFormSubtitle_ = nullptr;
    QLabel *loginStatus_ = nullptr;
    QLabel *userLabel_ = nullptr;
    QLabel *pathLabel_ = nullptr;
    QLineEdit *localDirectoryEdit_ = nullptr;
    QTableWidget *remoteTable_ = nullptr;
    QTableWidget *localTable_ = nullptr;
    QTableWidget *taskTable_ = nullptr;
    QTableWidget *historyTable_ = nullptr;
    QLabel *homeFileCount_ = nullptr;
    QLabel *homeTransferCount_ = nullptr;
    QLabel *homeRecentCount_ = nullptr;
    QLabel *workspaceUserLabel_ = nullptr;
    QPushButton *refreshButton_ = nullptr;
    QPushButton *parentButton_ = nullptr;
    QPushButton *mkdirButton_ = nullptr;
    QPushButton *deleteButton_ = nullptr;
    QPushButton *uploadButton_ = nullptr;
    QPushButton *downloadButton_ = nullptr;
    QLabel *taskStatus_ = nullptr;
    QProgressBar *taskProgress_ = nullptr;
    NetworkClient client_;
    QStringList pathParts_;
    QPointer<TransferWorker> activeWorker_;
    bool registrationMode_ = false;
};
