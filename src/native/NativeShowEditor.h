#pragma once

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSaveFile>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

class NativeShowEditor final : public QWidget {
public:
    explicit NativeShowEditor(QWidget* parent = nullptr) : QWidget(parent) {
        auto* root = new QVBoxLayout(this);
        auto* header = new QHBoxLayout;
        auto* open = new QPushButton(tr("Open show..."));
        auto* save = new QPushButton(tr("Save show..."));
        header->addWidget(open);
        header->addWidget(save);
        header->addStretch();
        root->addLayout(header);

        auto* pages = new QTabWidget;
        root->addWidget(pages, 1);
        auto* library = new QWidget;
        auto* libLayout = new QVBoxLayout(library);
        libLayout->addWidget(new QLabel(tr("Preset library (.milk)")));
        presets_ = new QListWidget;
        libLayout->addWidget(presets_, 1);
        auto* addPresets = new QPushButton(tr("Import presets..."));
        auto* toSequence = new QPushButton(tr("Add selected to sequence +"));
        auto* libActions = new QHBoxLayout;
        libActions->addWidget(addPresets);
        libActions->addWidget(toSequence);
        libLayout->addLayout(libActions);
        libLayout->addWidget(new QLabel(tr("PNG sprite library")));
        sprites_ = new QListWidget;
        libLayout->addWidget(sprites_, 1);
        auto* addSprites = new QPushButton(tr("Import PNG sprites..."));
        auto* spriteToSequence = new QPushButton(tr("Add selected sprite +"));
        auto* spriteActions = new QHBoxLayout;
        spriteActions->addWidget(addSprites);
        spriteActions->addWidget(spriteToSequence);
        libLayout->addLayout(spriteActions);
        pages->addTab(library, tr("Playlist"));

        auto* sequence = new QWidget;
        auto* seqLayout = new QVBoxLayout(sequence);
        seqLayout->addWidget(new QLabel(tr("Sequence: edit timing and reorder clips")));
        clips_ = new QListWidget;
        clips_->setSelectionMode(QAbstractItemView::SingleSelection);
        seqLayout->addWidget(clips_, 1);
        auto* timing = new QFormLayout;
        duration_ = new QDoubleSpinBox;
        duration_->setRange(0.1, 36000);
        duration_->setDecimals(2);
        duration_->setValue(30);
        fade_ = new QDoubleSpinBox;
        fade_->setRange(0, 15);
        fade_->setDecimals(2);
        fade_->setValue(3);
        timing->addRow(tr("Duration (seconds)"), duration_);
        timing->addRow(tr("Fade (seconds)"), fade_);
        seqLayout->addLayout(timing);
        auto* move = new QHBoxLayout;
        auto* up = new QPushButton(tr("Move up"));
        auto* down = new QPushButton(tr("Move down"));
        auto* remove = new QPushButton(tr("Remove"));
        move->addWidget(up);
        move->addWidget(down);
        move->addWidget(remove);
        seqLayout->addLayout(move);
        pages->addTab(sequence, tr("Sequence"));

        auto* render = new QWidget;
        auto* renderLayout = new QVBoxLayout(render);
        auto* renderForm = new QFormLayout;
        width_ = new QSpinBox;
        width_->setRange(16, 7680);
        width_->setSingleStep(2);
        width_->setValue(1920);
        height_ = new QSpinBox;
        height_->setRange(16, 4320);
        height_->setSingleStep(2);
        height_->setValue(1080);
        fps_ = new QSpinBox;
        fps_->setRange(1, 240);
        fps_->setValue(60);
        codec_ = new QComboBox;
        codec_->addItem(tr("H.264"), QStringLiteral("h264"));
        codec_->addItem(tr("H.265 / HEVC"), QStringLiteral("hevc"));
        codec_->addItem(tr("ProRes 422"), QStringLiteral("prores_422"));
        bitrate_ = new QSpinBox;
        bitrate_->setRange(1, 500);
        bitrate_->setValue(18);
        audio_ = new QLineEdit;
        audioDuration_ = new QDoubleSpinBox;
        audioDuration_->setRange(0.1, 36000);
        audioDuration_->setValue(180);
        audioGain_ = new QDoubleSpinBox;
        audioGain_->setRange(0, 2);
        audioGain_->setSingleStep(0.1);
        audioGain_->setValue(1);
        video_ = new QLineEdit;
        renderForm->addRow(tr("Width"), width_);
        renderForm->addRow(tr("Height"), height_);
        renderForm->addRow(tr("FPS"), fps_);
        renderForm->addRow(tr("Codec"), codec_);
        renderForm->addRow(tr("Bitrate (Mbps)"), bitrate_);
        renderForm->addRow(tr("Audio source"), fileRow(audio_, tr("Choose audio"), tr("Audio files (*)"), false));
        renderForm->addRow(tr("Audio duration"), audioDuration_);
        renderForm->addRow(tr("Audio gain"), audioGain_);
        renderForm->addRow(tr("Video output"), fileRow(video_, tr("Choose video output"), tr("Video files (*.mp4 *.mov)"), true));
        renderLayout->addLayout(renderForm);
        renderLayout->addStretch();
        pages->addTab(render, tr("Render"));

        auto* exportPage = new QWidget;
        auto* exportLayout = new QVBoxLayout(exportPage);
        exportLayout->addWidget(new QLabel(tr("Export a render-job manifest compatible with tools/render_offline.py")));
        auto* exportJob = new QPushButton(tr("Save render job JSON..."));
        exportLayout->addWidget(exportJob);
        exportLayout->addWidget(new QLabel(tr("Video rendering uses the separate Python + FFmpeg backend. "
            "Exporting this manifest does not launch a render.")));
        exportLayout->addStretch();
        pages->addTab(exportPage, tr("Export"));
        notice_ = new QLabel(tr("Changes are local to the native editor until saved. Live SDL synchronization is not yet implemented."));
        notice_->setWordWrap(true);
        root->addWidget(notice_);

        connect(open, &QPushButton::clicked, this, [this] { openShow(); });
        connect(save, &QPushButton::clicked, this, [this] { saveShow(); });
        connect(addPresets, &QPushButton::clicked, this, [this] {
            importFiles(presets_, tr("Import .milk presets"), tr("MilkDrop presets (*.milk)"));
        });
        connect(addSprites, &QPushButton::clicked, this, [this] {
            importFiles(sprites_, tr("Import sprites"), tr("PNG sprites (*.png)"));
        });
        connect(toSequence, &QPushButton::clicked, this, [this] { addClip(presets_, false); });
        connect(spriteToSequence, &QPushButton::clicked, this, [this] { addClip(sprites_, true); });
        connect(clips_, &QListWidget::currentRowChanged, this, [this](int row) {
            if (row < 0 || row >= entries_.size()) return;
            editing_ = true;
            duration_->setValue(entries_[row].duration);
            fade_->setValue(entries_[row].fade);
            editing_ = false;
        });
        connect(duration_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) {
            editTiming(value, true);
        });
        connect(fade_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) {
            editTiming(value, false);
        });
        connect(remove, &QPushButton::clicked, this, [this] {
            const int i = clips_->currentRow();
            if (i >= 0 && i < entries_.size()) {
                entries_.removeAt(i);
                refreshClips();
                clips_->setCurrentRow(std::min(i, entries_.size() - 1));
            }
        });
        connect(up, &QPushButton::clicked, this, [this] { moveClip(-1); });
        connect(down, &QPushButton::clicked, this, [this] { moveClip(1); });
        connect(exportJob, &QPushButton::clicked, this, [this] { exportManifest(); });
    }

private:
    struct Clip {
        QString path;
        bool sprite = false;
        double duration = 30;
        double fade = 3;
    };
    QVector<Clip> entries_;
    QListWidget* presets_ = nullptr;
    QListWidget* sprites_ = nullptr;
    QListWidget* clips_ = nullptr;
    QDoubleSpinBox* duration_ = nullptr;
    QDoubleSpinBox* fade_ = nullptr;
    QSpinBox *width_ = nullptr, *height_ = nullptr, *fps_ = nullptr, *bitrate_ = nullptr;
    QComboBox* codec_ = nullptr;
    QLineEdit *audio_ = nullptr, *video_ = nullptr;
    QDoubleSpinBox *audioDuration_ = nullptr, *audioGain_ = nullptr;
    QLabel* notice_ = nullptr;
    bool editing_ = false;

    QWidget* fileRow(QLineEdit* edit, const QString& title, const QString& filter, bool save) {
        auto* wrapper = new QWidget;
        auto* layout = new QHBoxLayout(wrapper);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* button = new QPushButton(tr("Browse..."));
        layout->addWidget(edit);
        layout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, edit, title, filter, save] {
            QString result = save ? QFileDialog::getSaveFileName(this, title, edit->text(), filter)
                                  : QFileDialog::getOpenFileName(this, title, edit->text(), filter);
            if (!result.isEmpty()) edit->setText(result);
        });
        return wrapper;
    }
    void importFiles(QListWidget* target, const QString& title, const QString& filter) {
        for (const auto& file : QFileDialog::getOpenFileNames(this, title, QString(), filter)) {
            const QString path = QFileInfo(file).absoluteFilePath();
            if (target->findItems(path, Qt::MatchExactly).isEmpty())
                target->addItem(path);
        }
    }
    void addClip(QListWidget* library, bool sprite) {
        if (!library->currentItem()) return;
        entries_.append({library->currentItem()->text(), sprite, duration_->value(), fade_->value()});
        refreshClips();
        clips_->setCurrentRow(entries_.size() - 1);
    }
    void editTiming(double value, bool duration) {
        const int i = clips_->currentRow();
        if (editing_ || i < 0 || i >= entries_.size()) return;
        if (duration) entries_[i].duration = value;
        else entries_[i].fade = value;
        refreshClips(i);
    }
    void moveClip(int direction) {
        int i = clips_->currentRow(), target = i + direction;
        if (i < 0 || target < 0 || target >= entries_.size()) return;
        entries_.swapItemsAt(i, target);
        refreshClips();
        clips_->setCurrentRow(target);
    }
    void refreshClips(int keepSelection = -1) {
        editing_ = true;
        clips_->clear();
        for (const auto& clip : entries_)
            clips_->addItem(QStringLiteral("%1  %2  (%3 s, fade %4 s)")
                .arg(clip.sprite ? QStringLiteral("SPRITE") : QStringLiteral("PRESET"))
                .arg(QFileInfo(clip.path).fileName())
                .arg(clip.duration).arg(clip.fade));
        editing_ = false;
        if (keepSelection >= 0) clips_->setCurrentRow(keepSelection);
    }
    static QJsonObject encodeClip(const Clip& clip) {
        return {{QStringLiteral("path"), clip.path},
                {QStringLiteral("sprite"), clip.sprite},
                {QStringLiteral("durationSeconds"), clip.duration},
                {QStringLiteral("fadeSeconds"), clip.fade}};
    }
    QJsonObject project() const {
        QJsonArray entries, presets, sprites;
        for (const auto& c : entries_) entries.append(encodeClip(c));
        for (int i = 0; i < presets_->count(); ++i) presets.append(presets_->item(i)->text());
        for (int i = 0; i < sprites_->count(); ++i) sprites.append(sprites_->item(i)->text());
        return {{QStringLiteral("schema"), QStringLiteral("thisis395.native-show.v1")},
                {QStringLiteral("clips"), entries}, {QStringLiteral("presets"), presets},
                {QStringLiteral("sprites"), sprites},
                {QStringLiteral("render"), QJsonObject{
                    {QStringLiteral("width"), width_->value()}, {QStringLiteral("height"), height_->value()},
                    {QStringLiteral("fps"), fps_->value()}, {QStringLiteral("codec"), codec_->currentData().toString()},
                    {QStringLiteral("bitrateMbps"), bitrate_->value()},
                    {QStringLiteral("audio"), audio_->text()}, {QStringLiteral("audioDuration"), audioDuration_->value()},
                    {QStringLiteral("audioGain"), audioGain_->value()}, {QStringLiteral("video"), video_->text()}}}};
    }
    bool writeJson(const QString& path, const QJsonObject& data) {
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(this, tr("Write failed"), file.errorString());
            return false;
        }
        if (file.write(QJsonDocument(data).toJson(QJsonDocument::Indented)) < 0 || !file.commit()) {
            QMessageBox::warning(this, tr("Write failed"), file.errorString());
            return false;
        }
        notice_->setText(tr("Saved: %1").arg(path));
        return true;
    }
    void saveShow() {
        QString path = QFileDialog::getSaveFileName(this, tr("Save show"), QString(), tr("ThisIs395 show (*.json)"));
        if (!path.isEmpty()) writeJson(path, project());
    }
    void openShow() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Open show"), QString(), tr("ThisIs395 show (*.json)"));
        if (path.isEmpty()) return;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, tr("Open failed"), file.errorString());
            return;
        }
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
        const auto data = doc.object();
        if (error.error != QJsonParseError::NoError ||
            data.value(QStringLiteral("schema")).toString() != QStringLiteral("thisis395.native-show.v1")) {
            QMessageBox::warning(this, tr("Invalid show"), tr("Expected a thisis395.native-show.v1 project file."));
            return;
        }
        entries_.clear();
        presets_->clear();
        sprites_->clear();
        for (const auto& value : data.value(QStringLiteral("presets")).toArray())
            presets_->addItem(value.toString());
        for (const auto& value : data.value(QStringLiteral("sprites")).toArray())
            sprites_->addItem(value.toString());
        for (const auto& value : data.value(QStringLiteral("clips")).toArray()) {
            const auto c = value.toObject();
            const QString path = c.value(QStringLiteral("path")).toString();
            const double duration = c.value(QStringLiteral("durationSeconds")).toDouble(30);
            const double fade = c.value(QStringLiteral("fadeSeconds")).toDouble(3);
            if (!path.isEmpty() && duration > 0 && fade >= 0)
                entries_.append({path, c.value(QStringLiteral("sprite")).toBool(), duration, fade});
        }
        const auto r = data.value(QStringLiteral("render")).toObject();
        width_->setValue(r.value(QStringLiteral("width")).toInt(1920));
        height_->setValue(r.value(QStringLiteral("height")).toInt(1080));
        fps_->setValue(r.value(QStringLiteral("fps")).toInt(60));
        bitrate_->setValue(r.value(QStringLiteral("bitrateMbps")).toInt(18));
        int codec = codec_->findData(r.value(QStringLiteral("codec")).toString(QStringLiteral("h264")));
        codec_->setCurrentIndex(std::max(codec, 0));
        audio_->setText(r.value(QStringLiteral("audio")).toString());
        audioDuration_->setValue(r.value(QStringLiteral("audioDuration")).toDouble(180));
        audioGain_->setValue(r.value(QStringLiteral("audioGain")).toDouble(1));
        video_->setText(r.value(QStringLiteral("video")).toString());
        refreshClips();
        notice_->setText(tr("Loaded show: %1").arg(path));
    }
    void exportManifest() {
        if (video_->text().trimmed().isEmpty() || entries_.isEmpty()) {
            QMessageBox::warning(this, tr("Incomplete job"), tr("Add clips and select an output video path."));
            return;
        }
        if (width_->value() % 2 || height_->value() % 2) {
            QMessageBox::warning(this, tr("Invalid dimensions"), tr("Video width and height must be even."));
            return;
        }
        QJsonArray presets, sprites;
        double presetOffset = 0, spriteOffset = 0;
        for (const auto& c : entries_) {
            if (!QFileInfo::exists(c.path)) {
                QMessageBox::warning(this, tr("Missing asset"), c.path);
                return;
            }
            QJsonObject item{{QStringLiteral("path"), c.path},
                             {QStringLiteral("startSeconds"), c.sprite ? spriteOffset : presetOffset},
                             {QStringLiteral("durationSeconds"), c.duration},
                             {QStringLiteral("fadeSeconds"), c.fade}};
            if (c.sprite) { sprites.append(item); spriteOffset += c.duration; }
            else { presets.append(item); presetOffset += c.duration; }
        }
        if (presets.isEmpty()) {
            QMessageBox::warning(this, tr("No presets"), tr("At least one preset clip is required."));
            return;
        }
        const QString source = audio_->text().trimmed();
        if (!source.isEmpty() && !QFileInfo::exists(source)) {
            QMessageBox::warning(this, tr("Missing audio"), source);
            return;
        }
        const QString destination = QFileDialog::getSaveFileName(this, tr("Save render manifest"),
                                                                 QString(), tr("Render job (*.json)"));
        if (destination.isEmpty()) return;
        if (QFileInfo::exists(destination) &&
            QMessageBox::question(this, tr("Replace manifest?"), tr("Replace the existing JSON file?")) != QMessageBox::Yes)
            return;
        QJsonObject job{
            {QStringLiteral("schema"), QStringLiteral("thisis395.render-job.v1")},
            {QStringLiteral("status"), QStringLiteral("planned")},
            {QStringLiteral("encoderImplemented"), true},
            {QStringLiteral("output"), QJsonObject{
                {QStringLiteral("path"), video_->text()}, {QStringLiteral("width"), width_->value()},
                {QStringLiteral("height"), height_->value()}, {QStringLiteral("fps"), fps_->value()},
                {QStringLiteral("codec"), codec_->currentData().toString()},
                {QStringLiteral("bitrateMbps"), bitrate_->value()}}},
            {QStringLiteral("audioSource"), source},
            {QStringLiteral("audioTrack"), QJsonObject{
                {QStringLiteral("path"), source}, {QStringLiteral("startSeconds"), 0},
                {QStringLiteral("durationSeconds"), audioDuration_->value()}, {QStringLiteral("gain"), audioGain_->value()}}},
            {QStringLiteral("tracks"), QJsonObject{{QStringLiteral("presets"), presets}, {QStringLiteral("sprites"), sprites}}}
        };
        writeJson(destination, job);
    }
};
