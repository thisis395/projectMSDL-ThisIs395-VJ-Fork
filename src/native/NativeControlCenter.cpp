#include "NativeShowEditor.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <QMainWindow>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

class NativeControlCenter final : public QMainWindow
{
public:
    NativeControlCenter()
    {
        setWindowTitle(QStringLiteral("ThisIs395 VJ Control Center"));
        resize(860, 580);
        auto* tabs = new QTabWidget(this);
        setCentralWidget(tabs);

        auto* visualizer = new QWidget;
        auto* visualLayout = new QVBoxLayout(visualizer);
        auto* launchSettings = new QGroupBox(tr("Visualizer launch"));
        auto* form = new QFormLayout(launchSettings);
        executable_ = new QLineEdit;
        presetPath_ = new QLineEdit;
        texturePath_ = new QLineEdit;
        width_ = new QSpinBox;
        height_ = new QSpinBox;
        width_->setRange(320, 16384);
        height_->setRange(240, 16384);
        fullscreen_ = new QCheckBox(tr("Start fullscreen"));

#ifdef Q_OS_WIN
        const QString defaultExecutable = QStringLiteral("projectMSDL.exe");
#elif defined(Q_OS_MACOS)
        const QString defaultExecutable = QStringLiteral("projectM");
#else
        const QString defaultExecutable = QStringLiteral("projectMSDL");
#endif
        executable_->setText(QDir(QCoreApplication::applicationDirPath()).filePath(defaultExecutable));
        auto browseRow = [this](QLineEdit* edit, bool directory) {
            auto* row = new QWidget;
            auto* layout = new QHBoxLayout(row);
            layout->setContentsMargins(0, 0, 0, 0);
            auto* browse = new QPushButton(tr("Browse..."));
            layout->addWidget(edit);
            layout->addWidget(browse);
            connect(browse, &QPushButton::clicked, this, [this, edit, directory] {
                const QString value = directory
                    ? QFileDialog::getExistingDirectory(this, tr("Choose folder"), edit->text())
                    : QFileDialog::getOpenFileName(this, tr("Choose visualizer executable"), edit->text());
                if (!value.isEmpty())
                    edit->setText(value);
            });
            return row;
        };
        form->addRow(tr("Executable"), browseRow(executable_, false));
        form->addRow(tr("Preset folder"), browseRow(presetPath_, true));
        form->addRow(tr("Texture folder"), browseRow(texturePath_, true));
        form->addRow(tr("Width"), width_);
        form->addRow(tr("Height"), height_);
        form->addRow(fullscreen_);
        visualLayout->addWidget(launchSettings);

        auto* controls = new QHBoxLayout;
        start_ = new QPushButton(tr("Launch visualizer"));
        stop_ = new QPushButton(tr("Stop visualizer"));
        stop_->setEnabled(false);
        controls->addWidget(start_);
        controls->addWidget(stop_);
        controls->addStretch();
        visualLayout->addLayout(controls);
        auto* liveControls = new QHBoxLayout;
        auto* previousPreset = new QPushButton(tr("Previous preset"));
        auto* nextPreset = new QPushButton(tr("Next preset"));
        auto* shuffle = new QPushButton(tr("Toggle shuffle"));
        auto* lock = new QPushButton(tr("Lock / unlock preset"));
        liveControls->addWidget(previousPreset);
        liveControls->addWidget(nextPreset);
        liveControls->addWidget(shuffle);
        liveControls->addWidget(lock);
        visualLayout->addLayout(liveControls);
        connect(previousPreset, &QPushButton::clicked, this, [this] { sendCommand(QStringLiteral("previous")); });
        connect(nextPreset, &QPushButton::clicked, this, [this] { sendCommand(QStringLiteral("next")); });
        connect(shuffle, &QPushButton::clicked, this, [this] { sendCommand(QStringLiteral("shuffle")); });
        connect(lock, &QPushButton::clicked, this, [this] { sendCommand(QStringLiteral("lock")); });

        status_ = new QLabel(tr("Visualizer not running"));
        visualLayout->addWidget(status_);
        output_ = new QPlainTextEdit;
        output_->setReadOnly(true);
        output_->setPlaceholderText(tr("Visualizer output appears here"));
        visualLayout->addWidget(output_, 1);
        tabs->addTab(visualizer, tr("Visualizer"));

        tabs->addTab(new NativeShowEditor(tabs), tr("Show Editor"));

        optionsWindow_ = new QWidget(nullptr, Qt::Window);
        optionsWindow_->setWindowTitle(tr("ThisIs395 VJ - Options"));
        auto* optionsLayout = new QVBoxLayout(optionsWindow_);
        optionsLayout->addWidget(new QLabel(tr("Visualizer launch options are shown in the Visualizer tab.\n"
                                              "Changes are saved locally when launching or closing the control center.")));
        optionsWindow_->resize(430, 160);
        auto* optionsButton = new QPushButton(tr("Pop out options"));
        controls->addWidget(optionsButton);
        connect(optionsButton, &QPushButton::clicked, optionsWindow_, [this] {
            optionsWindow_->show();
            optionsWindow_->raise();
            optionsWindow_->activateWindow();
        });

        connect(start_, &QPushButton::clicked, this, [this] { launchVisualizer(); });
        connect(stop_, &QPushButton::clicked, this, [this] {
            if (process_.state() != QProcess::NotRunning)
                process_.terminate();
        });
        connect(&process_, &QProcess::readyReadStandardOutput, this, [this] {
            output_->appendPlainText(QString::fromLocal8Bit(process_.readAllStandardOutput()));
        });
        connect(&process_, &QProcess::readyReadStandardError, this, [this] {
            output_->appendPlainText(QString::fromLocal8Bit(process_.readAllStandardError()));
        });
        connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            status_->setText(tr("Visualizer error: %1").arg(process_.errorString()));
            if (error == QProcess::FailedToStart) {
                mailboxPoll_.stop();
                pendingId_.clear();
                setRunning(false);
            }
        });
        connect(&process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int exitCode, QProcess::ExitStatus) {
                    status_->setText(tr("Visualizer exited with code %1").arg(exitCode));
                    mailboxPoll_.stop();
                    pendingId_.clear();
                    setRunning(false);
                });
        connect(&mailboxPoll_, &QTimer::timeout, this, [this] { readResponse(); });
        mailboxPoll_.setInterval(100);
        restoreSettings();
    }

    ~NativeControlCenter() override
    {
        if (process_.state() != QProcess::NotRunning) {
            process_.terminate();
            if (!process_.waitForFinished(2000))
                process_.kill();
            process_.waitForFinished(1000);
        }
        delete optionsWindow_;
    }

protected:
    void closeEvent(QCloseEvent* event) override
    {
        saveSettings();
        QMainWindow::closeEvent(event);
    }

private:
    void setRunning(bool running)
    {
        start_->setEnabled(!running);
        stop_->setEnabled(running);
    }

    void launchVisualizer()
    {
        if (process_.state() != QProcess::NotRunning)
            return;
        const QString executable = executable_->text().trimmed();
        if (!QFileInfo(executable).isFile()) {
            QMessageBox::warning(this, tr("Executable not found"),
                tr("Select the compiled projectMSDL (or projectM on macOS) executable first."));
            return;
        }
        saveSettings();
        QStringList args;
        if (!presetPath_->text().trimmed().isEmpty())
            args << QStringLiteral("--presetPath") << presetPath_->text().trimmed();
        if (!texturePath_->text().trimmed().isEmpty())
            args << QStringLiteral("--texturePath") << texturePath_->text().trimmed();
        if (!mailboxDir_.isValid()) {
            QMessageBox::warning(this, tr("Control mailbox unavailable"), tr("Unable to create private local control directory."));
            setRunning(false);
            return;
        }
        // The directory is unique to this control-center process, avoiding collisions.
        args << QStringLiteral("--controlDir") << mailboxDir_.path();
        args << QStringLiteral("--width") << QString::number(width_->value());
        args << QStringLiteral("--height") << QString::number(height_->value());
        args << QStringLiteral("--fullscreen") << (fullscreen_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
        output_->clear();
        process_.setProgram(executable);
        process_.setArguments(args);
        process_.setWorkingDirectory(QFileInfo(executable).absolutePath());
        status_->setText(tr("Starting visualizer..."));
        setRunning(true);
        process_.start();
        mailboxPoll_.start();
        sendHandshakeOnStart_ = true;
    }


    void sendCommand(const QString& action)
    {
        if (process_.state() != QProcess::Running) {
            status_->setText(tr("Launch the visualizer before sending commands."));
            return;
        }
        if (!pendingId_.isEmpty()) {
            status_->setText(tr("Waiting for the previous renderer response."));
            return;
        }
        pendingId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QJsonObject request{{QStringLiteral("version"), 1},
                            {QStringLiteral("id"), pendingId_},
                            {QStringLiteral("action"), action}};
        QSaveFile file(mailboxDir_.filePath(QStringLiteral("command.json")));
        if (!file.open(QIODevice::WriteOnly) ||
            file.write(QJsonDocument(request).toJson(QJsonDocument::Compact)) < 0 ||
            !file.commit()) {
            pendingId_.clear();
            status_->setText(tr("Unable to write renderer command."));
            return;
        }
        status_->setText(tr("Sent %1; awaiting renderer.").arg(action));
    }

    void readResponse()
    {
        if (sendHandshakeOnStart_ && process_.state() == QProcess::Running && pendingId_.isEmpty()) {
            sendHandshakeOnStart_ = false;
            sendCommand(QStringLiteral("ping"));
        }
        const QString path = mailboxDir_.filePath(QStringLiteral("response.json"));
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return;
        const auto response = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        QFile::remove(path);
        if (response.value(QStringLiteral("id")).toString() != pendingId_ || pendingId_.isEmpty())
            return;
        pendingId_.clear();
        if (!response.value(QStringLiteral("ok")).toBool()) {
            status_->setText(tr("Renderer rejected command: %1")
                                 .arg(response.value(QStringLiteral("error")).toString()));
            return;
        }
        status_->setText(tr("Renderer connected. Preset #%1, shuffle: %2, locked: %3")
                             .arg(response.value(QStringLiteral("presetIndex")).toInt())
                             .arg(response.value(QStringLiteral("shuffle")).toBool() ? tr("on") : tr("off"))
                             .arg(response.value(QStringLiteral("locked")).toBool() ? tr("yes") : tr("no")));
    }

    void restoreSettings()
    {
        QSettings s;
        executable_->setText(s.value(QStringLiteral("executable"), executable_->text()).toString());
        presetPath_->setText(s.value(QStringLiteral("presetPath")).toString());
        texturePath_->setText(s.value(QStringLiteral("texturePath")).toString());
        width_->setValue(s.value(QStringLiteral("width"), 1280).toInt());
        height_->setValue(s.value(QStringLiteral("height"), 720).toInt());
        fullscreen_->setChecked(s.value(QStringLiteral("fullscreen"), false).toBool());
        restoreGeometry(s.value(QStringLiteral("geometry")).toByteArray());
    }

    void saveSettings()
    {
        QSettings s;
        s.setValue(QStringLiteral("executable"), executable_->text());
        s.setValue(QStringLiteral("presetPath"), presetPath_->text());
        s.setValue(QStringLiteral("texturePath"), texturePath_->text());
        s.setValue(QStringLiteral("width"), width_->value());
        s.setValue(QStringLiteral("height"), height_->value());
        s.setValue(QStringLiteral("fullscreen"), fullscreen_->isChecked());
        s.setValue(QStringLiteral("geometry"), saveGeometry());
    }

    QProcess process_;
    QTemporaryDir mailboxDir_;
    QTimer mailboxPoll_;
    QString pendingId_;
    bool sendHandshakeOnStart_ = false;
    QLineEdit* executable_ = nullptr;
    QLineEdit* presetPath_ = nullptr;
    QLineEdit* texturePath_ = nullptr;
    QSpinBox* width_ = nullptr;
    QSpinBox* height_ = nullptr;
    QCheckBox* fullscreen_ = nullptr;
    QPushButton* start_ = nullptr;
    QPushButton* stop_ = nullptr;
    QLabel* status_ = nullptr;
    QPlainTextEdit* output_ = nullptr;
    QWidget* optionsWindow_ = nullptr;
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("ThisIs395"));
    QCoreApplication::setApplicationName(QStringLiteral("VJ Control Center"));
    NativeControlCenter window;
    window.show();
    return app.exec();
}
