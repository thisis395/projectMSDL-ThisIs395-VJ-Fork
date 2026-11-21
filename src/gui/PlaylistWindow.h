#pragma once

#include <cstddef>
#include <cstdint>

class ProjectMWrapper;

/** Live playlist browser and editing UI. */
class PlaylistWindow
{
public:
    PlaylistWindow();
    void Show();
    void Draw();

private:
    void DrawPlaylistTab();
    void DrawExportTab();

    ProjectMWrapper& _projectMWrapper;
    bool _visible{false};
    int _selected{-1};
    bool _hardCut{false};
    char _presetPath[2048]{};
    char _status[256]{};
};
