#pragma once

#include <QAbstractSpinBox>
#include <QComboBox>
#include <QDoubleSpinBox>
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
        remove_ = new QPushButton(tr("Remove"));
        refresh_ = new QPushButton(tr("Refresh"));
        actions->addWidget(add_);
        actions->addWidget(remove_);
        actions->addWidget(refresh_);
        root->addLayout(actions);

        list_ = new QListWidget;
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
        opacity_ = addSlider(form, tr("Opacity"), 0, 1, 1, 0.01, 3);
        rotation_ = addSlider(form, tr("Rotation (degrees)"), -180, 180, 0, 1, 1);
        gain_ = addSlider(form, tr("Audio gain"), 0, 3, 0.35, 0.05, 3);
        band_ = new QComboBox;
        band_->addItems({tr("Off"), tr("Bass (smoothed)"),
                         tr("Mids (smoothed)"), tr("Treble (smoothed)")});
        form->addRow(tr("Audio band"), band_);
        contentLayout->addLayout(form);
        apply_ = new QPushButton(tr("Apply to live visualizer"));
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

        connect(add_, &QPushButton::clicked, this, [this] {
            const QString path = QFileDialog::getOpenFileName(
                this, tr("Add transparent PNG sprite"), QString(), tr("PNG images (*.png)"));
            if (path.isEmpty()) return;
            if (list_->count() >= 16) {
                setStatus(tr("Only 16 live sprites are supported."));
                return;
            }
            dispatch(QStringLiteral("sprite.add"), {{QStringLiteral("path"), QFileInfo(path).absoluteFilePath()}});
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
        connect(band_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int) { if (!syncing_) applyChanges(); });
    }

    void setDispatch(Dispatch callback) { dispatch_ = std::move(callback); }

    void setConnected(bool value) {
        connected_ = value;
        add_->setEnabled(value);
        refresh_->setEnabled(value);
        updateSelectionEnabled();
    }

    void setStatus(const QString& message) { status_->setText(message); }

    void setState(const QJsonArray& items) {
        QString previousPath;
        if (auto* selected = list_->currentItem())
            previousPath = selected->data(Qt::UserRole).toString();
        const int previousRow = list_->currentRow();
        data_ = items;
        syncing_ = true;
        {
            QSignalBlocker blocker(list_);
            list_->clear();
            for (const auto& value : data_) {
                const QString path = value.toObject().value(QStringLiteral("path")).toString();
                auto* item = new QListWidgetItem(QFileInfo(path).fileName());
                item->setData(Qt::UserRole, path);
                item->setToolTip(path);
                list_->addItem(item);
            }
            int target = -1;
            for (int i = 0; i < list_->count(); ++i) {
                if (list_->item(i)->data(Qt::UserRole).toString() == previousPath) {
                    target = i;
                    break;
                }
            }
            if (target < 0 && list_->count() > 0)
                target = std::min(previousRow >= 0 ? previousRow : 0, list_->count() - 1);
            list_->setCurrentRow(target);
        }
        syncing_ = false;
        loadSelection(list_->currentRow());
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
        if (!connected_ || !dispatch_) {
            setStatus(tr("Start the visualizer first."));
            return;
        }
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
            const QPixmap pixmap(path);
            preview_->setPixmap(pixmap.isNull()
                ? QPixmap()
                : pixmap.scaled(160, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            preview_->setToolTip(path);
        } else {
            preview_->clear();
        }
        syncing_ = false;
        updateSelectionEnabled();
    }

    void applyChanges() {
        if (syncing_ || list_->currentRow() < 0 || !connected_) return;
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
    bool connected_ = false;
    bool syncing_ = false;
    QListWidget* list_ = nullptr;
    QDoubleSpinBox *x_ = nullptr, *y_ = nullptr, *scale_ = nullptr,
                   *opacity_ = nullptr, *rotation_ = nullptr, *gain_ = nullptr;
    QComboBox* band_ = nullptr;
    QLabel *preview_ = nullptr, *status_ = nullptr;
    QPushButton *add_ = nullptr, *remove_ = nullptr, *refresh_ = nullptr, *apply_ = nullptr;
};
