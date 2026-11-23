#include "../NativeSpriteManager.h"

#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char* argv[])
{
#if defined(Q_OS_LINUX)
    qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
    QApplication app(argc, argv);
    NativeSpriteManager manager;
    struct Request { QString action; QJsonObject args; };
    std::vector<Request> requests;
    manager.setDispatch([&](const QString& action, const QJsonObject& args) {
        requests.push_back({action, args});
    });

    const auto button = [&](const QString& title) -> QPushButton* {
        for (auto* child : manager.findChildren<QPushButton*>()) {
            if (child->text() == title)
                return child;
        }
        throw std::runtime_error("Required button missing: " + title.toStdString());
    };
    const auto require = [](bool condition, const std::string& why) {
        if (!condition) throw std::runtime_error(why);
    };

    auto* add = button(QStringLiteral("Add PNG..."));
    auto* refresh = button(QStringLiteral("Refresh"));
    auto* remove = button(QStringLiteral("Remove"));
    auto* apply = button(QStringLiteral("Apply to live visualizer"));

    require(add->isEnabled(), "Add PNG must be usable before renderer startup");
    require(refresh->isEnabled(), "Refresh must be usable before renderer startup");
    require(!remove->isEnabled() && !apply->isEnabled(),
            "Remove and Apply must wait for sprite selection");

    refresh->click();
    require(requests.size() == 1 && requests.back().action == QStringLiteral("sprite.list"),
            "Refresh button did not dispatch sprite.list");
    manager.setConnected(true);

    QJsonObject sample{
        {QStringLiteral("path"), QStringLiteral("/tmp/sample_sprite.png")},
        {QStringLiteral("x"), 0.5}, {QStringLiteral("y"), 0.5},
        {QStringLiteral("scale"), 0.25}, {QStringLiteral("opacity"), 1.0},
        {QStringLiteral("rotationRadians"), 0.0}, {QStringLiteral("gain"), 0.35},
        {QStringLiteral("audioBand"), 1}};
    manager.setState(QJsonArray{sample});

    auto* list = manager.findChild<QListWidget*>();
    require(list && list->count() == 1 && list->currentRow() == 0,
            "Live response did not select loaded sprite");
    require(remove->isEnabled() && apply->isEnabled(),
            "Sprite selection did not enable Remove / Apply");

    auto spins = manager.findChildren<QDoubleSpinBox*>();
    require(spins.size() == 6, "Native scale / position / opacity / gain controls absent");
    // Regression: the previous implementation cleared and rebuilt the
    // entire list after each renderer ack, overwriting active text fields.
    require(!spins[2]->keyboardTracking(),
            "Scale input must not rewrite partial keystrokes from slider feedback");
    auto* firstItem = list->item(0);
    spins[2]->setValue(0.6); // scale is the third numeric control
    auto* edit = spins[2]->findChild<QLineEdit*>();
    require(edit != nullptr, "Scale text editor unavailable");
    edit->setText(QStringLiteral("1.234"));
    for (int i = 0; i < 40; ++i) {
        auto duplicate = sample;
        duplicate.insert(QStringLiteral("scale"), 0.25 + i / 1000.0);
        manager.setState(QJsonArray{duplicate});
        require(list->item(0) == firstItem,
                "Incoming sprite state unnecessarily rebuilt the entire list");
        require(edit->text() == QStringLiteral("1.234"),
                "Incoming state destroyed partially typed numeric input");
    }
    spins[2]->setValue(0.6);
    apply->click();
    require(!requests.empty() && requests.back().action == QStringLiteral("sprite.update"),
            "Apply button did not dispatch sprite.update");
    require(std::abs(requests.back().args.value(QStringLiteral("scale")).toDouble() - 0.6) < 1e-6,
            "Scale control was not included in sprite update");

    remove->click();
    require(!requests.empty() && requests.back().action == QStringLiteral("sprite.remove") &&
                requests.back().args.value(QStringLiteral("index")).toInt(-1) == 0,
            "Remove button did not dispatch selected index");
    manager.setState(QJsonArray{});
    require(!remove->isEnabled() && !apply->isEnabled(),
            "Empty sprite list should disable edit/remove actions");

    manager.setConnected(false);
    require(add->isEnabled() && refresh->isEnabled(),
            "Offline state should still allow importing and refreshing");
    std::cout << "PASS: native sprite buttons, 40 repeated snapshots, typing preservation and scale dispatch\n";
    return 0;
}
