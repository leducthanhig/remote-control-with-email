#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    // Initiate socket
    string res = WSAInitiate();
    if (res.size()) {
        notifyError(res);
        return;
    }
    
    // Setup properties
    ui->setupUi(this);
    
    stopped = 0;
    remaining = 0;
    timer = new QTimer(this);
    
    webView = new QWebEngineView(this);
    stackedWidget = new QStackedWidget(this);
    stackedWidget->addWidget(ui->centralwidget);
    stackedWidget->addWidget(webView);
    setCentralWidget(stackedWidget);

    // Set validator for email address lineEdit
    QRegularExpression emailRegex(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
    QRegularExpressionValidator* emailValidator = new QRegularExpressionValidator(emailRegex, this);
    ui->lineEdit_acceptAddress->setValidator(emailValidator);

    // Set connections for signals to slots
    setConnections();
    
    // Perform logging into Google Account
    login();
}

MainWindow::~MainWindow() {
    // Delocate pointer
    delete ui; 

    // Clean up socket
    WSACleanup();
}

void MainWindow::login() {
    // Check if token informations exist
    ifstream tokensFile(TOKENS_PATH);
    if (!tokensFile.is_open()) {
        // Get credential informations from file
        ifstream fi(CREDENTIALS_PATH);
        json credentials = json::parse(fi)["installed"];
        fi.close();

        // Create authorization url
        QString authUrl = QString("%1?client_id=%2&redirect_uri=%3&response_type=code&scope=%4").arg(
            QString::fromStdString(credentials["auth_uri"].get<string>()),
            QString::fromStdString(credentials["client_id"].get<string>()),
            QString::fromStdString(credentials["redirect_uris"][0].get<string>()),
            GOOGLEAPI_AUTH_SCOPE_URL
        );

        // Open the url in the internal browser
        webView->setUrl(QUrl(authUrl));

        // Change the central widget to the browser
        stackedWidget->setCurrentIndex(1);

        // Connect to slot for getting authorization code
        connect(webView, &QWebEngineView::urlChanged, this, [this, credentials](const QUrl& url) {
            if (url.toString().startsWith(QString::fromStdString(credentials["redirect_uris"][0].get<string>()))) {
                // Extract the code
                QUrlQuery query(url);
                QString code = query.queryItemValue("code");
                if (!code.isEmpty()) {
                    if (webView) {
                        // Close the browser
                        webView->close();

                        // Turn back to the initial view
                        stackedWidget->setCurrentIndex(0);
                    }
                    // Get token informations by using the extracted code
                    getAccessToken(code.toStdString());

                    // Get and display the current user account
                    ui->label_account_info->setText(QString::fromStdString(getUserEmailAddress()));
                }
            }
        });
        
        tokensFile.close();
    }
    else {
        // Get and display the current user account
        ui->label_account_info->setText(QString::fromStdString(getUserEmailAddress()));
    }
}

void MainWindow::setConnections() {
    connect(ui->pushButton_start, &QPushButton::clicked, this, [this]() {
        if (ui->lineEdit_acceptAddress->text().isEmpty()) {
            warn("enter accept email address");
            ui->tabWidget->setCurrentIndex(2);
            return;
        }

        if (!ui->lineEdit_acceptAddress->hasAcceptableInput()) {
            warn("finish editing");
            ui->tabWidget->setCurrentIndex(2);
            return;
        }

        stopped = 0;
        ui->pushButton_start->setVisible(0);
        checkMailbox();
    });

    connect(ui->pushButton_stop, &QPushButton::clicked, this, [this]() {
        if (!confirm("stop")) return;
        
        stopped = 1;
    });

    connect(ui->pushButton_logout, &QPushButton::clicked, this, [this]() {
        if (!confirm("logout")) return;
        
        QFile::remove(TOKENS_PATH);
        login();
    });

    connect(timer, &QTimer::timeout, this, [this]() {
        if (stopped) {
            timer->stop();
            ui->textBrowser_status->append("Stopped!");
            ui->pushButton_start->setVisible(1);
            return;
        }

        if (!remaining) {
            checkMailbox();
            return;
        }

        QStringList lines = ui->textBrowser_status->toHtml().split('\n');
        lines.back().replace(QRegularExpression(R"(\s\d+\s)"), " " + QString::number(--remaining) + " ");
        ui->textBrowser_status->setText(lines.join('\n'));

        QCoreApplication::processEvents(); // Force GUI update
    });

    connect(ui->lineEdit_acceptAddress, &QLineEdit::textChanged, this, [this]() {
        if (ui->lineEdit_acceptAddress->hasAcceptableInput()) {
            ui->lineEdit_acceptAddress->setStyleSheet("");
        }
        else {
            ui->lineEdit_acceptAddress->setStyleSheet("QLineEdit { border: 2px solid red; }");
        }
    });
}

void MainWindow::checkMailbox() {
    try {
        // Clear the previous outputs
        ui->textBrowser_status->clear();

        // Define accept email address and mail filter
        string acceptAddress = ui->lineEdit_acceptAddress->text().toStdString();
        string q = "from:" + acceptAddress + "%20is:unread";

        // Get mail list from inbox
        addState("Checking mailbox");
        string response = getMailList(q);
        if (response.find("error") != string::npos) {
            throw runtime_error("Failed to get mail list");
        }
        updateState();
        json mailList = json::parse(response);

        if (mailList["resultSizeEstimate"] != 0) {
            // Get mail data
            addState("Getting mail data");
            response = getMail(mailList["messages"].back()["id"]);
            if (response.find("error") != string::npos) {
                throw runtime_error("Failed to get mail data");
            }
            updateState();
            json mailData = json::parse(response);

            // Get and print out the mail subject and a snippet of the body
            string subject = getMailSubject(mailData);
            string body = mailData["snippet"];
            if (subject == "") subject = "(empty)";
            if (body == "") body = "(empty)";
            ostringstream oss;
            oss << "Mail content:\nSubject: " << subject << "\nBody: " << body << endl;
            ui->textBrowser_status->append(oss.str().c_str());

            // Trash the handled mail
            addState("Trashing mail");
            response = trashMail(mailList["messages"].back()["id"]);
            if (response.find("error") != string::npos) {
                throw runtime_error("Failed to trash mail");
            }
            updateState();

            // Connect to server for sending message
            addState("Handling the request");
            string receivedMessage, receivedFilePath;
            response = handleMessage(ui->spinBox_port->value(), subject, body, receivedMessage, receivedFilePath);
            if (response.find("Failed") != string::npos) {
                throw runtime_error("Failed to handle request");
            }
            updateState();

            // Send back to the sender the server message
            addState("Sending the result back to the mail sender");
            response = sendMail(acceptAddress, getUserEmailAddress(), "Server response", receivedMessage, receivedFilePath);
            if (response.find("error") != string::npos) {
                throw runtime_error("Failed to send mail");
            }
            updateState();

            // Return if the mail subject starts with 'exit'
            if (subject.find("exit") != string::npos) stopped = 1;
        }
        else {
            ui->textBrowser_status->append("No unread mails found!");
        }
        
        // Setup the countdown timer for the next check
        remaining = ui->spinBox_intervalTime->value();
        ui->textBrowser_status->append(QString("The next check will be in %1 second(s)").arg(remaining));

        timer->start(1000);
    }
    catch (const exception& e) {
        // Notify the error
        ostringstream oss;
        oss << "Error: " << e.what();
        notifyError(oss.str());
        ui->textBrowser_status->append("Stopped!");
        ui->pushButton_start->setVisible(1);
    }
}

void MainWindow::addState(const string& msg) {
    QString text = QString("<span style='color:%1; font-weight:bold;'>%2: ...</span>").arg("#ffd72e", QString::fromStdString(msg));
    ui->textBrowser_status->append(text);
    QCoreApplication::processEvents(); // Force GUI update
}

void MainWindow::updateState() {
    QStringList lines = ui->textBrowser_status->toHtml().split('\n');
    lines.back().replace(QRegularExpression(R"(color:[^;])"), "color:#00c754;").replace("...", "done");
    ui->textBrowser_status->setText(lines.join('\n'));
    QCoreApplication::processEvents(); // Force GUI update
}

void MainWindow::notifyError(const string& msg) {
    QString text = QString("<span style='color:#ff0000; font-weight:bold;'>%2</span>").arg(QString::fromStdString(msg));
    ui->textBrowser_status->append(text);
}

void MainWindow::warn(const string& msg) {
    QString qMsg = QString::fromStdString("Please " + msg + " and try again!\n");
    QMessageBox msgBox(QMessageBox::Warning, "Warning", qMsg, QMessageBox::Ok);
    msgBox.exec();
}

void MainWindow::inform(const string& msg) {
    QString qMsg = QString::fromStdString(msg + "!\n");
    QMessageBox msgBox(QMessageBox::Information, "Information", qMsg, QMessageBox::Ok);
    msgBox.exec();
}

bool MainWindow::confirm(const string& msg) {
    QString qMsg = QString::fromStdString("Are you sure you want to " + msg + "?\n");
    QMessageBox msgBox(QMessageBox::Question, "Confirmation", qMsg, QMessageBox::Yes | QMessageBox::No);
    return msgBox.exec() == QMessageBox::Yes;
}