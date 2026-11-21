#pragma once

#include "FileChooser.h"

#include <cstdint>
#include <string>
#include <vector>

class ProjectMWrapper;

/**
 * ThisIs395 VJ: manage PNG user sprites rendered by libprojectM.
 * The sprite's MilkDrop per-frame expressions use projectM's analyzed audio,
 * so this does not require a second FFT implementation.
 */
class SpriteManagerWindow
{
public:
    explicit SpriteManagerWindow(ProjectMWrapper& projectMWrapper);

    void Show();
    void Draw();

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

    void AddFile(const std::string& path);
    bool CreateSprite(Sprite& sprite);
    void ApplyVariables(const Sprite& sprite);
    void RemoveSprite(size_t index);

    ProjectMWrapper& _projectMWrapper;
    FileChooser _fileChooser{FileChooser::Mode::File};
    std::vector<Sprite> _sprites;
    std::string _status;
    bool _visible{false};
};
