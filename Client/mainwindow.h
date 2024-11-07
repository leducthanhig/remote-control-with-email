#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "worker.h"
#include "client.h"
#include "gmailapi.h"
#include "ui_mainwindow.h"
#include <QtCore/QFile>
#include <QtCore/QTimer>
#include <QtCore/QThread>
#include <QtCore/QString>
#include <QtCore/QUrlQuery>
#include <QtCore/QStringList>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QStackedWidget>
#include <QtWebEngineWidgets/QWebEngineView>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QWebEngineView* webView;
    QStackedWidget* stackedWidget;
    QTimer *timer, *waitingTimer;
    QThread* workerThread;
    Worker* worker;
    int remaining, waiting;
    bool stopped;

    void startBackgroundTask(function<string()> fetchFunction, function<void(const string&)> callbackFunction);
    void login();
    void setConnections();
    void startSession();
    void addState(const string& msg);
    void updateState();
    void notifyError(const string& msg);
    void warn(const string& msg);
    void inform(const string& msg);
    bool confirm(const string& msg);
};

#endif // MAINWINDOW_H