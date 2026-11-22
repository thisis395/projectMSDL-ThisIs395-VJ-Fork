#pragma once

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include <SDL2/SDL.h>

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
    void DrawRenderTab();
    void DrawRenderTimeline();
    void SaveRenderJob();
    void DrawSequenceTab();
    void DrawSequenceClips(bool sprites);
    void UpdateSequence();

    struct SequenceClip
    {
        std::string path;
        float duration{30.0f};
        float fade{3.0f};
        bool sprite{false};
    };

    ProjectMWrapper& _projectMWrapper;
    bool _visible{false};
    int _selected{-1};
    bool _hardCut{false};
    char _presetPath[2048]{};
    char _status[256]{};
    char _spritePath[2048]{};
    char _exportPath[2048]{};
    std::vector<SequenceClip> _sequence;
    std::set<std::string> _favorites;
    float _defaultDuration{30.0f};
    float _defaultFade{3.0f};
    std::size_t _sequencePosition{0};
    uint64_t _sequenceStarted{0};
    bool _sequencePlaying{false};
    int _renderSelected{-1};
    double _renderPlayhead{0.0};
    float _renderPixelsPerSecond{14.0f};
    int _renderWidth{1920};
    int _renderHeight{1080};
    int _renderFps{60};
    int _renderCodec{0};
    int _renderBitrateMbps{18};
    char _renderJobPath[2048]{};
    char _renderVideoPath[2048]{};
    char _renderAudioPath[2048]{};
    char _renderStatus[256]{};
};
