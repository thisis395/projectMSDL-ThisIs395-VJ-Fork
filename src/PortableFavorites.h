#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace PortableFavorites {
namespace fs = std::filesystem;
using Paths = std::vector<fs::path>;

// Keep user data outside a signed macOS app bundle, but beside the executable
// for flat Windows/Linux distributions. Never depend on the working directory.
inline fs::path rootForExecutable(const fs::path& executable) {
    auto dir = fs::absolute(executable).parent_path();
    if (dir.filename() == "MacOS" && dir.parent_path().filename() == "Contents" &&
        dir.parent_path().parent_path().extension() == ".app")
        dir = dir.parent_path().parent_path().parent_path();
    return dir / "Favorites";
}

inline fs::path folder(const fs::path& root, bool sprite) {
    return root / (sprite ? "Sprites" : "Presets");
}

inline bool matchesType(const fs::path& path, bool sprite) {
    auto ext = path.extension().u8string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == (sprite ? ".png" : ".milk");
}

inline void initialize(const fs::path& root) {
    if (fs::is_symlink(root) || fs::is_symlink(folder(root, false)) ||
        fs::is_symlink(folder(root, true)))
        throw std::runtime_error("Favorite folders cannot be symbolic links.");
    fs::create_directories(folder(root, false));
    fs::create_directories(folder(root, true));
}

inline Paths list(const fs::path& root, bool sprite) {
    Paths result;
    const auto dir = folder(root, sprite);
    if (fs::is_symlink(root) || fs::is_symlink(dir))
        throw std::runtime_error("Favorite folders cannot be symbolic links.");
    if (!fs::exists(dir)) return result;
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        // Do not follow links or treat links to originals as owned copies.
        if (!entry.is_symlink() && entry.is_regular_file() && matchesType(entry.path(), sprite))
            result.push_back(entry.path());
    }
    std::sort(result.begin(), result.end());
    return result;
}

inline bool sameAsset(const fs::path& a, const fs::path& b) {
    std::error_code ec;
    if (fs::equivalent(a, b, ec) && !ec) return true;
    if (a.filename() != b.filename()) return false;
    const auto sizeA = fs::file_size(a, ec);
    if (ec) return false;
    const auto sizeB = fs::file_size(b, ec);
    if (ec || sizeA != sizeB) return false;
    std::ifstream left(a, std::ios::binary), right(b, std::ios::binary);
    if (!left || !right) return false;
    char x[16384], y[16384];
    do {
        left.read(x, sizeof x);
        right.read(y, sizeof y);
        if (left.gcount() != right.gcount() ||
            !std::equal(x, x + left.gcount(), y)) return false;
    } while (left.gcount() != 0 && left && right);
    return left.eof() && right.eof() && !left.bad() && !right.bad();
}

inline fs::path find(const Paths& saved, const fs::path& source) {
    const auto exact = std::find(saved.begin(), saved.end(), source);
    if (exact != saved.end()) return *exact;
    for (const auto& copy : saved)
        if (copy.filename() == source.filename() && sameAsset(copy, source)) return copy;
    return {};
}

inline fs::path add(const fs::path& root, bool sprite, const fs::path& source) {
    if (!fs::is_regular_file(source) || !matchesType(source, sprite))
        throw std::runtime_error("Select a readable .milk preset or PNG sprite.");
    initialize(root);
    auto existing = find(list(root, sprite), source);
    if (!existing.empty()) return existing;
    const auto dir = folder(root, sprite);
    auto destination = dir / source.filename();
    // Preserve the original filename, including texture/preset naming conventions.
    // A collision gets a numbered subdirectory instead of overwriting an asset.
    if (fs::exists(destination)) {
        for (unsigned n = 2;; ++n) {
            auto sub = dir / std::to_string(n);
            if (fs::create_directory(sub)) {
                destination = sub / source.filename();
                break;
            }
            if (!fs::exists(sub)) throw std::runtime_error("Cannot create favorite folder.");
        }
    }
    // copy_file without overwrite protects existing favorites.
    if (!fs::copy_file(source, destination))
        throw std::runtime_error("Could not copy favorite.");
    return destination;
}

inline void remove(const fs::path& root, bool sprite, const fs::path& copy) {
    // Only remove a file returned by our library scan, never an external source.
    const auto saved = list(root, sprite);
    if (std::find(saved.begin(), saved.end(), copy) == saved.end())
        throw std::runtime_error("Favorite no longer exists in the portable folder.");
    if (!fs::remove(copy)) throw std::runtime_error("Could not remove favorite.");
}
} // namespace PortableFavorites
