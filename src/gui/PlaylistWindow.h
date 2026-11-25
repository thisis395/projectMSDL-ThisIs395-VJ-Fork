#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
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
    void DrawFavoritesTab();
    void RefreshFavorites();
    bool IsFavorite(const std::string& path, bool sprite);
    void ToggleFavorite(const std::string& path, bool sprite);
    void DrawSpriteLibraryTab();
    void ImportSpriteLibraryPath(const std::string& path, bool directory);
    void DrawExportTab();
    void DrawRenderTab();
    void DrawRenderTimeline();
    void SaveRenderJob(bool replaceExisting = false);
    void PollRenderStatus();
    void SetupRenderPrerequisites();
    void LaunchOfflineRender();
    void DrawSequenceTab();
    void DrawSequenceClips(bool sprites);
    void UpdateSequence();
    bool CueSequencePreset(std::size_t index);

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
    int _selectedSprite{-1};
    std::vector<std::string> _spriteLibrary;
    std::vector<std::string> _savedPresets, _savedSprites;
    std::map<std::pair<bool, std::string>, bool> _favoriteCache;
    char _spriteLibraryPath[2048]{};
    char _spriteFolderPath[2048]{};
    bool _hardCut{false};
    char _presetPath[2048]{};
    char _status[256]{};
    char _spritePath[2048]{};
    char _exportPath[2048]{};
    std::vector<SequenceClip> _sequence;
    float _defaultDuration{30.0f};
    float _defaultFade{3.0f};
    std::size_t _sequencePosition{0};
    uint64_t _sequenceStarted{0};
    bool _sequencePlaying{false};
    bool _sequenceLocked{false};
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
    float _renderAudioStart{0.0f};
    float _renderAudioDuration{180.0f};
    float _renderAudioGain{1.0f};
    char _renderStatus[256]{};
    float _renderProgress{0.0f};
    bool _renderRunning{false};
    uint64_t _renderLastPoll{0};
};
