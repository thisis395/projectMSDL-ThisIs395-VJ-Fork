#pragma once

#include "FileChooser.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class ProjectMWrapper;
class ProjectMGUI;

/**
 * ThisIs395 VJ: manage PNG user sprites rendered by libprojectM.
 * The sprite's MilkDrop per-frame expressions use projectM's analyzed audio,
 * so this does not require a second FFT implementation.
 */
class SpriteManagerWindow
{
public:
    explicit SpriteManagerWindow(ProjectMGUI& gui);

    void Show();
    void Draw();

    // Native Qt editor and legacy ImGui manager share this one live sprite list.
    // Call these only from the SDL render thread (NativeControlMailbox::poll).
    struct Controls {
        std::string path;
        float x{0.5f};
        float y{0.5f};
        float scale{0.25f};
        float opacity{1.0f};
        float rotation{0.0f}; // radians; Qt presents degrees
        float gain{0.35f};
        int audioBand{1};
    };
    bool AddFile(const std::string& path);
    bool UpdateManagedSprite(std::size_t index, const Controls& controls);
    bool RemoveManagedSprite(std::size_t index);
    std::vector<Controls> Snapshot() const;
    const std::string& Status() const { return _status; }

private:
    struct Sprite
    {
        uint32_t id{0};
        std::string path;
        std::string textureName;
        float x{0.5f};
        float y{0.5f};
        float scale{0.25f};
        float opacity{1.0f};
        float rotation{0.0f};
        float gain{0.35f};
        int audioBand{1}; // 0: off; 1: bass; 2: mids; 3: treble
    };

    bool CreateSprite(Sprite& sprite);
    void ApplyVariables(const Sprite& sprite);
    void RemoveSprite(std::size_t index);

    ProjectMWrapper& _projectMWrapper;
    FileChooser _fileChooser{FileChooser::Mode::File};
    std::vector<Sprite> _sprites;
    std::string _status;
    bool _visible{false};
};
