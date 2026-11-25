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
#include <iostream>
#include <cmath>
#include <QCheckBox>
#include <QComboBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QMetaObject>
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
#include <QTextDocument>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#ifdef Q_OS_WIN
#include <shobjidl.h>
#endif

#ifdef THISIS395_VJ_EMBEDDED_RENDERER
extern int RunProjectMRenderer(int argc, char* argv[], bool verifyControlDir);
#endif

class NativeControlCenter final : public QMainWindow
{
public:
    NativeControlCenter()
    {
        setWindowTitle(QStringLiteral("ThisIs395 VJ Control Center"));
        resize(1160, 700);
        setWindowIcon(QIcon(QStringLiteral(":/thisis395/portrait.png")));
        tabs_ = new QTabWidget(this);
        auto* tabs = tabs_;
        setCentralWidget(tabs);

        // Panic is documentation, not an emergency stop. Keep it available
        // on every tab and never send a command to the renderer.
        auto* panic = new QPushButton(QStringLiteral("huh?"), tabs);
        panic->setObjectName(QStringLiteral("panic-readme"));
        panic->setAccessibleName(tr("Open project README"));
        panic->setToolTip(tr("PANIC: read the README. No visualizers will be harmed."));
        panic->setCursor(Qt::PointingHandCursor);
        panic->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: #b91c1c; color: white; "
            "font-weight: bold; padding: 5px 16px; border-radius: 4px; }"
            "QPushButton:hover { background-color: #dc2626; }"
            "QPushButton:pressed { background-color: #991b1b; }"));
        tabs->setCornerWidget(panic, Qt::TopRightCorner);
        connect(panic, &QPushButton::clicked, this, [this] {
            const QUrl readme(QStringLiteral(
                "https://github.com/thisis395/projectMSDL-ThisIs395-VJ-Fork#readme"));
            if (!QDesktopServices::openUrl(readme))
                QMessageBox::warning(this, tr("huh?"),
                    tr("Could not open the README in your browser.\n%1").arg(readme.toString()));
        });

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
        auto* displayRow = new QWidget;
        auto* displayLayout = new QHBoxLayout(displayRow);
        displayLayout->setContentsMargins(0, 0, 0, 0);
        display_ = new QComboBox;
        display_->setObjectName(QStringLiteral("visualizer-display"));
        display_->addItem(tr("Automatic / OS default"), 0);
        displayLayout->addWidget(display_, 1);
        moveDisplay_ = new QPushButton(tr("Move"));
        moveDisplay_->setObjectName(QStringLiteral("display-move"));
        moveDisplay_->setEnabled(false);
        displayLayout->addWidget(moveDisplay_);
        refreshDisplays_ = new QPushButton(tr("Refresh"));
        refreshDisplays_->setEnabled(false);
        displayLayout->addWidget(refreshDisplays_);
        connect(display_, &QComboBox::currentIndexChanged, this, [this](int) {
            moveDisplay_->setEnabled(rendererReady_ && display_->currentData().toInt() > 0);
        });
        connect(moveDisplay_, &QPushButton::clicked, this, [this] {
            const int display = display_->currentData().toInt();
            if (display > 0)
                sendCommand(QStringLiteral("window.display"), QString(),
                    QJsonObject{{QStringLiteral("display"), display}});
        });
        connect(refreshDisplays_, &QPushButton::clicked, this, [this] {
            sendCommand(QStringLiteral("window.displays"));
        });
        form->addRow(tr("Display"), displayRow);
        form->addRow(fullscreen_);
        visualLayout->addWidget(launchSettings);

        auto* meshGroup = new QGroupBox(tr("Rendering quality"));
        auto* meshForm = new QFormLayout(meshGroup);
        auto* meshRow = new QWidget;
        auto* meshLayout = new QHBoxLayout(meshRow);
        meshLayout->setContentsMargins(0, 0, 0, 0);
        meshX_ = new QSpinBox;
        meshY_ = new QSpinBox;
        meshX_->setObjectName(QStringLiteral("mesh-x"));
        meshY_->setObjectName(QStringLiteral("mesh-y"));
        meshX_->setAccessibleName(tr("Mesh width"));
        meshY_->setAccessibleName(tr("Mesh height"));
        for (auto* box : {meshX_, meshY_}) {
            box->setRange(8, 300);
            box->setToolTip(tr("Per-point transformation grid; higher values cost more CPU time. Not the output resolution."));
        }
        meshX_->setValue(96);
        meshY_->setValue(54);
        meshLayout->addWidget(meshX_);
        meshLayout->addWidget(new QLabel(QStringLiteral("×")));
        meshLayout->addWidget(meshY_);
        auto* resetMesh = new QPushButton(tr("Default"));
        connect(resetMesh, &QPushButton::clicked, this, [this] {
            meshX_->setValue(96);
            meshY_->setValue(54);
        });
        meshLayout->addWidget(resetMesh);
        meshApply_ = new QPushButton(tr("Apply & save"));
        meshApply_->setObjectName(QStringLiteral("mesh-apply"));
        meshApply_->setEnabled(false);
        connect(meshApply_, &QPushButton::clicked, this, [this] {
            sendCommand(QStringLiteral("render.mesh"), QString(),
                QJsonObject{{QStringLiteral("meshX"), meshX_->value()},
                            {QStringLiteral("meshY"), meshY_->value()}});
        });
        meshLayout->addWidget(meshApply_);
        meshForm->addRow(tr("Mesh X / Y"), meshRow);
        auto* meshHelp = new QLabel(tr("96 × 54 recommended. Launch the visualizer, then apply changes live. Lower values can improve FPS."));
        meshHelp->setWordWrap(true);
        meshForm->addRow(meshHelp);
        visualLayout->addWidget(meshGroup);

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

        fpsLabel_ = new QLabel(tr("FPS: —"));
        fpsLabel_->setObjectName(QStringLiteral("renderer-fps"));
        fpsLabel_->setMinimumWidth(110);
        fpsLabel_->setToolTip(tr("Measured renderer frame rate, including frame pacing and buffer swaps."));
        statusBar()->addPermanentWidget(fpsLabel_);
        status_ = new QLabel(tr("Visualizer not running"));
        visualLayout->addWidget(status_);
        output_ = new QPlainTextEdit;
        output_->setReadOnly(true);
        output_->document()->setMaximumBlockCount(600);
        output_->setPlaceholderText(tr("Visualizer output appears here"));
        visualLayout->addWidget(output_, 1);
        tabs->addTab(visualizer, tr("Visualizer"));

        auto* showEditor = new NativeShowEditor(tabs);
        showEditor->setCueCallback([this](const QString& path) { sendCommand(QStringLiteral("cue"), path); });
        showEditor->setSpriteCallback([this](const QString& path) {
            showSpriteManager();
            // Show Editor and Sprite Manager must go through the same import
            // logic, so the PNG appears in the library even if the renderer
            // cannot load it and the error stays visible to the user.
            spriteManager_->importSpriteFile(path);
        });
        tabs->addTab(showEditor, tr("Show Editor"));

        // A single Sprite Manager lives in the Control Center. The SDL window
        // is solely the visualizer. No duplicate floating sprite editors.
        spriteManager_ = new NativeSpriteManager(tabs);
        spriteManager_->setDispatch([this](const QString& action, const QJsonObject& params) {
            sendCommand(action, QString(), params);
        });
        tabs->addTab(spriteManager_, tr("Sprite Manager"));

        auto* openSprites = new QPushButton(tr("Sprite Manager"));
        controls->addWidget(openSprites);
        connect(openSprites, &QPushButton::clicked, this, [this] { showSpriteManager(); });

        optionsWindow_ = new QWidget(nullptr, Qt::Window);
        optionsWindow_->setWindowTitle(tr("ThisIs395 VJ - Options"));
        auto* optionsLayout = new QVBoxLayout(optionsWindow_);
        optionsLayout->addWidget(new QLabel(tr("Visualizer launch options are shown in the Visualizer tab.\n"
                                              "Changes are saved locally when launching or closing the control center.")));
        auto* optionsSprites = new QPushButton(tr("Open native Sprite Manager"));
        optionsLayout->addWidget(optionsSprites);
        connect(optionsSprites, &QPushButton::clicked, this, [this] { showSpriteManager(); });
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
        // Keep renderer stdout/stderr from flooding the Qt event loop and
        // freezing buttons/spinbox editors when the engine logs heavily.
        auto bufferOutput = [this](const QByteArray& chunk) {
            outputBuffer_.append(chunk);
            if (outputBuffer_.size() > 65536)
                outputBuffer_ = outputBuffer_.right(65536);
        };
        connect(&process_, &QProcess::readyReadStandardOutput, this, [this, bufferOutput] {
            bufferOutput(process_.readAllStandardOutput());
        });
        connect(&process_, &QProcess::readyReadStandardError, this, [this, bufferOutput] {
            bufferOutput(process_.readAllStandardError());
        });
        connect(&outputPoll_, &QTimer::timeout, this, [this] {
            if (outputBuffer_.isEmpty()) return;
            QByteArray next;
            next.swap(outputBuffer_);
            output_->appendPlainText(QString::fromLocal8Bit(next));
        });
        outputPoll_.setInterval(250);
        outputPoll_.start();
        connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            status_->setText(tr("Visualizer error: %1").arg(process_.errorString()));
            if (error == QProcess::FailedToStart) {
                mailboxPoll_.stop();
                pendingId_.clear();
                pendingAction_.clear();
                queuedCommands_.clear();
                rendererReady_ = false;
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
                    rendererReady_ = false;
                    spriteManager_->setConnected(false);
                    spriteManager_->setState(QJsonArray());
                    setRunning(false);
                });
        connect(&mailboxPoll_, &QTimer::timeout, this, [this] { readResponse(); });
        mailboxPoll_.setInterval(100);
        connect(&statsPoll_, &QTimer::timeout, this, [this] {
            if (!rendererReady_ || !statsSupported_ || process_.state() != QProcess::Running ||
                !pendingId_.isEmpty() || !queuedCommands_.isEmpty())
                return;
            sendCommand(QStringLiteral("render.stats"));
        });
        statsPoll_.setInterval(1000);
        statsPoll_.start();
        restoreSettings();
    }

    // CI exercises the same real Qt control center the user launches,
    // including the renderer subprocess, JSON mailbox and live widgets.
    // --vj-control-smoke is only for automated release qualification.
    void runEndToEndSpriteSmoke(const QString& pngPath)
    {
        QElapsedTimer started;
        started.start();
        auto* timer = new QTimer(this);
        // Make the actual Sprite Manager tab visible before exercising its
        // real file dialog; do not bypass the user-facing import path.
        showSpriteManager();
        auto* add = spriteManager_->findChild<QPushButton*>(QStringLiteral("sprite-add"));
        if (!add || !add->isEnabled()) {
            std::cerr << "FAIL: Add PNG button missing/disabled" << std::endl;
            QCoreApplication::exit(1);
            return;
        }
        add->click();
        QTimer::singleShot(0, this, [this, pngPath] {
            auto* dialog = spriteManager_->findChild<QFileDialog*>(
                QStringLiteral("sprite-file-picker"));
            if (!dialog) {
                std::cerr << "FAIL: Add PNG did not open file picker" << std::endl;
                QCoreApplication::exit(1);
                return;
            }
            const QFileInfo png(pngPath);
            if (!png.isFile() || !png.isReadable()) {
                std::cerr << "FAIL: smoke PNG missing or unreadable: "
                          << pngPath.toStdString() << std::endl;
                QCoreApplication::exit(1);
                return;
            }
            dialog->setDirectory(png.absolutePath());
            auto* name = dialog->findChild<QLineEdit*>(QStringLiteral("fileNameEdit"));
            auto* buttons = dialog->findChild<QDialogButtonBox*>();
            auto* open = buttons ? buttons->button(QDialogButtonBox::Open) : nullptr;
            if (!name || !open) {
                std::cerr << "FAIL: PNG picker filename field or Open button missing" << std::endl;
                QCoreApplication::exit(1);
                return;
            }
            // Exercise the same text field and Open button as a real user.
            // QFileSystemModel is asynchronous: wait for a valid selection,
            // not a fixed sleep followed by a potentially ignored accept().
            auto* pickerPoll = new QTimer(dialog);
            connect(pickerPoll, &QTimer::timeout, dialog,
                    [dialog, name, open, png, pickerPoll, attempts = 0]() mutable {
                if (++attempts > 50) {
                    pickerPoll->stop();
                    std::cerr << "FAIL: PNG picker never enabled Open for "
                              << png.absoluteFilePath().toStdString() << std::endl;
                    QCoreApplication::exit(1);
                    return;
                }
                if (name->text() != png.absoluteFilePath())
                    name->setText(png.absoluteFilePath());
                const auto selected = dialog->selectedFiles();
                if (!open->isEnabled() || selected.isEmpty() ||
                    QFileInfo(selected.first()).absoluteFilePath() != png.absoluteFilePath())
                    return;
                pickerPoll->stop();
                open->click();
            });
            pickerPoll->start(100);
        });
        connect(timer, &QTimer::timeout, this, [this, timer, started, phase = 0]() mutable {
            const auto fail = [this](const char* why) {
                std::cerr << "FAIL: Qt-to-renderer Sprite Manager smoke: " << why
                          << "\nVisualizer: " << status_->text().toStdString()
                          << "\nSprite Manager: " << spriteManager_->statusText().toStdString()
                          << "\nProcess state: " << int(process_.state())
                          << "\nLast reply: " << lastReplyAction_.toStdString()
                          << "\nPending: " << pendingAction_.toStdString()
                          << "\nQueued commands: " << queuedCommands_.size()
                          << "\nOutput tail: " << (output_->toPlainText() +
                              QString::fromLocal8Bit(outputBuffer_)).right(4000).toStdString()
                          << std::endl;
                QCoreApplication::exit(1);
            };
            if (phase == 0 && started.elapsed() > 8000) {
                auto* library = spriteManager_->findChild<QListWidget*>(
                    QStringLiteral("sprite-import-library"));
                if (!library || library->count() == 0) {
                    fail("File picker never committed PNG to the import library");
                    return;
                }
            }
            if (started.elapsed() > 55000) {
                fail("Control Center did not complete live sprite workflow within 55 seconds");
                return;
            }
            if (!lastReplyOk_ && (lastReplyAction_ == QStringLiteral("sprite.add") ||
                                  lastReplyAction_ == QStringLiteral("sprite.update") ||
                                  lastReplyAction_ == QStringLiteral("sprite.list") ||
                                  lastReplyAction_ == QStringLiteral("sprite.remove"))) {
                fail("Renderer rejected native sprite command");
                return;
            }
            auto* list = spriteManager_->findChild<QListWidget*>(QStringLiteral("sprite-live-list"));
            auto* scale = spriteManager_->findChild<QDoubleSpinBox*>(QStringLiteral("sprite-scale-input"));
            auto* band = spriteManager_->findChild<QComboBox*>(QStringLiteral("sprite-audio-band"));
            auto* apply = spriteManager_->findChild<QPushButton*>(QStringLiteral("sprite-apply"));
            auto* refresh = spriteManager_->findChild<QPushButton*>(QStringLiteral("sprite-refresh"));
            auto* remove = spriteManager_->findChild<QPushButton*>(QStringLiteral("sprite-remove"));
            if (!list || !scale || !band || !apply || !refresh || !remove) {
                fail("Native Sprite Manager controls missing");
                return;
            }
            if (phase == 0 && spriteManager_->isConnected() &&
                lastReplyAction_ == QStringLiteral("sprite.add") &&
                list->count() == 1 && list->currentRow() == 0) {
                if (!apply->isEnabled() || !remove->isEnabled()) {
                    fail("Imported sprite did not enable Apply/Remove");
                    return;
                }
                if (lastRendererSpriteCount_ < 1) {
                    fail("Qt lists PNG but projectM has no active rendered sprite");
                    return;
                }
                auto* library = spriteManager_->findChild<QListWidget*>(
                    QStringLiteral("sprite-import-library"));
                if (!library || library->count() != 1 ||
                    !library->item(0)->text().startsWith(QStringLiteral("Live:"))) {
                    fail("Actual PNG picker did not populate the live imported file library");
                    return;
                }
                scale->setValue(0.73);
                apply->click();
                phase = 1;
            } else if (phase == 1 && lastReplyAction_ == QStringLiteral("sprite.update") &&
                       pendingId_.isEmpty()) {
                const QJsonArray data = spriteManager_->currentState();
                if (data.size() != 1 ||
                    std::abs(data.at(0).toObject().value(QStringLiteral("scale")).toDouble() -
                             0.73) > 0.0001) {
                    fail("Scale edit did not reach the live renderer");
                    return;
                }
                // This must execute the *original renderer-owned* audio-band
                // reconstruction, not merely update a native combobox.
                band->setCurrentIndex(3); // Treble (smoothed)
                phase = 2;
            } else if (phase == 2 && lastReplyAction_ == QStringLiteral("sprite.update") &&
                       pendingId_.isEmpty()) {
                const QJsonArray data = spriteManager_->currentState();
                if (data.size() != 1 ||
                    data.at(0).toObject().value(QStringLiteral("audioBand")).toInt(-1) != 3 ||
                    lastRendererSpriteCount_ < 1) {
                    fail("Treble audio band was not applied by the live sprite backend");
                    return;
                }
                refresh->click();
                phase = 3;
            } else if (phase == 3 && lastReplyAction_ == QStringLiteral("sprite.list") &&
                       pendingId_.isEmpty()) {
                const QJsonArray data = spriteManager_->currentState();
                if (data.size() != 1 ||
                    std::abs(data.at(0).toObject().value(QStringLiteral("scale")).toDouble() -
                             0.73) > 0.0001 ||
                    data.at(0).toObject().value(QStringLiteral("audioBand")).toInt(-1) != 3) {
                    fail("Refresh lost scale or audio-band sprite state");
                    return;
                }
                remove->click();
                phase = 4;
            } else if (phase == 4 && lastReplyAction_ == QStringLiteral("sprite.remove") &&
                       pendingId_.isEmpty() && spriteManager_->currentState().isEmpty() &&
                       list->count() == 0) {
                if (!meshApply_->isEnabled()) {
                    fail("Mesh control not enabled after renderer handshake");
                    return;
                }
                meshX_->setValue(48);
                meshY_->setValue(32);
                meshApply_->click();
                phase = 5;
            } else if (phase == 5 && lastReplyAction_ == QStringLiteral("render.mesh") &&
                       pendingId_.isEmpty()) {
                if (!lastReplyOk_ || lastMeshX_ != 48 || lastMeshY_ != 32) {
                    fail("Native mesh control did not change actual engine mesh");
                    return;
                }
                meshX_->setValue(96);
                meshY_->setValue(54);
                meshApply_->click();
                phase = 6;
            } else if (phase == 6 && lastReplyAction_ == QStringLiteral("render.mesh") &&
                       pendingId_.isEmpty()) {
                if (!lastReplyOk_ || lastMeshX_ != 96 || lastMeshY_ != 54) {
                    fail("Native mesh default restore did not reach engine");
                    return;
                }
                if (display_->count() < 2 || !moveDisplay_->isEnabled()) {
                    fail("SDL display list or Move control missing");
                    return;
                }
                moveDisplay_->click();
                phase = 7;
            } else if (phase == 7 && lastReplyAction_ == QStringLiteral("window.display") &&
                       pendingId_.isEmpty()) {
                if (!lastReplyOk_) {
                    fail("Renderer rejected selected display");
                    return;
                }
                phase = 8;
            } else if (phase == 8 && fpsLabel_->text() != tr("FPS: —")) {
                timer->stop();
                std::cout << "PASS: display picker, measured FPS status bar, live mesh Apply/restore, actual Add PNG file picker, decoded PNG texture, "
                             "Qt live list, numeric Apply, audio-band recompile, Refresh/Remove and renderer IPC" << std::endl;
                QCoreApplication::exit(0);
            }
        });
        timer->start(100);
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
        if (!running) {
            meshApply_->setEnabled(false);
            moveDisplay_->setEnabled(false);
            refreshDisplays_->setEnabled(false);
            fpsLabel_->setText(tr("FPS: —"));
            statsSupported_ = false;
        }
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
        if (display_->currentData().toInt() > 0)
            args << QStringLiteral("--monitor") << QString::number(display_->currentData().toInt());
        args << QStringLiteral("--fullscreen") << (fullscreen_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
        output_->clear();
        process_.setProgram(executable);
        process_.setArguments(args);
        process_.setWorkingDirectory(QFileInfo(executable).absolutePath());
        status_->setText(tr("Starting visualizer..."));
        setRunning(true);
        spriteManager_->setConnected(false);
        spriteManager_->setState(QJsonArray());
        rendererReady_ = false;
        pendingId_.clear();
        pendingAction_.clear();
        queuedCommands_.clear();
        handshakeRetries_ = 0;
        process_.start();
        mailboxPoll_.start();
        sendHandshakeOnStart_ = true;
    }


    void showSpriteManager()
    {
        // Keep the manager embedded in the Control Center. It never becomes
        // a second independent window and shares state with the renderer.
        if (!tabs_ || !spriteManager_) return;
        tabs_->setCurrentWidget(spriteManager_);
        raise();
        activateWindow();
        if (rendererReady_)
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
            // PNG import should work even from a fresh Control Center launch.
            // Start the renderer and replay the sprite.add command after ping,
            // rather than presenting the user with a disabled/broken manager.
            if (action == QStringLiteral("sprite.add") ||
                action == QStringLiteral("sprite.list")) {
                if (process_.state() == QProcess::NotRunning)
                    launchVisualizer();
                if (process_.state() != QProcess::NotRunning) {
                    if (queuedCommands_.size() < 32) {
                        queuedCommands_.enqueue({action, path, params});
                        spriteManager_->setStatus(tr("PNG queued. Connecting to the visualizer..."));
                    } else {
                        spriteManager_->setStatus(tr("Sprite queue full; let the visualizer finish starting."));
                    }
                    return;
                }
                spriteManager_->setStatus(tr("Visualizer could not start. Check the Visualizer output."));
                return;
            }
            status_->setText(tr("Launch the visualizer before sending commands."));
            if (action.startsWith(QStringLiteral("sprite.")))
                spriteManager_->setStatus(tr("Launch the visualizer first."));
            return;
        }

        // Until the renderer has answered ping, all UI commands except ping
        // must remain queued. Otherwise they can preempt the handshake,
        // leaving every native sprite control disabled.
        if ((!rendererReady_ && action != QStringLiteral("ping")) ||
            !pendingId_.isEmpty()) {
            // Refresh is idempotent. Repeated clicks should not fill the
            // mailbox with stale snapshots and delay actual sprite edits.
            if (action == QStringLiteral("sprite.list")) {
                if (pendingAction_ == action) return;
                for (const auto& queued : queuedCommands_) {
                    if (queued.action == action) return;
                }
            }
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
        pendingSpritePath_ = action == QStringLiteral("sprite.add")
            ? params.value(QStringLiteral("path")).toString() : QString();
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
        if (action != QStringLiteral("render.stats"))
            status_->setText(tr("Sent %1; awaiting renderer.").arg(action));
    }

    void sendNextQueuedCommand()
    {
        if (rendererReady_ && pendingId_.isEmpty() && !queuedCommands_.isEmpty()) {
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
                const bool wasStats = pendingAction_ == QStringLiteral("render.stats");
                const bool wasSpriteCommand = pendingAction_.startsWith(QStringLiteral("sprite."));
                const bool wasHandshake = pendingAction_ == QStringLiteral("ping");
                const QString failedSpritePath = pendingSpritePath_;
                pendingSpritePath_.clear();
                pendingId_.clear();
                pendingAction_.clear();
                // Preserve queued PNG imports across startup ping retries.
                // A timed-out transform has unknown state, so discard those.
                if (!wasHandshake && !wasStats)
                    queuedCommands_.clear();
                QFile::remove(mailboxDir_.filePath(QStringLiteral("command.json")));
                if (wasStats) {
                    fpsLabel_->setText(tr("FPS: —"));
                    sendNextQueuedCommand();
                    return;
                }
                status_->setText(tr("Renderer did not respond within 5 seconds."));
                if (wasSpriteCommand) {
                    spriteManager_->setStatus(tr("Sprite command timed out. Check the visualizer log."));
                    if (!failedSpritePath.isEmpty())
                        spriteManager_->reportImportFailure(
                            failedSpritePath, tr("No renderer response within five seconds"));
                }
                if (wasHandshake && handshakeRetries_++ < 3) {
                    spriteManager_->setStatus(tr("Waiting for renderer startup (retry %1/3)...").arg(handshakeRetries_));
                    // Delaying the next ping avoids hammering a renderer that
                    // is still loading presets and building its OpenGL context.
                    QTimer::singleShot(750, this, [this] {
                        if (process_.state() == QProcess::Running && pendingId_.isEmpty())
                            sendCommand(QStringLiteral("ping"));
                    });
                } else if (wasHandshake) {
                    queuedCommands_.clear();
                    spriteManager_->setStatus(tr("Renderer did not complete the handshake. Open Visualizer output for details."));
                }
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
        const QString importPath = pendingSpritePath_;
        pendingSpritePath_.clear();
        pendingId_.clear();
        pendingAction_.clear();
        const bool ok = response.value(QStringLiteral("ok")).toBool();
        if (action == QStringLiteral("render.stats")) {
            const double fps = response.value(QStringLiteral("fps")).toDouble();
            fpsLabel_->setText(ok && fps > 0
                ? tr("FPS: %1").arg(fps, 0, 'f', 1) : tr("FPS: —"));
            sendNextQueuedCommand();
            return;
        }
        lastReplyAction_ = action;
        lastReplyOk_ = ok;
        lastRendererSpriteCount_ = response.value(QStringLiteral("activeSpriteCount")).toInt(-1);
        if (ok && action == QStringLiteral("ping")) {
            handshakeRetries_ = 0;
            rendererReady_ = true;
            statsSupported_ = response.contains(QStringLiteral("fps"));
            spriteManager_->setConnected(true);
            refreshDisplays_->setEnabled(response.value(QStringLiteral("displays")).isArray());
            meshApply_->setEnabled(response.contains(QStringLiteral("meshX")) &&
                                   response.contains(QStringLiteral("meshY")));
        }
        if (ok && response.value(QStringLiteral("displays")).isArray()) {
            const int current = response.value(QStringLiteral("display")).toInt();
            display_->clear();
            display_->addItem(tr("Automatic / OS default"), 0);
            for (const auto& value : response.value(QStringLiteral("displays")).toArray()) {
                const auto monitor = value.toObject();
                const int index = monitor.value(QStringLiteral("index")).toInt();
                QString label = tr("%1: %2 (%3 × %4)")
                    .arg(index).arg(monitor.value(QStringLiteral("name")).toString())
                    .arg(monitor.value(QStringLiteral("width")).toInt())
                    .arg(monitor.value(QStringLiteral("height")).toInt());
                const int hz = monitor.value(QStringLiteral("refresh")).toInt();
                if (hz > 0) label += tr(" · %1 Hz").arg(hz);
                display_->addItem(label, index);
            }
            display_->setCurrentIndex(qMax(0, display_->findData(current)));
            moveDisplay_->setEnabled(rendererReady_ && current > 0);
            if (action == QStringLiteral("window.display")) saveSettings();
        }
        if (ok && (action == QStringLiteral("ping") || action == QStringLiteral("render.mesh")) &&
            response.contains(QStringLiteral("meshX")) && response.contains(QStringLiteral("meshY"))) {
            lastMeshX_ = response.value(QStringLiteral("meshX")).toInt();
            lastMeshY_ = response.value(QStringLiteral("meshY")).toInt();
            meshX_->setValue(lastMeshX_);
            meshY_->setValue(lastMeshY_);
        }
        if (ok && response.value(QStringLiteral("sprites")).isArray()) {
            // Defer a state refresh until all pending slider edits have landed.
            if (queuedCommands_.isEmpty() || action != QStringLiteral("sprite.update"))
                spriteManager_->setState(response.value(QStringLiteral("sprites")).toArray());
        }
        if (!ok) {
            const QString error = response.value(QStringLiteral("error")).toString();
            status_->setText(tr("Renderer rejected command: %1").arg(error));
            if (action == QStringLiteral("sprite.add") && !importPath.isEmpty())
                spriteManager_->reportImportFailure(importPath, error);
            else if (action.startsWith(QStringLiteral("sprite.")))
                spriteManager_->setStatus(error);
        } else {
            status_->setText(tr("Renderer connected. Preset #%1, shuffle: %2, locked: %3")
                .arg(response.value(QStringLiteral("presetIndex")).toInt())
                .arg(response.value(QStringLiteral("shuffle")).toBool() ? tr("on") : tr("off"))
                .arg(response.value(QStringLiteral("locked")).toBool() ? tr("yes") : tr("no")));
            if (action == QStringLiteral("window.display"))
                status_->setText(tr("Visualizer moved to display %1.").arg(display_->currentData().toInt()));
            if (action == QStringLiteral("render.mesh"))
                status_->setText(tr("Live mesh applied and saved: %1 × %2").arg(lastMeshX_).arg(lastMeshY_));
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
        const int savedDisplay = s.value(QStringLiteral("display"), 0).toInt();
        if (savedDisplay > 0) {
            display_->addItem(tr("Previously selected display %1").arg(savedDisplay), savedDisplay);
            display_->setCurrentIndex(display_->count() - 1);
        }
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
        s.setValue(QStringLiteral("display"), display_->currentData().toInt());
        s.setValue(QStringLiteral("geometry"), saveGeometry());
    }

    QProcess process_;
    QTemporaryDir mailboxDir_;
    QTimer mailboxPoll_;
    QTimer statsPoll_;
    bool statsSupported_ = false;
    QTimer outputPoll_;
    QByteArray outputBuffer_;
    QString pendingId_;
    QString pendingAction_;
    QString pendingSpritePath_;
    QQueue<Command> queuedCommands_;
    QElapsedTimer pendingElapsed_;
    bool sendHandshakeOnStart_ = false;
    bool rendererReady_ = false;
    QString lastReplyAction_;
    bool lastReplyOk_ = true;
    int lastRendererSpriteCount_ = -1;
    int handshakeRetries_ = 0;
    QLineEdit* executable_ = nullptr;
    QLineEdit* presetPath_ = nullptr;
    QLineEdit* texturePath_ = nullptr;
    QSpinBox* width_ = nullptr;
    QSpinBox* height_ = nullptr;
    QSpinBox* meshX_ = nullptr;
    QSpinBox* meshY_ = nullptr;
    QPushButton* meshApply_ = nullptr;
    int lastMeshX_ = -1;
    int lastMeshY_ = -1;
    QCheckBox* fullscreen_ = nullptr;
    QComboBox* display_ = nullptr;
    QPushButton* moveDisplay_ = nullptr;
    QPushButton* refreshDisplays_ = nullptr;
    QLabel* fpsLabel_ = nullptr;
    QPushButton* start_ = nullptr;
    QPushButton* stop_ = nullptr;
    QLabel* status_ = nullptr;
    QPlainTextEdit* output_ = nullptr;
    QWidget* optionsWindow_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    NativeSpriteManager* spriteManager_ = nullptr;
};

int main(int argc, char** argv)
{
#ifdef Q_OS_WIN
    // The Control Center and its renderer child are two windows of one app.
    // An explicit shared AppUserModelID groups them under one taskbar identity.
    SetCurrentProcessExplicitAppUserModelID(L"ThisIs395.VJ");
#endif
#ifdef THISIS395_VJ_EMBEDDED_RENDERER
    const bool verifyControlDir = argc > 1 && std::strcmp(argv[1], "--vj-renderer-probe") == 0;
    if (verifyControlDir || (argc > 1 && std::strcmp(argv[1], "--vj-renderer") == 0)) {
        std::vector<char*> rendererArgs;
        rendererArgs.reserve(static_cast<std::size_t>(argc));
        rendererArgs.push_back(argv[0]);
        for (int i = 2; i < argc; ++i)
            rendererArgs.push_back(argv[i]);
        int rendererArgc = static_cast<int>(rendererArgs.size());
        rendererArgs.push_back(nullptr); // Preserve argv[argc] sentinel.
        // Initialize Qt Core in renderer mode too: QImageReader needs a
        // reliable application/plugin environment to decode imported PNGs,
        // especially inside the deployed Windows/macOS app layout.
        QCoreApplication imageRuntime(rendererArgc, rendererArgs.data());
        return RunProjectMRenderer(rendererArgc, rendererArgs.data(), verifyControlDir);
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
    if (argc > 2 && std::strcmp(argv[1], "--vj-control-smoke") == 0) {
        const QString pngPath = QString::fromLocal8Bit(argv[2]);
        QTimer::singleShot(0, &window, [&window, pngPath] {
            window.runEndToEndSpriteSmoke(pngPath);
        });
    }
    return app.exec();
}
