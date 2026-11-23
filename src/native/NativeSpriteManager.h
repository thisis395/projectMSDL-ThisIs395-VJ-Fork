#pragma once

#include <QAbstractSpinBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QApplication>
#include <QImageReader>
#include <QImage>
#include <QSize>
#include <QFileDialog>
#include <QFrame>
#include <QAbstractItemView>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <utility>

// Dedicated Qt6 widget using the *same* renderer-owned SpriteManagerWindow
// as the legacy ImGui interface. Commands are issued via the local mailbox.
class NativeSpriteManager final : public QWidget {
public:
    using Dispatch = std::function<void(const QString&, const QJsonObject&)>;

    explicit NativeSpriteManager(QWidget* parent = nullptr) : QWidget(parent) {
        auto* root = new QVBoxLayout(this);
        auto* heading = new QLabel(tr("Live PNG sprites (maximum 16)"));
        root->addWidget(heading);

        auto* actions = new QHBoxLayout;
        add_ = new QPushButton(tr("Add PNG..."));
        add_->setObjectName(QStringLiteral("sprite-add"));
        remove_ = new QPushButton(tr("Remove"));
        remove_->setObjectName(QStringLiteral("sprite-remove"));
        refresh_ = new QPushButton(tr("Refresh"));
        refresh_->setObjectName(QStringLiteral("sprite-refresh"));
        actions->addWidget(add_);
        actions->addWidget(remove_);
        actions->addWidget(refresh_);
        root->addLayout(actions);

        // Imported assets and live renderer sprites are different states.
        // Previously the only list was the live renderer snapshot, so even a
        // selected PNG simply vanished from view when renderer import failed.
        auto* libraryTitle = new QLabel(tr("Imported PNG files (double-click to retry loading)"));
        root->addWidget(libraryTitle);
        library_ = new QListWidget;
        library_->setObjectName(QStringLiteral("sprite-import-library"));
        library_->setMaximumHeight(100);
        root->addWidget(library_);
        root->addWidget(new QLabel(tr("Sprites currently active in visualizer")));

        list_ = new QListWidget;
        list_->setObjectName(QStringLiteral("sprite-live-list"));
        list_->setSelectionMode(QAbstractItemView::SingleSelection);
        root->addWidget(list_, 1);

        auto* settings = new QGroupBox(tr("Selected sprite"));
        auto* settingsScroll = new QScrollArea;
        settingsScroll->setWidgetResizable(true);
        settingsScroll->setFrameShape(QFrame::NoFrame);
        auto* content = new QWidget;
        auto* contentLayout = new QVBoxLayout(content);
        auto* form = new QFormLayout;
        x_ = addSlider(form, tr("X position"), 0, 1, 0.5, 0.01, 3);
        y_ = addSlider(form, tr("Y position"), 0, 1, 0.5, 0.01, 3);
        scale_ = addSlider(form, tr("Scale"), 0.02, 2, 0.25, 0.01, 3);
        scale_->setObjectName(QStringLiteral("sprite-scale-input"));
        opacity_ = addSlider(form, tr("Opacity"), 0, 1, 1, 0.01, 3);
        rotation_ = addSlider(form, tr("Rotation (degrees)"), -180, 180, 0, 1, 1);
        gain_ = addSlider(form, tr("Audio gain"), 0, 3, 0.35, 0.05, 3);
        band_ = new QComboBox;
        band_->addItems({tr("Off"), tr("Bass (smoothed)"),
                         tr("Mids (smoothed)"), tr("Treble (smoothed)")});
        form->addRow(tr("Audio band"), band_);
        contentLayout->addLayout(form);
        apply_ = new QPushButton(tr("Apply to live visualizer"));
        apply_->setObjectName(QStringLiteral("sprite-apply"));
        contentLayout->addWidget(apply_);
        settingsScroll->setWidget(content);
        auto* boxLayout = new QVBoxLayout(settings);
        boxLayout->addWidget(settingsScroll);
        root->addWidget(settings, 2);
        preview_ = new QLabel;
        preview_->setMinimumHeight(50);
        preview_->setMaximumHeight(110);
        preview_->setAlignment(Qt::AlignCenter);
        root->addWidget(preview_);

        status_ = new QLabel(tr("Launch the visualizer to manage live sprites."));
        status_->setWordWrap(true);
        root->addWidget(status_);
        setConnected(false);
        add_->setToolTip(tr("Choose a PNG. The visualizer starts automatically if necessary."));
        refresh_->setToolTip(tr("Refresh the live list and connect to the visualizer if necessary."));
        remove_->setToolTip(tr("Select a loaded sprite before removing it."));
        apply_->setToolTip(tr("Select a loaded sprite before applying scale, position and other changes."));
        setStatus(tr("Add PNG starts the visualizer automatically. Select a loaded sprite to enable its controls."));

        connect(add_, &QPushButton::clicked, this, [this] {
            // Non-blocking dialog: do not nest a modal Qt event loop while
            // command acknowledgments and the SDL renderer are active.
            auto* dialog = new QFileDialog(this, tr("Add transparent PNG sprite"));
            dialog->setFileMode(QFileDialog::ExistingFile);
            dialog->setNameFilter(tr("PNG images (*.png)"));
            // The native Windows file picker can outlive a temporary Qt
            // widget and its callbacks. Keep selection inside the Qt event
            // loop and make the same path testable on every platform.
            dialog->setOption(QFileDialog::DontUseNativeDialog, true);
            dialog->setObjectName(QStringLiteral("sprite-file-picker"));
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            connect(dialog, &QFileDialog::fileSelected, this,
                    [this](const QString& selected) { importSpriteFile(selected); });
            dialog->open();
        });
        connect(remove_, &QPushButton::clicked, this, [this] {
            if (list_->currentRow() < 0) return;
            dispatch(QStringLiteral("sprite.remove"), {{QStringLiteral("index"), list_->currentRow()}});
        });
        connect(refresh_, &QPushButton::clicked, this, [this] {
            dispatch(QStringLiteral("sprite.list"), {});
        });
        connect(apply_, &QPushButton::clicked, this, [this] { applyChanges(); });
        connect(list_, &QListWidget::currentRowChanged, this,
                [this](int row) { loadSelection(row); });
        connect(library_, &QListWidget::itemDoubleClicked, this,
                [this](QListWidgetItem* item) {
                    if (item) dispatch(QStringLiteral("sprite.add"),
                        {{QStringLiteral("path"), item->data(Qt::UserRole).toString()}});
                });
        connect(band_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int) { if (!syncing_) applyChanges(); });
    }

    void setDispatch(Dispatch callback) { dispatch_ = std::move(callback); }

    bool isConnected() const { return connected_; }
    QJsonArray currentState() const { return data_; }

    // Shared by the asynchronous file picker and GUI integration tests.
    void importSpriteFile(const QString& selected) {
        const QFileInfo file(selected);
        if (!file.isFile() || !file.isReadable() ||
            file.suffix().compare(QStringLiteral("png"), Qt::CaseInsensitive) != 0) {
            setStatus(tr("Select an accessible PNG image."));
            return;
        }
        if (list_->count() >= 16) {
            setStatus(tr("Only 16 live sprites are supported."));
            return;
        }
        const QString path = file.absoluteFilePath();
        bool alreadyListed = false;
        for (int i = 0; i < library_->count(); ++i) {
            if (library_->item(i)->data(Qt::UserRole).toString() == path) {
                alreadyListed = true;
                break;
            }
        }
        if (!alreadyListed) {
            auto* item = new QListWidgetItem(tr("Queued: %1").arg(file.fileName()));
            item->setData(Qt::UserRole, path);
            item->setToolTip(path);
            library_->addItem(item);
        }
        setStatus(tr("Importing %1...").arg(file.fileName()));
        dispatch(QStringLiteral("sprite.add"), {{QStringLiteral("path"), path}});
    }

    void setConnected(bool value) {
        connected_ = value;
        // Importing a PNG is permitted before the renderer starts. The
        // parent will launch the visualizer and queue the pending import.
        add_->setEnabled(true);
        // Refresh may also start the renderer when it is not running.
        refresh_->setEnabled(true);
        updateSelectionEnabled();
        if (!value) setStatus(tr("No live renderer connection. Launch the visualizer first."));
    }

    void setStatus(const QString& message) { status_->setText(message); }

    void setState(const QJsonArray& items, bool refreshControls = false) {
        // Renderer replies arrive for every update, sometimes many times per
        // second. Do not clear the list or re-decode a potentially huge PNG
        // merely because a new scale/gain acknowledgment arrived.
        bool pathsChanged = list_->count() != items.size();
        if (!pathsChanged) {
            for (int i = 0; i < items.size(); ++i) {
                const QString nextPath = items.at(i).toObject().value(QStringLiteral("path")).toString();
                if (list_->item(i)->data(Qt::UserRole).toString() != nextPath) {
                    pathsChanged = true;
                    break;
                }
            }
        }

        data_ = items;
        for (int i = 0; i < library_->count(); ++i) {
            auto* candidate = library_->item(i);
            const QString libraryPath = candidate->data(Qt::UserRole).toString();
            bool active = false;
            for (const auto& item : items) {
                if (item.toObject().value(QStringLiteral("path")).toString() == libraryPath) {
                    active = true;
                    break;
                }
            }
            candidate->setText((active ? tr("Live: %1") : tr("Not live: %1"))
                .arg(QFileInfo(libraryPath).fileName()));
        }
        if (pathsChanged) {
            const QString previousPath = list_->currentItem()
                ? list_->currentItem()->data(Qt::UserRole).toString()
                : QString();
            const int previousRow = list_->currentRow();
            QSignalBlocker blocker(list_);
            list_->clear();
            for (const auto& value : items) {
                const QString path = value.toObject().value(QStringLiteral("path")).toString();
                auto* item = new QListWidgetItem(QFileInfo(path).fileName());
                item->setData(Qt::UserRole, path);
                item->setToolTip(path);
                list_->addItem(item);
            }
            int selected = -1;
            for (int i = 0; i < list_->count(); ++i) {
                if (list_->item(i)->data(Qt::UserRole).toString() == previousPath) {
                    selected = i;
                    break;
                }
            }
            if (selected < 0 && list_->count() > 0)
                selected = std::min(previousRow >= 0 ? previousRow : 0, list_->count() - 1);
            list_->setCurrentRow(selected);
        }

        // Do not erase partially typed numeric values when a network/IPC
        // acknowledgment arrives. Explicit Refresh may update the controls,
        // but not while an editor owns keyboard focus.
        QWidget* focus = QApplication::focusWidget();
        const std::initializer_list<QDoubleSpinBox*> controls{
            x_, y_, scale_, opacity_, rotation_, gain_};
        const bool editing = focus && std::any_of(
            controls.begin(), controls.end(), [focus](QDoubleSpinBox* spin) {
                return spin && (spin == focus || spin->isAncestorOf(focus));
            });
        if ((pathsChanged || refreshControls) && !editing)
            loadSelection(list_->currentRow());
        else
            updateSelectionEnabled();
        setStatus(tr("%1 / 16 live sprites.").arg(list_->count()));
    }

private:
    // All non-rotation sliders have 0.001 increments; rotation also uses
    // one decimal degree in its spinbox. The engine still receives radians.
    QDoubleSpinBox* addSlider(QFormLayout* form, const QString& label,
                              double low, double high, double initial,
                              double step, int decimals) {
        auto* row = new QWidget;
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* slider = new QSlider(Qt::Horizontal);
        auto* spin = new QDoubleSpinBox;
        spin->setDecimals(decimals);
        // With keyboard tracking on, partial text like "0." triggered the
        // linked slider and rewrote the spinbox while the user was typing.
        spin->setKeyboardTracking(false);
        spin->setRange(low, high);
        spin->setSingleStep(step);
        spin->setValue(initial);
        slider->setRange(static_cast<int>(std::round(low * 1000)),
                         static_cast<int>(std::round(high * 1000)));
        slider->setValue(static_cast<int>(std::round(initial * 1000)));
        layout->addWidget(slider, 1);
        layout->addWidget(spin);
        form->addRow(label, row);
        connect(slider, &QSlider::valueChanged, spin,
                [spin](int value) { spin->setValue(value / 1000.0); });
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), slider,
                [slider](double value) { slider->setValue(static_cast<int>(std::round(value * 1000))); });
        connect(slider, &QSlider::sliderReleased, this, [this] {
            if (!syncing_) applyChanges();
        });
        connect(spin, &QAbstractSpinBox::editingFinished, this, [this] {
            if (!syncing_) applyChanges();
        });
        return spin;
    }

    void dispatch(const QString& action, const QJsonObject& params) {
        if (!dispatch_) {
            setStatus(tr("No renderer controller attached."));
            return;
        }
        if (!connected_ && action != QStringLiteral("sprite.add") &&
            action != QStringLiteral("sprite.list")) {
            setStatus(tr("Start the visualizer before editing live sprites."));
            return;
        }
        if (!connected_)
            setStatus(action == QStringLiteral("sprite.add")
                ? tr("Starting visualizer and queuing PNG import...")
                : tr("Connecting to visualizer and refreshing sprites..."));
        dispatch_(action, params);
    }

    void updateSelectionEnabled() {
        const bool selected = connected_ && list_->currentRow() >= 0;
        remove_->setEnabled(selected);
        apply_->setEnabled(selected);
        for (auto* spin : {x_, y_, scale_, opacity_, rotation_, gain_})
            spin->setEnabled(selected);
        band_->setEnabled(selected);
    }

    void loadSelection(int row) {
        syncing_ = true;
        const bool valid = row >= 0 && row < data_.size();
        if (valid) {
            const auto obj = data_.at(row).toObject();
            x_->setValue(obj.value(QStringLiteral("x")).toDouble(0.5));
            y_->setValue(obj.value(QStringLiteral("y")).toDouble(0.5));
            scale_->setValue(obj.value(QStringLiteral("scale")).toDouble(0.25));
            opacity_->setValue(obj.value(QStringLiteral("opacity")).toDouble(1));
            rotation_->setValue(obj.value(QStringLiteral("rotationRadians")).toDouble() * (180.0 / 3.14159265358979323846));
            gain_->setValue(obj.value(QStringLiteral("gain")).toDouble(0.35));
            band_->setCurrentIndex(std::clamp(obj.value(QStringLiteral("audioBand")).toInt(1), 0, 3));
            const QString path = obj.value(QStringLiteral("path")).toString();
            if (path != previewPath_) {
                previewPath_ = path;
                // Preview one selected image only. Decode it close to the
                // display size instead of repeatedly loading the full 4K PNG.
                QImageReader reader(path);
                reader.setAutoTransform(true);
                const QSize original = reader.size();
                if (original.isValid())
                    reader.setScaledSize(original.scaled(QSize(160, 100), Qt::KeepAspectRatio));
                const QImage small = reader.read();
                preview_->setPixmap(small.isNull() ? QPixmap() : QPixmap::fromImage(small));
                preview_->setToolTip(path);
            }
        } else {
            previewPath_.clear();
            preview_->clear();
        }
        syncing_ = false;
        updateSelectionEnabled();
    }

    void applyChanges() {
        if (syncing_ || list_->currentRow() < 0 || !connected_) return;
        setStatus(tr("Updating live sprite..."));
        dispatch(QStringLiteral("sprite.update"), {
            {QStringLiteral("index"), list_->currentRow()},
            {QStringLiteral("x"), x_->value()},
            {QStringLiteral("y"), y_->value()},
            {QStringLiteral("scale"), scale_->value()},
            {QStringLiteral("opacity"), opacity_->value()},
            {QStringLiteral("rotationRadians"), rotation_->value() * (3.14159265358979323846 / 180.0)},
            {QStringLiteral("gain"), gain_->value()},
            {QStringLiteral("audioBand"), band_->currentIndex()}
        });
    }

    Dispatch dispatch_;
    QJsonArray data_;
    QString previewPath_;
    bool connected_ = false;
    bool syncing_ = false;
    QListWidget* list_ = nullptr;
    QListWidget* library_ = nullptr;
    QDoubleSpinBox *x_ = nullptr, *y_ = nullptr, *scale_ = nullptr,
                   *opacity_ = nullptr, *rotation_ = nullptr, *gain_ = nullptr;
    QComboBox* band_ = nullptr;
    QLabel *preview_ = nullptr, *status_ = nullptr;
    QPushButton *add_ = nullptr, *remove_ = nullptr, *refresh_ = nullptr, *apply_ = nullptr;
};
