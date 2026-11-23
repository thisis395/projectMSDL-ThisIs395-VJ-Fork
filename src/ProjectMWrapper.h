#pragma once

#include "notifications/AudioDataAvailableNotification.h"
#include "notifications/PlaybackControlNotification.h"

#include <projectM-4/projectM.h>
#include <projectM-4/playlist.h>

#include <Poco/Logger.h>
#include <Poco/NObserver.h>

#include <Poco/Util/AbstractConfiguration.h>
#include <Poco/Util/Subsystem.h>

#include <memory>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <vector>

class ProjectMWrapper : public Poco::Util::Subsystem
{
public:
    const char* name() const override;

    void initialize(Poco::Util::Application& app) override;

    void uninitialize() override;

    /**
     * Returns the projectM instance handle.
     * @return The projectM instance handle used to call API functions.
     */
    projectm_handle ProjectM() const;

    /**
     * Returns the playlist handle.
     * @return The plaslist handle.
     */
    projectm_playlist_handle Playlist() const;

    /** Add the directory containing a selected PNG to projectM's texture search paths. */
    void AddSpriteTextureDirectory(const std::string& directory);

    // Pre-decode imported PNGs on the renderer thread, rather than depending
    // on projectM's global texture search path and placeholder fallback.
    bool PrepareSpriteTexture(const std::string& path, std::string& textureName, std::string& error);
    bool SpriteTextureDelivered(const std::string& textureName) const;
    void ReleaseSpriteTexture(const std::string& textureName);

    /**
     * Renders a single projectM frame.
     */
    void RenderFrame();

    /**
     * @brief Returns the targeted FPS value.
     * @return The user-configured target FPS. Can be 0, which means unlimited.
     */
    int TargetFPS();

    /**
     * @brief Updates projectM with the current, actual FPS value.
     * @param fps The current FPS value.
     */
    void UpdateRealFPS(float fps);

    /**
     * @brief If splash is disabled, shows the initial preset.
     * If shuffle is on, a random preset will be picked. Otherwise, the first playlist item is displayed.
     */
    void DisplayInitialPreset();

    /**
     * @brief Changes beat sensitivity by the given value.
     * @param value A positive or negative delta value.
     */
    void ChangeBeatSensitivity(float value);

    /**
     * @brief Returns the libprojectM version this application was built against.
     * @return A string with the libprojectM build version.
     */
    std::string ProjectMBuildVersion();

    /**
     * @brief Returns the libprojectM version this applications currently runs with.
     * @return A string with the libprojectM runtime library version.
     */
    std::string ProjectMRuntimeVersion();

    /**
     * Copies the full path of the current preset into the OS clipboard.
     */
    void PresetFileNameToClipboard() const;

private:
    /**
     * @brief projectM callback. Called whenever a preset is switched.
     * @param isHardCut True if the switch was a hard cut.
     * @param index New preset playlist index.
     * @param context Callback context, e.g. "this" pointer.
     */
    static void PresetSwitchedEvent(bool isHardCut, unsigned int index, void* context);

    void PlaybackControlNotificationHandler(const Poco::AutoPtr<PlaybackControlNotification>& notification);

    void AudioDataAvailableNotificationHandler(const Poco::AutoPtr<AudioDataAvailableNotification>& notification);

    std::vector<std::string> GetPathListWithDefault(const std::string& baseKey, const std::string& defaultPath);

    /**
     * @brief Event callback if a configuration value has changed.
     * @param property The key and value that has been changed.
     */
    void OnConfigurationPropertyChanged(const Poco::Util::AbstractConfiguration::KeyValue& property);

    /**
     * @brief Event callback if a configuration value has been removed.
     * @param key The key of the removed property.
     */
    void OnConfigurationPropertyRemoved(const std::string& key);

    Poco::AutoPtr<Poco::Util::AbstractConfiguration> _userConfig; //!< View of the "projectM" configuration subkey in the "user" configuration.
    Poco::AutoPtr<Poco::Util::AbstractConfiguration> _projectMConfigView; //!< View of the "projectM" configuration subkey in the "effective" configuration.

    std::vector<std::string> _textureSearchPaths;
    struct SpriteTextureData
    {
        std::vector<unsigned char> rgba;
        unsigned int width{0};
        unsigned int height{0};
        bool delivered{false};
    };
    std::unordered_map<std::string, SpriteTextureData> _spriteTextureData;
    std::uint64_t _nextSpriteTextureId{0};

    projectm_handle _projectM{nullptr}; //!< Pointer to the projectM instance used by the application.
    projectm_playlist_handle _playlist{nullptr}; //!< Pointer to the projectM playlist manager instance.

    uint32_t _audioChannels{0}; //!< Number of audio channels of the current capture device.
    std::vector<float> _audioStagingBuffer; //!< Buffer which receives audio data from the capture implementation.
    mutable Poco::Mutex _audioBufferMutex; //!< Mutex protecting access to the audio staging buffer.

    Poco::NObserver<ProjectMWrapper, PlaybackControlNotification> _playbackControlNotificationObserver{*this, &ProjectMWrapper::PlaybackControlNotificationHandler};
    Poco::NObserver<ProjectMWrapper, AudioDataAvailableNotification> _audioDataAvailableNotificationObserver{*this, &ProjectMWrapper::AudioDataAvailableNotificationHandler};

    Poco::Logger& _logger{Poco::Logger::get("SDLRenderingWindow")}; //!< The class logger.
};
