#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    // Initiate socket
    string res = WSAInitiate();
    if (res.size()) {
        notifyError(res);
        return;
    }
    
    // Setup UI
    ui->setupUi(this);
    
    // Initiate properties
    stopped = remaining = waiting = 0;
    timer = new QTimer(this);
    waitingTimer = new QTimer(this);
    webView = new QWebEngineView(this);
    stackedWidget = new QStackedWidget(this);
    worker = new Worker();
    workerThread = new QThread(this);
    
    // Setup views
    stackedWidget->addWidget(ui->centralwidget);
    stackedWidget->addWidget(webView);
    setCentralWidget(stackedWidget);

    // Move worker to the thread
    worker->moveToThread(workerThread);

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
    // Clean up socket
    WSACleanup();

    // Delocate pointers
    delete ui;
    delete worker;

    // Wait for the thread to quit
    workerThread->quit();
    workerThread->wait();
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
    connect(workerThread, &QThread::started, worker, &Worker::startTask);

    connect(ui->pushButton_start, &QPushButton::clicked, this, &MainWindow::startSession);

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
            timer->stop();
            startSession();
            return;
        }

        QStringList lines = ui->textBrowser_status->toHtml().split('\n');
        lines.back().replace(QRegularExpression(R"(\s\d+\s)"), " " + QString::number(--remaining) + " ");
        ui->textBrowser_status->setText(lines.join('\n'));
    });

    connect(waitingTimer, &QTimer::timeout, this, [this]() {
        if (stopped) {
            if (workerThread->isRunning()) {
                // Disconnect the callback slot from the `taskFinished` signal
                disconnect(worker, &Worker::taskFinished, nullptr, nullptr);
                
                workerThread->quit();
                workerThread->wait();
            }
            
            waitingTimer->stop();
            ui->textBrowser_status->append("Stopped!");
            ui->pushButton_start->setVisible(1);
            return;
        }

        QString oldStr(waiting, '.');
        waiting = (waiting % 3) + 1;
        QString newStr(waiting, '.');

        QStringList lines = ui->textBrowser_status->toHtml().split('\n');
        lines.back().replace(oldStr, newStr);
        ui->textBrowser_status->setText(lines.join('\n'));
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

void MainWindow::startSession() {
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

    // Clear the previous outputs
    ui->textBrowser_status->clear();

    // Get mail list from inbox
    addState("Checking mailbox");
    startBackgroundTask(
        [this] {
            return getMailList("from:" + ui->lineEdit_acceptAddress->text().toStdString() + "%20is:unread");
        },
        [this](const string& res) {
            if (res.find("error") != string::npos) {
                notifyError("Failed to get mail list");
                return;
            }
            json mailList = json::parse(res);
            updateState();

            if (mailList["resultSizeEstimate"] != 0) {
                string mailID = mailList["messages"].back()["id"];
                // Get mail data
                addState("Getting mail data");
                startBackgroundTask(
                    [mailID] {
                        return getMail(mailID);
                    },
                    [this, mailID](const string& res) {
                        if (res.find("error") != string::npos) {
                            notifyError("Failed to get mail data");
                            return;
                        }
                        json mailData = json::parse(res);
                        updateState();

                        // Get and print out the mail subject and a snippet of the body
                        string subject = getMailSubject(mailData);
                        string body = mailData["snippet"];
                        if (subject == "") subject = "(empty)";
                        if (body == "") body = "(empty)";
                        ui->textBrowser_status->append(QString("Mail content:\nSubject: %1\nBody: %2").arg(subject.c_str(), body.c_str()));

                        // Trash the handled mail
                        addState("Trashing mail");
                        startBackgroundTask(
                            [mailID]() {
                                return trashMail(mailID);
                            },
                            [this, subject, body](const string& res) {
                                if (res.find("error") != string::npos) {
                                    notifyError("Failed to trash mail");
                                    return;
                                }
                                updateState();

                                // Connect to server for sending message
                                addState("Handling the request");
                                startBackgroundTask(
                                    [this, subject, body]() {
                                        return handleMessage(ui->spinBox_port->value(), subject, body);
                                    },
                                    [this, subject](const string& res) {
                                        if (res.find("Failed") != string::npos) {
                                            notifyError("Failed to handle request");
                                            return;
                                        }
                                        updateState();

                                        size_t pos = res.find("\nreceivedFilePath=");
                                        string receivedMessage = res.substr(0, pos), receivedFilePath = res.substr(pos + 18);

                                        // Send back to the sender the server message
                                        addState("Sending the result back to the mail sender");
                                        startBackgroundTask(
                                            [this, receivedMessage, receivedFilePath]() {
                                                return sendMail(ui->lineEdit_acceptAddress->text().toStdString(), getUserEmailAddress(), "Server response", receivedMessage, receivedFilePath);
                                            },
                                            [this, subject](const string& res) {
                                                if (res.find("error") != string::npos) {
                                                    notifyError("Failed to send mail");
                                                    return;
                                                }
                                                updateState();

                                                // Return if the mail subject starts with 'exit'
                                                if (subject.find("exit") != string::npos) {
                                                    stopped = 1;
                                                    ui->textBrowser_status->append("Stopped!");
                                                    ui->pushButton_start->setVisible(1);
                                                }
                                                else {
                                                    // Setup the countdown timer for the next check
                                                    remaining = ui->spinBox_intervalTime->value();
                                                    ui->textBrowser_status->append(QString("The next check will be in %1 second(s)").arg(remaining));
                                                    timer->start(1000);
                                                }
                                            }
                                        );
                                    }
                                );
                            }
                        );
                    }
                );
            }
            else {
                ui->textBrowser_status->append("No unread mails found!");

                // Setup the countdown timer for the next check
                remaining = ui->spinBox_intervalTime->value();
                ui->textBrowser_status->append(QString("The next check will be in %1 second(s)").arg(remaining));
                timer->start(1000);
            }
        }
    );
}

void MainWindow::addState(const string& msg) {
    QString text = QString("<span style='color:%1; font-weight:bold;'>%2: ...</span>").arg("#ffd72e", QString::fromStdString(msg));
    ui->textBrowser_status->append(text);
    
    waiting = 3;
    waitingTimer->start(300);
}

void MainWindow::updateState() {
    waitingTimer->stop();
    
    QStringList lines = ui->textBrowser_status->toHtml().split('\n');
    lines.back().replace(QRegularExpression(R"(color:[^;])"), "color:#00c754;").replace(QString(waiting, '.'), "done");
    ui->textBrowser_status->setText(lines.join('\n'));
}

void MainWindow::notifyError(const string& msg) {
    QString text = QString("<span style='color:#ff0000; font-weight:bold;'>Error: %2</span>").arg(QString::fromStdString(msg));
    ui->textBrowser_status->append(text);
    
    stopped = 1;
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

void MainWindow::startBackgroundTask(function<string()> fetchFunction, function<void(const string&)> callbackFunction) {
    // Set the task for the worker
    worker->setTask(fetchFunction);

    // Disconnect any previous connections to avoid multiple calls
    disconnect(worker, &Worker::taskFinished, nullptr, nullptr);

    // Connect worker's taskFinished signal to a lambda that calls the callback function
    connect(worker, &Worker::taskFinished, this, [=](const QString& result) {
        callbackFunction(result.toStdString());  // Call the callback function with the result
    }, Qt::QueuedConnection);

    // Connect worker's taskFinished to quit
    connect(worker, &Worker::taskFinished, workerThread, &QThread::quit);

    // Start the thread
    if (workerThread->isRunning()) {
        workerThread->quit();
        workerThread->wait();
    }
    workerThread->start();
}