#include "mainwindow.h"

#include "transferworker.h"

#include <QApplication>
#include <QButtonGroup>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFont>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("NetDisk Qt 客户端"));
    resize(1100, 760);
    setMinimumSize(900, 620);
    setStyleSheet(QStringLiteral(R"(
        QMainWindow { background: #f5f7fc; }
        QWidget#loginPage {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                        stop:0 #eef2ff, stop:0.55 #f7f8ff, stop:1 #e9edff);
        }
        QFrame#loginCard {
            background: #ffffff;
            border: 1px solid rgba(255,255,255,210);
            border-radius: 18px;
        }
        QLabel#logoMark {
            color: #ffffff;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                        stop:0 #4d70ff, stop:1 #7e8cff);
            border-radius: 24px;
            font-size: 26px;
            font-weight: 700;
        }
        QLabel#brandTitle { color: #17213d; font-size: 25px; font-weight: 700; }
        QLabel#brandSubtitle { color: #8a93aa; font-size: 12px; }
        QLabel#loginTabActive {
            color: #315cf6;
            font-size: 15px;
            font-weight: 600;
            border-bottom: 2px solid #4e6fff;
            padding: 0 0 12px 0;
        }
        QLabel#loginTabDisabled { color: #b9bfce; font-size: 15px; padding: 0 0 12px 0; }
        QLabel#loginSubtitle { color: #6d7690; font-size: 13px; }
        QLineEdit#loginInput, QSpinBox#loginPort {
            background: #f7f8fb;
            border: 1px solid #eef0f5;
            border-radius: 10px;
            color: #26304b;
            padding: 11px 13px;
            selection-background-color: #6d83ff;
        }
        QLineEdit#loginInput:focus, QSpinBox#loginPort:focus {
            background: #ffffff;
            border: 1px solid #6880ff;
        }
        QPushButton#primaryButton {
            color: #ffffff;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                        stop:0 #456bff, stop:1 #8290ff);
            border: none;
            border-radius: 10px;
            font-size: 15px;
            font-weight: 600;
            min-height: 42px;
        }
        QPushButton#primaryButton:hover { background: #3f63ed; }
        QPushButton#primaryButton:pressed { background: #324fc4; }
        QPushButton#secondaryButton {
            color: #5575ff;
            background: transparent;
            border: none;
            font-size: 13px;
            min-height: 30px;
        }
        QPushButton#secondaryButton:hover { color: #294fe5; }
        QLabel#loginHint { color: #a1a8b8; font-size: 12px; }
        QLabel#loginStatus { color: #e05b67; font-size: 12px; }
        QWidget#filePage { background: #f5f7fc; }
        QTableWidget {
            background: #ffffff;
            alternate-background-color: #fbfcff;
            border: 1px solid #e8ebf3;
            border-radius: 12px;
            gridline-color: #f0f2f7;
            selection-background-color: #e9edff;
            selection-color: #27335a;
        }
        QHeaderView::section {
            background: #f7f8fc;
            color: #7d879e;
            border: none;
            border-bottom: 1px solid #e8ebf3;
            padding: 8px;
            font-weight: 600;
        }
        QPushButton {
            background: #ffffff;
            border: 1px solid #dfe4ef;
            border-radius: 8px;
            color: #4b5670;
            padding: 7px 14px;
        }
        QPushButton:hover { background: #eef2ff; border-color: #9eafff; color: #385be9; }
        QPushButton:disabled { color: #b9bfcb; background: #f5f6f9; }
        QLineEdit {
            background: #ffffff;
            border: 1px solid #dfe4ef;
            border-radius: 8px;
            padding: 8px 10px;
            color: #26304b;
        }
        QProgressBar {
            background: #e9edf6;
            border: none;
            border-radius: 5px;
            text-align: center;
            color: #ffffff;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                        stop:0 #4d6fff, stop:1 #8795ff);
            border-radius: 5px;
        }
        QWidget#workspacePage { background: #111216; color: #f0f1f5; }
        QWidget#sidebar { background: #202126; }
        QLabel#workspaceLogo {
            color: #ffffff;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                        stop:0 #4d70ff, stop:1 #8995ff);
            border-radius: 20px;
            font-size: 23px;
            font-weight: 700;
        }
        QPushButton#navButton {
            background: transparent;
            border: none;
            border-radius: 10px;
            color: #a9abb6;
            font-size: 12px;
            padding: 9px 3px;
            min-height: 52px;
        }
        QPushButton#navButton:hover { background: #2e3037; color: #ffffff; }
        QPushButton#navButton:checked { background: #3d3e46; color: #ffffff; }
        QLabel#workspaceTitle { color: #ffffff; font-size: 22px; font-weight: 700; }
        QLabel#workspaceSubTitle { color: #8c8f9b; font-size: 12px; }
        QLabel#workspaceUser { color: #dedfe5; font-size: 13px; }
        QLineEdit#searchBox {
            background: #1b1c20;
            border: 1px solid #282a32;
            border-radius: 9px;
            color: #e8e9ef;
            padding: 9px 13px;
        }
        QFrame#statCard {
            background: #191a20;
            border: 1px solid #272932;
            border-radius: 12px;
        }
        QLabel#statValue { color: #ffffff; font-size: 25px; font-weight: 700; }
        QLabel#statLabel { color: #9295a1; font-size: 12px; }
        QFrame#darkPanel {
            background: #191a20;
            border: 1px solid #272932;
            border-radius: 12px;
        }
        QLabel#panelTitle { color: #ffffff; font-size: 16px; font-weight: 600; }
        QWidget#workspacePage QTableWidget {
            background: #17181d;
            alternate-background-color: #1a1b21;
            color: #e8e9ef;
            border: 1px solid #282a32;
            gridline-color: #24262d;
            selection-background-color: #303b66;
            selection-color: #ffffff;
        }
        QWidget#workspacePage QHeaderView::section {
            background: #202126;
            color: #9b9eaa;
            border-bottom: 1px solid #2b2d35;
        }
        QWidget#workspacePage QLineEdit {
            background: #1b1c20;
            border-color: #2c2f39;
            color: #e8e9ef;
        }
        QWidget#workspacePage QPushButton {
            background: #202126;
            border-color: #343742;
            color: #d0d2dc;
        }
        QWidget#workspacePage QPushButton:hover { background: #30323b; border-color: #6377d9; color: #ffffff; }
        QWidget#workspacePage QPushButton#primaryButton { color: #ffffff; border: none; }
    )"));
    pages_ = new QStackedWidget(this);
    setCentralWidget(pages_);
    buildLoginPage();
    buildWorkspacePage();
    setLoggedInUi(false);
}

MainWindow::~MainWindow()
{
    if (activeWorker_) {
        activeWorker_->requestInterruption();
        activeWorker_->wait();
    }
    client_.logout();
}

void MainWindow::buildLoginPage()
{
    loginPage_ = new QWidget(this);
    loginPage_->setObjectName(QStringLiteral("loginPage"));
    QVBoxLayout *outer = new QVBoxLayout(loginPage_);
    outer->setContentsMargins(28, 24, 28, 24);
    outer->addStretch(2);

    QHBoxLayout *brand = new QHBoxLayout();
    brand->setSpacing(13);
    brand->setSizeConstraint(QLayout::SetFixedSize);
    QLabel *logo = new QLabel(QStringLiteral("N"), loginPage_);
    logo->setObjectName(QStringLiteral("logoMark"));
    logo->setAlignment(Qt::AlignCenter);
    logo->setFixedSize(48, 48);
    QVBoxLayout *brandText = new QVBoxLayout();
    brandText->setSpacing(2);
    QLabel *brandTitle = new QLabel(QStringLiteral("NetDisk 云盘"), loginPage_);
    brandTitle->setObjectName(QStringLiteral("brandTitle"));
    QLabel *brandSubtitle = new QLabel(QStringLiteral("安全、可靠、便捷的个人文件空间"), loginPage_);
    brandSubtitle->setObjectName(QStringLiteral("brandSubtitle"));
    brandText->addWidget(brandTitle);
    brandText->addWidget(brandSubtitle);
    brand->addWidget(logo);
    brand->addLayout(brandText);
    outer->addLayout(brand);
    outer->setAlignment(brand, Qt::AlignHCenter);
    outer->addSpacing(24);

    QFrame *card = new QFrame(loginPage_);
    card->setObjectName(QStringLiteral("loginCard"));
    card->setMaximumWidth(430);
    card->setMinimumWidth(390);
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(34);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(54, 72, 140, 38));
    card->setGraphicsEffect(shadow);
    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(30, 26, 30, 25);
    cardLayout->setSpacing(14);

    loginPageBackButton_ = new QPushButton(QStringLiteral("‹  返回登录"), card);
    loginPageBackButton_->setObjectName(QStringLiteral("secondaryButton"));
    loginPageBackButton_->setVisible(false);
    cardLayout->addWidget(loginPageBackButton_, 0, Qt::AlignLeft);

    QHBoxLayout *tabs = new QHBoxLayout();
    tabs->setSpacing(28);
    QLabel *activeTab = new QLabel(QStringLiteral("账号登录"), card);
    activeTab->setObjectName(QStringLiteral("loginTabActive"));
    QLabel *disabledTab = new QLabel(QStringLiteral("扫码登录"), card);
    disabledTab->setObjectName(QStringLiteral("loginTabDisabled"));
    tabs->addWidget(activeTab);
    tabs->addWidget(disabledTab);
    tabs->addStretch();
    cardLayout->addLayout(tabs);

    loginFormTitle_ = new QLabel(QStringLiteral("账号登录"), card);
    loginFormTitle_->setObjectName(QStringLiteral("loginSubtitle"));
    cardLayout->addWidget(loginFormTitle_);
    loginFormSubtitle_ = new QLabel(QStringLiteral("登录后管理你的云端文件"), card);
    loginFormSubtitle_->setObjectName(QStringLiteral("loginHint"));
    cardLayout->addWidget(loginFormSubtitle_);

    usernameEdit_ = new QLineEdit(card);
    usernameEdit_->setObjectName(QStringLiteral("loginInput"));
    usernameEdit_->setPlaceholderText(QStringLiteral("请输入用户名"));
    usernameEdit_->setMinimumHeight(43);
    passwordEdit_ = new QLineEdit(card);
    passwordEdit_->setObjectName(QStringLiteral("loginInput"));
    passwordEdit_->setPlaceholderText(QStringLiteral("请输入密码"));
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setMinimumHeight(43);
    confirmPasswordEdit_ = new QLineEdit(card);
    confirmPasswordEdit_->setObjectName(QStringLiteral("loginInput"));
    confirmPasswordEdit_->setPlaceholderText(QStringLiteral("请再次输入密码"));
    confirmPasswordEdit_->setEchoMode(QLineEdit::Password);
    confirmPasswordEdit_->setMinimumHeight(43);
    confirmPasswordEdit_->setVisible(false);
    cardLayout->addWidget(usernameEdit_);
    cardLayout->addWidget(passwordEdit_);
    cardLayout->addWidget(confirmPasswordEdit_);

    QHBoxLayout *serverRow = new QHBoxLayout();
    serverRow->setSpacing(8);
    hostEdit_ = new QLineEdit(QStringLiteral("127.0.0.1"), card);
    hostEdit_->setObjectName(QStringLiteral("loginInput"));
    hostEdit_->setPlaceholderText(QStringLiteral("服务器地址"));
    hostEdit_->setMinimumHeight(37);
    portEdit_ = new QSpinBox(card);
    portEdit_->setObjectName(QStringLiteral("loginPort"));
    portEdit_->setRange(1, 65535);
    portEdit_->setValue(2000);
    portEdit_->setMinimumHeight(37);
    portEdit_->setFixedWidth(94);
    serverRow->addWidget(hostEdit_, 1);
    serverRow->addWidget(portEdit_);
    cardLayout->addWidget(new QLabel(QStringLiteral("服务端连接"), card));
    cardLayout->addLayout(serverRow);

    loginButton_ = new QPushButton(QStringLiteral("登录"), card);
    loginButton_->setObjectName(QStringLiteral("primaryButton"));
    registerButton_ = new QPushButton(QStringLiteral("还没有账号？注册并登录"), card);
    registerButton_->setObjectName(QStringLiteral("secondaryButton"));
    cardLayout->addSpacing(2);
    cardLayout->addWidget(loginButton_);
    cardLayout->addWidget(registerButton_);

    loginStatus_ = new QLabel(card);
    loginStatus_->setObjectName(QStringLiteral("loginStatus"));
    loginStatus_->setAlignment(Qt::AlignCenter);
    loginStatus_->setWordWrap(true);
    cardLayout->addWidget(loginStatus_);
    QLabel *hint = new QLabel(QStringLiteral("TLS 加密连接 · 密码不会明文保存"), card);
    hint->setObjectName(QStringLiteral("loginHint"));
    hint->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(hint);

    outer->addWidget(card, 0, Qt::AlignHCenter);
    outer->addSpacing(16);
    QLabel *footer = new QLabel(QStringLiteral("NetDisk  ·  让文件随时可用"), loginPage_);
    footer->setObjectName(QStringLiteral("loginHint"));
    footer->setAlignment(Qt::AlignCenter);
    outer->addWidget(footer);
    outer->addStretch(2);

    connect(passwordEdit_, &QLineEdit::returnPressed, this, &MainWindow::login);
    connect(confirmPasswordEdit_, &QLineEdit::returnPressed, this, &MainWindow::registerUser);
    connect(loginButton_, &QPushButton::clicked, this, &MainWindow::login);
    connect(registerButton_, &QPushButton::clicked, this, &MainWindow::registerUser);
    connect(loginPageBackButton_, &QPushButton::clicked, this, &MainWindow::resetLoginMode);
    pages_->addWidget(loginPage_);
}

void MainWindow::buildWorkspacePage()
{
    workspacePage_ = new QWidget(this);
    workspacePage_->setObjectName(QStringLiteral("workspacePage"));
    QHBoxLayout *root = new QHBoxLayout(workspacePage_);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QWidget *sidebar = new QWidget(workspacePage_);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(88);
    QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(10, 18, 10, 18);
    sidebarLayout->setSpacing(9);
    QLabel *logo = new QLabel(QStringLiteral("N"), sidebar);
    logo->setObjectName(QStringLiteral("workspaceLogo"));
    logo->setAlignment(Qt::AlignCenter);
    logo->setFixedSize(42, 42);
    sidebarLayout->addWidget(logo, 0, Qt::AlignHCenter);
    sidebarLayout->addSpacing(24);

    auto createNavButton = [sidebar](const QString &text) {
        QPushButton *button = new QPushButton(text, sidebar);
        button->setObjectName(QStringLiteral("navButton"));
        button->setCheckable(true);
        return button;
    };
    homeNavButton_ = createNavButton(QStringLiteral("⌂\n首页"));
    filesNavButton_ = createNavButton(QStringLiteral("▣\n文件"));
    transfersNavButton_ = createNavButton(QStringLiteral("⇅\n传输"));
    sidebarLayout->addWidget(homeNavButton_);
    sidebarLayout->addWidget(filesNavButton_);
    sidebarLayout->addWidget(transfersNavButton_);
    sidebarLayout->addStretch();
    QLabel *security = new QLabel(QStringLiteral("TLS\n安全连接"), sidebar);
    security->setAlignment(Qt::AlignCenter);
    security->setStyleSheet(QStringLiteral("color:#777a87;font-size:10px;"));
    sidebarLayout->addWidget(security);
    root->addWidget(sidebar);

    QWidget *content = new QWidget(workspacePage_);
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(26, 18, 26, 22);
    contentLayout->setSpacing(16);
    QHBoxLayout *topBar = new QHBoxLayout();
    QLineEdit *search = new QLineEdit(content);
    search->setObjectName(QStringLiteral("searchBox"));
    search->setPlaceholderText(QStringLiteral("搜索云盘内文件"));
    search->setFixedWidth(320);
    topBar->addWidget(search);
    topBar->addStretch();
    workspaceUserLabel_ = new QLabel(content);
    workspaceUserLabel_->setObjectName(QStringLiteral("workspaceUser"));
    topBar->addWidget(workspaceUserLabel_);
    QPushButton *logoutButton = new QPushButton(QStringLiteral("退出登录"), content);
    topBar->addWidget(logoutButton);
    contentLayout->addLayout(topBar);

    contentPages_ = new QStackedWidget(content);
    contentLayout->addWidget(contentPages_, 1);
    root->addWidget(content, 1);

    buildHomePage();
    buildFilePage();
    buildTransferPage();
    contentPages_->addWidget(homePage_);
    contentPages_->addWidget(filePage_);
    contentPages_->addWidget(transferPage_);

    connect(homeNavButton_, &QPushButton::clicked, this, &MainWindow::showHomePage);
    connect(filesNavButton_, &QPushButton::clicked, this, &MainWindow::showFilesPage);
    connect(transfersNavButton_, &QPushButton::clicked, this, &MainWindow::showTransfersPage);
    connect(logoutButton, &QPushButton::clicked, [this]() {
        client_.logout();
        setLoggedInUi(false);
    });
    pages_->addWidget(workspacePage_);
}

void MainWindow::buildHomePage()
{
    homePage_ = new QWidget(this);
    homePage_->setObjectName(QStringLiteral("contentPage"));
    QVBoxLayout *layout = new QVBoxLayout(homePage_);
    layout->setContentsMargins(8, 4, 8, 4);
    QLabel *title = new QLabel(QStringLiteral("首页"), homePage_);
    title->setObjectName(QStringLiteral("workspaceTitle"));
    layout->addWidget(title);
    QLabel *subtitle = new QLabel(QStringLiteral("欢迎回来，这里记录你的云端文件活动"), homePage_);
    subtitle->setObjectName(QStringLiteral("workspaceSubTitle"));
    layout->addWidget(subtitle);

    QHBoxLayout *stats = new QHBoxLayout();
    auto addStatCard = [this, stats](const QString &label, QLabel **value) {
        QFrame *card = new QFrame(homePage_);
        card->setObjectName(QStringLiteral("statCard"));
        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(18, 15, 18, 15);
        QLabel *number = new QLabel(QStringLiteral("0"), card);
        number->setObjectName(QStringLiteral("statValue"));
        QLabel *caption = new QLabel(label, card);
        caption->setObjectName(QStringLiteral("statLabel"));
        cardLayout->addWidget(number);
        cardLayout->addWidget(caption);
        stats->addWidget(card);
        *value = number;
    };
    addStatCard(QStringLiteral("当前目录文件"), &homeFileCount_);
    addStatCard(QStringLiteral("传输任务"), &homeTransferCount_);
    addStatCard(QStringLiteral("最近操作"), &homeRecentCount_);
    layout->addLayout(stats);
    layout->addSpacing(10);

    QFrame *historyPanel = new QFrame(homePage_);
    historyPanel->setObjectName(QStringLiteral("darkPanel"));
    QVBoxLayout *historyLayout = new QVBoxLayout(historyPanel);
    historyLayout->setContentsMargins(16, 14, 16, 16);
    QLabel *historyTitle = new QLabel(QStringLiteral("最近活动"), historyPanel);
    historyTitle->setObjectName(QStringLiteral("panelTitle"));
    historyLayout->addWidget(historyTitle);
    historyTable_ = new QTableWidget(0, 4, historyPanel);
    historyTable_->setHorizontalHeaderLabels({QStringLiteral("时间"), QStringLiteral("操作"),
                                              QStringLiteral("详情"), QStringLiteral("状态")});
    historyTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    historyTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    historyTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    historyTable_->horizontalHeader()->setStretchLastSection(true);
    historyLayout->addWidget(historyTable_, 1);
    layout->addWidget(historyPanel, 1);
}

void MainWindow::buildTransferPage()
{
    transferPage_ = new QWidget(this);
    transferPage_->setObjectName(QStringLiteral("contentPage"));
    QVBoxLayout *layout = new QVBoxLayout(transferPage_);
    layout->setContentsMargins(8, 4, 8, 4);
    QHBoxLayout *heading = new QHBoxLayout();
    QLabel *title = new QLabel(QStringLiteral("传输列表"), transferPage_);
    title->setObjectName(QStringLiteral("workspaceTitle"));
    heading->addWidget(title);
    heading->addStretch();
    QLabel *hint = new QLabel(QStringLiteral("上传和下载任务会在这里显示"), transferPage_);
    hint->setObjectName(QStringLiteral("workspaceSubTitle"));
    heading->addWidget(hint);
    layout->addLayout(heading);

    QFrame *panel = new QFrame(transferPage_);
    panel->setObjectName(QStringLiteral("darkPanel"));
    QVBoxLayout *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(16, 16, 16, 16);
    taskTable_ = new QTableWidget(0, 3, panel);
    taskTable_->setHorizontalHeaderLabels({QStringLiteral("任务"), QStringLiteral("进度"), QStringLiteral("状态")});
    taskTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    taskTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    taskTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    taskTable_->horizontalHeader()->setStretchLastSection(true);
    panelLayout->addWidget(taskTable_, 1);
    taskStatus_ = new QLabel(QStringLiteral("暂无传输任务"), panel);
    taskStatus_->setObjectName(QStringLiteral("workspaceSubTitle"));
    taskProgress_ = new QProgressBar(panel);
    taskProgress_->setRange(0, 1000);
    taskProgress_->setValue(0);
    panelLayout->addWidget(taskStatus_);
    panelLayout->addWidget(taskProgress_);
    layout->addWidget(panel, 1);
}

void MainWindow::buildFilePage()
{
    filePage_ = new QWidget(this);
    filePage_->setObjectName(QStringLiteral("filePage"));
    QVBoxLayout *layout = new QVBoxLayout(filePage_);
    QHBoxLayout *top = new QHBoxLayout();
    userLabel_ = new QLabel(filePage_); pathLabel_ = new QLabel(QStringLiteral("/"), filePage_);
    top->addWidget(userLabel_); top->addSpacing(20);
    top->addWidget(new QLabel(QStringLiteral("远程目录："), filePage_));
    top->addWidget(pathLabel_, 1);
    QPushButton *logoutButton = new QPushButton(QStringLiteral("退出登录"), filePage_);
    top->addWidget(logoutButton); layout->addLayout(top);

    QHBoxLayout *local = new QHBoxLayout();
    local->addWidget(new QLabel(QStringLiteral("本地目录："), filePage_));
    localDirectoryEdit_ = new QLineEdit(QDir::homePath(), filePage_);
    QPushButton *chooseLocal = new QPushButton(QStringLiteral("选择"), filePage_);
    local->addWidget(localDirectoryEdit_, 1); local->addWidget(chooseLocal); layout->addLayout(local);

    QHBoxLayout *actions = new QHBoxLayout();
    refreshButton_ = new QPushButton(QStringLiteral("刷新"), filePage_);
    parentButton_ = new QPushButton(QStringLiteral("返回上级"), filePage_);
    mkdirButton_ = new QPushButton(QStringLiteral("新建文件夹"), filePage_);
    deleteButton_ = new QPushButton(QStringLiteral("删除"), filePage_);
    uploadButton_ = new QPushButton(QStringLiteral("上传文件"), filePage_);
    downloadButton_ = new QPushButton(QStringLiteral("下载选中文件"), filePage_);
    uploadButton_->setObjectName(QStringLiteral("primaryButton"));
    downloadButton_->setObjectName(QStringLiteral("primaryButton"));
    actions->addWidget(refreshButton_); actions->addWidget(parentButton_);
    actions->addWidget(mkdirButton_); actions->addWidget(deleteButton_); actions->addStretch();
    actions->addWidget(uploadButton_); actions->addWidget(downloadButton_); layout->addLayout(actions);

    remoteTable_ = new QTableWidget(filePage_); remoteTable_->setColumnCount(3);
    remoteTable_->setHorizontalHeaderLabels({QStringLiteral("名称"), QStringLiteral("类型"), QStringLiteral("大小")});
    remoteTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    remoteTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    remoteTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    remoteTable_->horizontalHeader()->setStretchLastSection(true);
    remoteTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    localTable_ = new QTableWidget(filePage_);
    localTable_->setColumnCount(3);
    localTable_->setHorizontalHeaderLabels({QStringLiteral("本地文件"),
                                            QStringLiteral("类型"),
                                            QStringLiteral("大小")});
    localTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    localTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    localTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    localTable_->horizontalHeader()->setStretchLastSection(true);
    localTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    QSplitter *fileSplitter = new QSplitter(Qt::Horizontal, filePage_);
    QWidget *remotePanel = new QWidget(fileSplitter);
    QVBoxLayout *remoteLayout = new QVBoxLayout(remotePanel);
    remoteLayout->setContentsMargins(0, 0, 0, 0);
    remoteLayout->addWidget(new QLabel(QStringLiteral("远程网盘"), remotePanel));
    remoteLayout->addWidget(remoteTable_);
    QWidget *localPanel = new QWidget(fileSplitter);
    QVBoxLayout *localLayout = new QVBoxLayout(localPanel);
    localLayout->setContentsMargins(0, 0, 0, 0);
    localLayout->addWidget(new QLabel(QStringLiteral("本地文件夹"), localPanel));
    localLayout->addWidget(localTable_);
    fileSplitter->addWidget(remotePanel);
    fileSplitter->addWidget(localPanel);
    fileSplitter->setStretchFactor(0, 1);
    fileSplitter->setStretchFactor(1, 1);
    layout->addWidget(fileSplitter, 1);

    connect(logoutButton, &QPushButton::clicked, [this]() { client_.logout(); setLoggedInUi(false); });
    connect(chooseLocal, &QPushButton::clicked, this, &MainWindow::selectLocalDirectory);
    connect(localDirectoryEdit_, &QLineEdit::editingFinished, this, &MainWindow::refreshLocal);
    connect(refreshButton_, &QPushButton::clicked, this, &MainWindow::refreshRemote);
    connect(parentButton_, &QPushButton::clicked, this, &MainWindow::goParent);
    connect(mkdirButton_, &QPushButton::clicked, this, &MainWindow::makeDirectory);
    connect(deleteButton_, &QPushButton::clicked, this, &MainWindow::deleteSelected);
    connect(uploadButton_, &QPushButton::clicked, this, &MainWindow::uploadFile);
    connect(downloadButton_, &QPushButton::clicked, this, &MainWindow::downloadFile);
    connect(remoteTable_, &QTableWidget::cellDoubleClicked, this, &MainWindow::tableDoubleClicked);
    connect(localTable_, &QTableWidget::cellDoubleClicked, this, &MainWindow::localTableDoubleClicked);
}

void MainWindow::setLoggedInUi(bool loggedIn)
{
    pages_->setCurrentWidget(loggedIn ? workspacePage_ : loginPage_);
    if (!loggedIn) { resetLoginMode(); usernameEdit_->setFocus(); return; }
    userLabel_->setText(QStringLiteral("用户：") + client_.username());
    workspaceUserLabel_->setText(QStringLiteral("用户：") + client_.username());
    pathParts_.clear(); updatePathLabel(); refreshRemote(); refreshLocal();
    showHomePage();
    updateHomeSummary();
}

void MainWindow::showHomePage()
{
    if (contentPages_ == nullptr) return;
    contentPages_->setCurrentWidget(homePage_);
    homeNavButton_->setChecked(true);
    filesNavButton_->setChecked(false);
    transfersNavButton_->setChecked(false);
    updateHomeSummary();
}

void MainWindow::showFilesPage()
{
    if (contentPages_ == nullptr) return;
    contentPages_->setCurrentWidget(filePage_);
    homeNavButton_->setChecked(false);
    filesNavButton_->setChecked(true);
    transfersNavButton_->setChecked(false);
    refreshRemote();
    refreshLocal();
}

void MainWindow::showTransfersPage()
{
    if (contentPages_ == nullptr) return;
    contentPages_->setCurrentWidget(transferPage_);
    homeNavButton_->setChecked(false);
    filesNavButton_->setChecked(false);
    transfersNavButton_->setChecked(true);
}

void MainWindow::login()
{
    if (registrationMode_) {
        registerUser();
        return;
    }
    loginStatus_->setText(QStringLiteral("正在建立 TLS 连接并登录…"));
    loginButton_->setEnabled(false); registerButton_->setEnabled(false);
    QString error;
    const bool ok = client_.login(hostEdit_->text(), static_cast<quint16>(portEdit_->value()),
                                  usernameEdit_->text(), passwordEdit_->text(), error);
    loginButton_->setEnabled(true); registerButton_->setEnabled(true);
    if (!ok) { loginStatus_->setText(QStringLiteral("登录失败：") + error); return; }
    appendHistory(QStringLiteral("登录"), QStringLiteral("登录 NetDisk 云盘"));
    setLoggedInUi(true);
}

void MainWindow::registerUser()
{
    if (!registrationMode_) {
        registrationMode_ = true;
        loginPageBackButton_->setVisible(true);
        loginFormTitle_->setText(QStringLiteral("创建你的 NetDisk 账号"));
        loginFormSubtitle_->setText(QStringLiteral("注册后即可安全管理云端文件"));
        confirmPasswordEdit_->setVisible(true);
        loginButton_->setText(QStringLiteral("注册并登录"));
        registerButton_->setText(QStringLiteral("已有账号？返回登录"));
        loginStatus_->clear();
        usernameEdit_->setFocus();
        return;
    }
    if (sender() == registerButton_) {
        resetLoginMode();
        return;
    }
    if (passwordEdit_->text() != confirmPasswordEdit_->text()) {
        loginStatus_->setText(QStringLiteral("两次输入的密码不一致"));
        return;
    }
    loginStatus_->setText(QStringLiteral("正在注册并登录…"));
    loginButton_->setEnabled(false); registerButton_->setEnabled(false);
    QString error;
    const bool ok = client_.registerAndLogin(hostEdit_->text(), static_cast<quint16>(portEdit_->value()),
                                             usernameEdit_->text(), passwordEdit_->text(), error);
    loginButton_->setEnabled(true); registerButton_->setEnabled(true);
    if (!ok) { loginStatus_->setText(QStringLiteral("注册失败：") + error); return; }
    appendHistory(QStringLiteral("注册"), QStringLiteral("创建 NetDisk 账号"));
    setLoggedInUi(true);
}

void MainWindow::resetLoginMode()
{
    registrationMode_ = false;
    loginPageBackButton_->setVisible(false);
    loginFormTitle_->setText(QStringLiteral("账号登录"));
    loginFormSubtitle_->setText(QStringLiteral("登录后管理你的云端文件"));
    confirmPasswordEdit_->clear();
    confirmPasswordEdit_->setVisible(false);
    loginButton_->setText(QStringLiteral("登录"));
    registerButton_->setText(QStringLiteral("还没有账号？注册"));
    loginStatus_->clear();
}

void MainWindow::appendHistory(const QString &operation, const QString &detail,
                               const QString &status)
{
    if (historyTable_ == nullptr) return;
    const int row = 0;
    historyTable_->insertRow(row);
    historyTable_->setItem(row, 0, new QTableWidgetItem(
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
    historyTable_->setItem(row, 1, new QTableWidgetItem(operation));
    historyTable_->setItem(row, 2, new QTableWidgetItem(detail));
    historyTable_->setItem(row, 3, new QTableWidgetItem(status));
    if (historyTable_->rowCount() > 50) historyTable_->removeRow(50);
    updateHomeSummary();
}

void MainWindow::updateHomeSummary()
{
    if (homeFileCount_ != nullptr) {
        homeFileCount_->setText(QString::number(remoteTable_ == nullptr ? 0 : remoteTable_->rowCount()));
    }
    if (homeTransferCount_ != nullptr) {
        homeTransferCount_->setText(QString::number(taskTable_ == nullptr ? 0 : taskTable_->rowCount()));
    }
    if (homeRecentCount_ != nullptr) {
        homeRecentCount_->setText(QString::number(historyTable_ == nullptr ? 0 : historyTable_->rowCount()));
    }
}

void MainWindow::refreshRemote()
{
    QVector<RemoteEntry> entries; QString error;
    if (!client_.list(entries, error)) { showError(QStringLiteral("刷新目录失败：") + error); return; }
    remoteTable_->setRowCount(0);
    for (const RemoteEntry &entry : entries) {
        const int row = remoteTable_->rowCount(); remoteTable_->insertRow(row);
        QTableWidgetItem *name = new QTableWidgetItem(entry.name);
        name->setData(Qt::UserRole, entry.type == QLatin1Char('d'));
        remoteTable_->setItem(row, 0, name);
        remoteTable_->setItem(row, 1, new QTableWidgetItem(entry.type == QLatin1Char('d') ? QStringLiteral("文件夹") : QStringLiteral("文件")));
        remoteTable_->setItem(row, 2, new QTableWidgetItem(entry.type == QLatin1Char('d') ? QStringLiteral("-") : formatSize(entry.size)));
    }
    updateHomeSummary();
}

void MainWindow::refreshLocal()
{
    if (localTable_ == nullptr) return;
    const QDir directory(localDirectoryEdit_->text());
    if (!directory.exists()) {
        localTable_->setRowCount(0);
        return;
    }
    localTable_->setRowCount(0);
    const QFileInfoList entries = directory.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &entry : entries) {
        const int row = localTable_->rowCount();
        localTable_->insertRow(row);
        QTableWidgetItem *name = new QTableWidgetItem(entry.fileName());
        name->setData(Qt::UserRole, entry.absoluteFilePath());
        name->setData(Qt::UserRole + 1, entry.isDir());
        localTable_->setItem(row, 0, name);
        localTable_->setItem(row, 1, new QTableWidgetItem(
            entry.isDir() ? QStringLiteral("文件夹") : QStringLiteral("文件")));
        localTable_->setItem(row, 2, new QTableWidgetItem(
            entry.isDir() ? QStringLiteral("-") : formatSize(static_cast<quint64>(entry.size()))));
    }
}

void MainWindow::makeDirectory()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("新建文件夹"), QStringLiteral("文件夹名称："), QLineEdit::Normal, QString(), &ok);
    if (!ok || name.isEmpty()) return;
    QString error;
    if (!client_.makeDirectory(name, error)) { showError(QStringLiteral("创建失败：") + error); return; }
    appendHistory(QStringLiteral("新建文件夹"), name);
    refreshRemote();
}

void MainWindow::goParent()
{
    if (client_.currentDirectory() == 0) return;
    quint64 ignored = 0; QString error;
    if (!client_.changeDirectory(QStringLiteral(".."), ignored, error)) { showError(QStringLiteral("返回上级目录失败：") + error); return; }
    if (!pathParts_.isEmpty()) pathParts_.removeLast(); updatePathLabel(); refreshRemote();
    appendHistory(QStringLiteral("浏览目录"), QStringLiteral("返回上级目录"));
}

void MainWindow::tableDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    if (row < 0 || row >= remoteTable_->rowCount()) return;
    QTableWidgetItem *item = remoteTable_->item(row, 0);
    if (item == nullptr || !item->data(Qt::UserRole).toBool()) return;
    quint64 ignored = 0; QString error;
    if (!client_.changeDirectory(item->text(), ignored, error)) { showError(QStringLiteral("进入目录失败：") + error); return; }
    pathParts_.append(item->text()); updatePathLabel(); refreshRemote();
    appendHistory(QStringLiteral("浏览目录"), QStringLiteral("进入 ") + item->text());
}

void MainWindow::localTableDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    if (row < 0 || row >= localTable_->rowCount()) return;
    QTableWidgetItem *item = localTable_->item(row, 0);
    if (item == nullptr || !item->data(Qt::UserRole + 1).toBool()) return;
    localDirectoryEdit_->setText(item->data(Qt::UserRole).toString());
    refreshLocal();
}

QString MainWindow::selectedRemoteName(bool *isDirectory) const
{
    const QList<QTableWidgetItem *> selected = remoteTable_->selectedItems();
    if (selected.isEmpty()) return QString();
    const QTableWidgetItem *item = remoteTable_->item(selected.first()->row(), 0);
    if (item == nullptr) return QString();
    if (isDirectory != nullptr) *isDirectory = item->data(Qt::UserRole).toBool();
    return item->text();
}

void MainWindow::deleteSelected()
{
    bool directory = false; const QString name = selectedRemoteName(&directory);
    if (name.isEmpty()) { showError(QStringLiteral("请先选择文件或文件夹")); return; }
    if (QMessageBox::question(this, QStringLiteral("确认删除"), QStringLiteral("确定删除“%1”吗？").arg(name)) != QMessageBox::Yes) return;
    QString error;
    if (!client_.removeEntry(name, directory, error)) { showError(QStringLiteral("删除失败：") + error); return; }
    appendHistory(QStringLiteral("删除"), name);
    refreshRemote();
}

void MainWindow::selectLocalDirectory()
{
    const QString directory = QFileDialog::getExistingDirectory(this, QStringLiteral("选择本地目录"), localDirectoryEdit_->text());
    if (!directory.isEmpty()) {
        localDirectoryEdit_->setText(directory);
        refreshLocal();
    }
}

void MainWindow::uploadFile()
{
    if (activeWorker_) { showError(QStringLiteral("当前已有传输任务，请等待完成后再操作")); return; }
    QString path;
    const QList<QTableWidgetItem *> selected = localTable_->selectedItems();
    if (!selected.isEmpty()) {
        const QTableWidgetItem *item = localTable_->item(selected.first()->row(), 0);
        if (item != nullptr && !item->data(Qt::UserRole + 1).toBool()) {
            path = item->data(Qt::UserRole).toString();
        }
    }
    if (path.isEmpty()) {
        path = QFileDialog::getOpenFileName(this, QStringLiteral("选择上传文件"), localDirectoryEdit_->text());
    }
    if (path.isEmpty()) return;
    const QString name = QFileInfo(path).fileName();
    if (name.contains(QRegularExpression(QStringLiteral("[\\s/\\\\]")))) { showError(QStringLiteral("当前协议不支持带空格或路径分隔符的文件名")); return; }
    TransferWorker *worker = new TransferWorker(TransferWorker::Upload, client_.host(), client_.port(), client_.token(), client_.currentDirectory(), path, name, false, this);
    startTransfer(worker, QStringLiteral("上传：") + name);
}

void MainWindow::downloadFile()
{
    if (activeWorker_) { showError(QStringLiteral("当前已有传输任务，请等待完成后再操作")); return; }
    bool directory = false; const QString name = selectedRemoteName(&directory);
    if (name.isEmpty() || directory) { showError(QStringLiteral("请选择一个远程文件")); return; }
    const QString directoryPath = localDirectoryEdit_->text();
    if (!QDir(directoryPath).exists()) { showError(QStringLiteral("本地目录不存在")); return; }
    const QString target = QDir(directoryPath).filePath(name);
    bool overwrite = false;
    if (QFile::exists(target)) {
        if (QMessageBox::question(this, QStringLiteral("文件已存在"), QStringLiteral("本地文件已存在，是否覆盖？")) != QMessageBox::Yes) return;
        overwrite = true;
    }
    TransferWorker *worker = new TransferWorker(TransferWorker::Download, client_.host(), client_.port(), client_.token(), client_.currentDirectory(), target, name, overwrite, this);
    startTransfer(worker, QStringLiteral("下载：") + name);
}

void MainWindow::startTransfer(TransferWorker *worker, const QString &title)
{
    activeWorker_ = worker;
    taskTable_->insertRow(0);
    taskTable_->setItem(0, 0, new QTableWidgetItem(title));
    taskTable_->setItem(0, 1, new QTableWidgetItem(QStringLiteral("0%")));
    taskTable_->setItem(0, 2, new QTableWidgetItem(QStringLiteral("准备中")));
    taskProgress_->setValue(0); taskStatus_->setText(QStringLiteral("准备开始：") + title);
    connect(worker, &TransferWorker::progress, this, &MainWindow::transferProgress);
    connect(worker, &TransferWorker::status, this, &MainWindow::transferStatus);
    connect(worker, &TransferWorker::succeeded, this, &MainWindow::transferSucceeded);
    connect(worker, &TransferWorker::failed, this, &MainWindow::transferFailed);
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    connect(worker, &QThread::finished, this, [this]() {
        activeWorker_.clear(); uploadButton_->setEnabled(true); downloadButton_->setEnabled(true);
    });
    uploadButton_->setEnabled(false); downloadButton_->setEnabled(false); worker->start();
    showTransfersPage();
}

void MainWindow::transferProgress(quint64 completed, quint64 total)
{
    const int value = total == 0 ? 1000 : static_cast<int>(qMin<quint64>(1000, completed * 1000 / total));
    taskProgress_->setValue(value);
    if (taskTable_->rowCount() > 0) taskTable_->item(0, 1)->setText(QString::number(value / 10.0, 'f', 1) + QStringLiteral("%"));
}

void MainWindow::transferStatus(const QString &text)
{
    taskStatus_->setText(text);
    if (taskTable_->rowCount() > 0) taskTable_->item(0, 2)->setText(text);
}

void MainWindow::transferSucceeded(const QString &text)
{
    transferStatus(QStringLiteral("完成：") + text); taskProgress_->setValue(1000); refreshRemote();
    appendHistory(QStringLiteral("文件传输"), text);
    updateHomeSummary();
}

void MainWindow::transferFailed(const QString &text)
{
    transferStatus(QStringLiteral("失败：") + text); showError(text);
    appendHistory(QStringLiteral("文件传输"), text, QStringLiteral("失败"));
}

void MainWindow::showError(const QString &message)
{
    statusBar()->showMessage(message, 8000); QMessageBox::warning(this, QStringLiteral("操作失败"), message);
}

QString MainWindow::formatSize(quint64 size) const
{
    static const QStringList units = {QStringLiteral("B"), QStringLiteral("KB"), QStringLiteral("MB"), QStringLiteral("GB")};
    double value = static_cast<double>(size); int unit = 0;
    while (value >= 1024.0 && unit < units.size() - 1) { value /= 1024.0; ++unit; }
    return QString::number(value, 'f', unit == 0 ? 0 : 2) + QLatin1Char(' ') + units[unit];
}

void MainWindow::updatePathLabel()
{
    QString path = QStringLiteral("/");
    if (!pathParts_.isEmpty()) path += pathParts_.join(QLatin1Char('/')) + QLatin1Char('/');
    pathLabel_->setText(path); parentButton_->setEnabled(client_.currentDirectory() != 0);
}
