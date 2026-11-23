#include "NativeShowEditor.h"
#include "NativeSpriteManager.h"

#include <QApplication>
#include <QIcon>
#include <QColor>
#include <QPalette>
#include <QPixmap>
#include <QByteArray>
#include <cstring>
#include <vector>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QFile>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QQueue>
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

#ifdef THISIS395_VJ_EMBEDDED_RENDERER
extern int RunProjectMRenderer(int argc, char* argv[]);
#endif

class NativeControlCenter final : public QMainWindow
{
public:
    NativeControlCenter()
    {
        setWindowTitle(QStringLiteral("ThisIs395 VJ Control Center"));
        resize(1160, 700);
        setWindowIcon(QIcon(QStringLiteral(":/thisis395/portrait.png")));
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

#ifdef THISIS395_VJ_EMBEDDED_RENDERER
        const QString defaultExecutable = QCoreApplication::applicationFilePath();
#elif defined(Q_OS_WIN)
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
#ifdef THISIS395_VJ_EMBEDDED_RENDERER
        // This executable contains the SDL renderer. Never ask for a second app.
        form->addRow(tr("Application"), executable_);
#else
        form->addRow(tr("Executable"), browseRow(executable_, false));
#endif
        form->addRow(tr("Preset folder"), browseRow(presetPath_, true));
        form->addRow(tr("Texture folder"), browseRow(texturePath_, true));
        form->addRow(tr("Width"), width_);
        form->addRow(tr("Height"), height_);
        form->addRow(fullscreen_);
        visualLayout->addWidget(launchSettings);

        auto* controls = new QHBoxLayout;
        start_ = new QPushButton(tr("Pop out visualizer"));
        stop_ = new QPushButton(tr("Close visualizer"));
        stop_->setEnabled(false);
        controls->addWidget(start_);
        controls->addWidget(stop_);
        auto* focus = new QPushButton(tr("Show / focus visualizer"));
        controls->addWidget(focus);
        auto* hide = new QPushButton(tr("Hide visualizer"));
        controls->addWidget(hide);
        connect(focus, &QPushButton::clicked, this, [this] {
            if (process_.state() == QProcess::NotRunning)
                launchVisualizer();
            else
                sendCommand(QStringLiteral("window.focus"));
        });
        connect(hide, &QPushButton::clicked, this, [this] {
            if (process_.state() == QProcess::Running)
                sendCommand(QStringLiteral("window.hide"));
        });
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

        auto* showEditor = new NativeShowEditor(tabs);
        showEditor->setCueCallback([this](const QString& path) { sendCommand(QStringLiteral("cue"), path); });
        showEditor->setSpriteCallback([this](const QString& path) {
            if (spriteDock_) spriteDock_->show();
            sendCommand(QStringLiteral("sprite.add"), QString(), {{QStringLiteral("path"), path}});
        });
        tabs->addTab(showEditor, tr("Show Editor"));

        // Dock in the native control center or float into its own window.
        spriteDock_ = new QDockWidget(tr("Sprite Manager"), this);
        spriteManager_ = new NativeSpriteManager(spriteDock_);
        spriteManager_->setDispatch([this](const QString& action, const QJsonObject& params) {
            sendCommand(action, QString(), params);
        });
        spriteDock_->setWidget(spriteManager_);
        spriteDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        spriteDock_->setFeatures(QDockWidget::DockWidgetMovable |
                                 QDockWidget::DockWidgetFloatable |
                                 QDockWidget::DockWidgetClosable);
        addDockWidget(Qt::RightDockWidgetArea, spriteDock_);
        spriteDock_->setMinimumWidth(340);
        auto* openSprites = new QPushButton(tr("Pop out Sprite Manager"));
        controls->addWidget(openSprites);
        connect(openSprites, &QPushButton::clicked, this, [this] { popOutSpriteManager(); });

        optionsWindow_ = new QWidget(nullptr, Qt::Window);
        optionsWindow_->setWindowTitle(tr("ThisIs395 VJ - Options"));
        auto* optionsLayout = new QVBoxLayout(optionsWindow_);
        optionsLayout->addWidget(new QLabel(tr("Visualizer launch options are shown in the Visualizer tab.\n"
                                              "Changes are saved locally when launching or closing the control center.")));
        auto* optionsSprites = new QPushButton(tr("Open native Sprite Manager"));
        optionsLayout->addWidget(optionsSprites);
        connect(optionsSprites, &QPushButton::clicked, this, [this] { popOutSpriteManager(); });
        optionsWindow_->resize(430, 200);
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
                pendingAction_.clear();
                queuedCommands_.clear();
                spriteManager_->setConnected(false);
                setRunning(false);
            }
        });
        connect(&process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int exitCode, QProcess::ExitStatus) {
                    status_->setText(tr("Visualizer exited with code %1").arg(exitCode));
                    mailboxPoll_.stop();
                    pendingId_.clear();
                    pendingAction_.clear();
                    queuedCommands_.clear();
                    spriteManager_->setConnected(false);
                    spriteManager_->setState(QJsonArray());
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
#ifdef THISIS395_VJ_EMBEDDED_RENDERER
        const QString executable = QCoreApplication::applicationFilePath();
#else
        const QString executable = executable_->text().trimmed();
#endif
        if (!QFileInfo(executable).isFile()) {
            QMessageBox::warning(this, tr("Executable not found"),
                tr("Select the compiled projectMSDL (or projectM on macOS) executable first."));
            return;
        }
        saveSettings();
        QStringList args;
#ifdef THISIS395_VJ_EMBEDDED_RENDERER
        // Same executable: renderer mode creates the separate SDL/OpenGL window.
        args << QStringLiteral("--vj-renderer");
#endif
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
        spriteManager_->setConnected(false);
        spriteManager_->setState(QJsonArray());
        queuedCommands_.clear();
        process_.start();
        mailboxPoll_.start();
        sendHandshakeOnStart_ = true;
    }


    void popOutSpriteManager()
    {
        if (!spriteDock_) return;
        spriteDock_->show();
        spriteDock_->setFloating(true);
        spriteDock_->resize(500, 750);
        spriteDock_->raise();
        spriteDock_->activateWindow();
        if (spriteManager_ && process_.state() == QProcess::Running)
            sendCommand(QStringLiteral("sprite.list"));
    }

    struct Command {
        QString action;
        QString path;
        QJsonObject params;
    };

    void sendCommand(const QString& action, const QString& path = QString(),
                     const QJsonObject& params = QJsonObject())
    {
        if (process_.state() != QProcess::Running) {
            status_->setText(tr("Launch the visualizer before sending commands."));
            if (action.startsWith(QStringLiteral("sprite.")))
                spriteManager_->setStatus(tr("Launch the visualizer first."));
            return;
        }

        // Only one command may occupy the mailbox. Queue controls rather than
        // drop changes while a prior slider adjustment awaits acknowledgment.
        if (!pendingId_.isEmpty()) {
            if (action == QStringLiteral("sprite.update") && !queuedCommands_.isEmpty() &&
                queuedCommands_.last().action == action &&
                queuedCommands_.last().params.value(QStringLiteral("index")) ==
                    params.value(QStringLiteral("index"))) {
                queuedCommands_.last() = {action, path, params};
                return;
            }
            if (queuedCommands_.size() >= 32) {
                status_->setText(tr("Renderer command queue full. Try again after it catches up."));
                return;
            }
            queuedCommands_.enqueue({action, path, params});
            return;
        }

        pendingId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
        pendingAction_ = action;
        pendingElapsed_.start();
        QJsonObject request{{QStringLiteral("version"), 1},
                            {QStringLiteral("id"), pendingId_},
                            {QStringLiteral("action"), action}};
        if (action == QStringLiteral("cue"))
            request.insert(QStringLiteral("path"), path);
        for (auto it = params.constBegin(); it != params.constEnd(); ++it)
            request.insert(it.key(), it.value());

        QSaveFile file(mailboxDir_.filePath(QStringLiteral("command.json")));
        if (!file.open(QIODevice::WriteOnly) ||
            file.write(QJsonDocument(request).toJson(QJsonDocument::Compact)) < 0 ||
            !file.commit()) {
            pendingId_.clear();
            pendingAction_.clear();
            status_->setText(tr("Unable to write renderer command."));
            if (action.startsWith(QStringLiteral("sprite.")))
                spriteManager_->setStatus(tr("Unable to write live sprite command."));
            return;
        }
        status_->setText(tr("Sent %1; awaiting renderer.").arg(action));
    }

    void sendNextQueuedCommand()
    {
        if (pendingId_.isEmpty() && !queuedCommands_.isEmpty()) {
            const Command next = queuedCommands_.dequeue();
            sendCommand(next.action, next.path, next.params);
        }
    }

    void readResponse()
    {
        if (sendHandshakeOnStart_ && process_.state() == QProcess::Running &&
            pendingId_.isEmpty()) {
            sendHandshakeOnStart_ = false;
            sendCommand(QStringLiteral("ping"));
        }

        const QString path = mailboxDir_.filePath(QStringLiteral("response.json"));
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            if (!pendingId_.isEmpty() && pendingElapsed_.isValid() &&
                pendingElapsed_.elapsed() > 5000) {
                const bool wasSpriteCommand = pendingAction_.startsWith(QStringLiteral("sprite."));
                pendingId_.clear();
                pendingAction_.clear();
                queuedCommands_.clear();
                QFile::remove(mailboxDir_.filePath(QStringLiteral("command.json")));
                status_->setText(tr("Renderer did not respond within 5 seconds."));
                if (wasSpriteCommand)
                    spriteManager_->setStatus(tr("Live renderer timed out; refresh to resync."));
            }
            return;
        }

        const auto response = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        QFile::remove(path);
        if (response.value(QStringLiteral("id")).toString() != pendingId_ ||
            pendingId_.isEmpty())
            return;

        const QString action = pendingAction_;
        pendingId_.clear();
        pendingAction_.clear();
        const bool ok = response.value(QStringLiteral("ok")).toBool();
        if (ok && action == QStringLiteral("ping"))
            spriteManager_->setConnected(true);
        if (ok && response.value(QStringLiteral("sprites")).isArray()) {
            // Defer a state refresh until all pending slider edits have landed.
            if (queuedCommands_.isEmpty() || action != QStringLiteral("sprite.update"))
                spriteManager_->setState(response.value(QStringLiteral("sprites")).toArray());
        }
        if (!ok) {
            const QString error = response.value(QStringLiteral("error")).toString();
            status_->setText(tr("Renderer rejected command: %1").arg(error));
            if (action.startsWith(QStringLiteral("sprite.")))
                spriteManager_->setStatus(error);
        } else {
            status_->setText(tr("Renderer connected. Preset #%1, shuffle: %2, locked: %3")
                .arg(response.value(QStringLiteral("presetIndex")).toInt())
                .arg(response.value(QStringLiteral("shuffle")).toBool() ? tr("on") : tr("off"))
                .arg(response.value(QStringLiteral("locked")).toBool() ? tr("yes") : tr("no")));
            if (action == QStringLiteral("sprite.update"))
                spriteManager_->setStatus(tr("Live sprite settings applied."));
            else if (action == QStringLiteral("sprite.add"))
                spriteManager_->setStatus(tr("PNG sprite added to visualizer."));
            else if (action == QStringLiteral("sprite.remove"))
                spriteManager_->setStatus(tr("Live sprite removed."));
        }
        sendNextQueuedCommand();
    }

    void restoreSettings()
    {
        QSettings s;
#ifdef THISIS395_VJ_EMBEDDED_RENDERER
        // Never restore a stale path to the now-removed second renderer binary.
        executable_->setText(QCoreApplication::applicationFilePath());
        executable_->setReadOnly(true);
#else
        executable_->setText(s.value(QStringLiteral("executable"), executable_->text()).toString());
#endif
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
    QString pendingAction_;
    QQueue<Command> queuedCommands_;
    QElapsedTimer pendingElapsed_;
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
    QDockWidget* spriteDock_ = nullptr;
    NativeSpriteManager* spriteManager_ = nullptr;
};

int main(int argc, char** argv)
{
#ifdef THISIS395_VJ_EMBEDDED_RENDERER
    if (argc > 1 && std::strcmp(argv[1], "--vj-renderer") == 0) {
        std::vector<char*> rendererArgs;
        rendererArgs.reserve(static_cast<std::size_t>(argc));
        rendererArgs.push_back(argv[0]);
        for (int i = 2; i < argc; ++i)
            rendererArgs.push_back(argv[i]);
        const int rendererArgc = static_cast<int>(rendererArgs.size());
        rendererArgs.push_back(nullptr); // Preserve argv[argc] sentinel.
        return RunProjectMRenderer(rendererArgc, rendererArgs.data());
    }
#endif
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/thisis395/portrait.png")));

    // Keep the supplied portrait monochrome, but use the user's green theme.
    QPalette palette = app.palette();
    palette.setColor(QPalette::Window, QColor("#101510"));
    palette.setColor(QPalette::WindowText, QColor("#EAF2E9"));
    palette.setColor(QPalette::Base, QColor("#0B100C"));
    palette.setColor(QPalette::AlternateBase, QColor("#162016"));
    palette.setColor(QPalette::Text, QColor("#EAF2E9"));
    palette.setColor(QPalette::Button, QColor("#1C281E"));
    palette.setColor(QPalette::ButtonText, QColor("#F0F4F0"));
    palette.setColor(QPalette::Highlight, QColor("#3CDD28"));
    palette.setColor(QPalette::HighlightedText, QColor("#071506"));
    palette.setColor(QPalette::Link, QColor("#51EF43"));
    palette.setColor(QPalette::PlaceholderText, QColor("#93A398"));
    app.setPalette(palette);
    app.setStyleSheet(QStringLiteral(
        "QTabBar::tab:selected { border-bottom: 2px solid #3CDD28; }"
        "QPushButton:hover { border-color: #3CDD28; }"
        "QDockWidget::title { background: #162016; }"
        "QScrollBar::handle { background: #314C32; }"));

    QCoreApplication::setOrganizationName(QStringLiteral("ThisIs395"));
    QCoreApplication::setApplicationName(QStringLiteral("VJ Control Center"));
    NativeControlCenter window;
    window.show();
    return app.exec();
}
