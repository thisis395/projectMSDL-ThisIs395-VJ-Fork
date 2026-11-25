#include "../src/PortableFavorites.h"
#include <chrono>
#include <iostream>

namespace pf = PortableFavorites;
namespace fs = pf::fs;

static void require(bool test, const char* message) {
    if (!test) throw std::runtime_error(message);
}
static void write(const fs::path& path, const char* data) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << data;
    if (!file) throw std::runtime_error("Could not write fixture.");
}
int main() {
    const auto temp = fs::temp_directory_path() / ("vj-favorites-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        const auto executable = temp / "suite" / "ThisIs395-VJ";
        const auto root = pf::rootForExecutable(executable);
        require(root == temp / "suite" / "Favorites", "Wrong flat root");
        require(pf::rootForExecutable(temp / "suite" / "projectM.app" / "Contents" /
                "MacOS" / "projectM") == root, "macOS favorites must be outside bundle");
        pf::initialize(root);
        const auto a = temp / "originals" / "first" / "same.milk";
        const auto b = temp / "originals" / "second" / "same.milk";
        const auto png = temp / "originals" / "sprite.png";
        write(a, "preset A");
        write(b, "preset B");
        write(png, "sprite bytes");
        const auto copyA = pf::add(root, false, a);
        require(pf::add(root, false, a) == copyA, "Duplicate star created another copy");
        const auto copyB = pf::add(root, false, b);
        require(copyA != copyB && copyA.filename() == copyB.filename(), "Collision lost a file");
        const auto copyPng = pf::add(root, true, png);
        require(pf::list(root, false).size() == 2 && pf::list(root, true).size() == 1,
                "Favorites not restored from disk");
        require(pf::find(pf::list(root, false), a) == copyA, "Star not restored");
        bool rejected = false;
        try { pf::remove(root, false, a); } catch (const std::exception&) { rejected = true; }
        require(rejected && fs::exists(a), "External original was not protected");
        rejected = false;
        try { pf::add(root, false, png); } catch (const std::exception&) { rejected = true; }
        require(rejected, "Wrong asset type was accepted");
        fs::create_directories(temp / "moved");
        fs::rename(root, temp / "moved" / "Favorites");
        fs::remove_all(temp / "originals");
        const auto moved = pf::rootForExecutable(temp / "moved" / "ThisIs395-VJ");
        auto presets = pf::list(moved, false);
        auto sprites = pf::list(moved, true);
        require(presets.size() == 2 && sprites.size() == 1, "Moved copies not found");
        require(fs::file_size(sprites.front()) == 12, "Moved sprite content lost");
        pf::remove(moved, false, presets.front());
        require(pf::list(moved, false).size() == 1, "Unstar did not persist");
        fs::remove_all(temp);
        std::cout << "Portable favorites regression checks passed.\n";
    } catch (const std::exception& e) {
        fs::remove_all(temp);
        std::cerr << e.what() << '\n';
        return 1;
    }
}
