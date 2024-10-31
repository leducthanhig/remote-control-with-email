#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    connect(ui->pushButton_logout, &QPushButton::clicked, this, &MainWindow::logout);

    view = new QWebEngineView(this);
    stackedWidget = new QStackedWidget(this);
    stackedWidget->addWidget(ui->centralwidget);
    stackedWidget->addWidget(view);
    setCentralWidget(stackedWidget);

    login();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::login() {
    ifstream tokensFile(TOKENS_PATH);
    if (!tokensFile.is_open()) {
        ifstream fi(CREDENTIALS_PATH);
        json credentials = json::parse(fi)["installed"];
        fi.close();

        QString authUrl = QString("%1?client_id=%2&redirect_uri=%3&response_type=code&scope=%4").arg(
            QString::fromStdString(credentials["auth_uri"].get<string>()),
            QString::fromStdString(credentials["client_id"].get<string>()),
            QString::fromStdString(credentials["redirect_uris"][0].get<string>()),
            GOOGLEAPI_AUTH_SCOPE_URL
        );

        view->setUrl(QUrl(authUrl));
        stackedWidget->setCurrentIndex(1);

        connect(view, &QWebEngineView::urlChanged, this, [this, credentials](const QUrl& url) {
            if (url.toString().startsWith(QString::fromStdString(credentials["redirect_uris"][0].get<string>()))) {
                QUrlQuery query(url);
                QString code = query.queryItemValue("code");
                if (!code.isEmpty()) {
                    if (view) {
                        view->close();
                        stackedWidget->setCurrentIndex(0);
                    }
                    getAccessToken(code.toStdString());
                    ui->label_account_info->setText(QString::fromStdString(getUserEmailAddress()));
                }
            }
        });
        
        tokensFile.close();
    }
    else {
        ui->label_account_info->setText(QString::fromStdString(getUserEmailAddress()));
    }
}

void MainWindow::logout() {
    QFile::remove(TOKENS_PATH);
    login();
}

void MainWindow::startSession() {
    string res = WSAInitiate();
    if (res.size()) {
        ui->textBrowser_status->append(res.c_str());
        WSACleanup();
        return;
    }

    if (!checkMailbox()) {
        WSACleanup();
        return;
    }

    sleeping = 1;

    /*connect(ui->textBrowser_status, &QTextBrowser::textChanged, this, []() {

    });*/
}

bool MainWindow::checkMailbox() {
    try {
        string acceptAddress = ui->lineEdit_acceptAddress->text().toStdString();
        string q = "from:" + acceptAddress + "%20is:unread";
        
        // Get mail list from inbox
        ui->textBrowser_status->append("\nChecking mailbox...\n");
        string response = getMailList(q);
        if (response.find("error") != string::npos) {
            ui->textBrowser_status->append("\nFailed to get mail list.\n");
        }
        json mailList = json::parse(getMailList(q));

        if (mailList["resultSizeEstimate"] != 0) {
            // Get mail data
            ui->textBrowser_status->append("\nGetting mail's data...\n");
            if (response.find("error") == string::npos) {
                ui->textBrowser_status->append("\nGet mail data successfully.\n");
            }
            else {
                ui->textBrowser_status->append("\nFailed to get mail data.\n");
            }
            json mailData = json::parse(getMail(mailList["messages"].back()["id"]));

            // Get and print out the mail subject and a snippet of the body
            string subject = getMailSubject(mailData);
            string body = mailData["snippet"];
            if (subject == "") subject = "(empty)";
            if (body == "") body = "(empty)";
            ostringstream oss;
            oss << "\nFrom: " << ui->lineEdit_acceptAddress->text().toStdString() << "\nSubject: " << subject << "\nBody: " << body << endl;
            ui->textBrowser_status->append(oss.str().c_str());

            // Connect to server for sending message
            string receivedMessage, receivedFilePath;
            handleMessage(ui->lineEdit_port->text().toInt(), subject, body, receivedMessage, receivedFilePath);

            // Trash the handled mail
            ui->textBrowser_status->append("\nTrashing mail...\n");
            response = trashMail(mailList["messages"].back()["id"]);
            if (response.find("error") == string::npos) {
                ui->textBrowser_status->append("\nTrashed mail successfully.\n");
            }
            else {
                ui->textBrowser_status->append("\nFailed to trash mail.\n");
            }

            // Send back to the sender the server message
            response = sendMail(acceptAddress, getUserEmailAddress(), "Server response", receivedMessage, receivedFilePath);
            if (response.find("error") == string::npos) {
                ui->textBrowser_status->append("\nSent mail successfully.\n");
            }
            else {
                ui->textBrowser_status->append("\nFailed to send mail.\n");
            }

            // Return 0 if the mail subject starts with 'exit'
            return subject.find("exit");
        }
        else {
            ui->textBrowser_status->append("\nNo unread mails found!\n");
            return 1;
        }
    }
    catch (const exception& e) {
        ostringstream oss;
        oss << "\nException: " << e.what() << endl;
        ui->textBrowser_status->append(oss.str().c_str());
        return 0;
    }
}