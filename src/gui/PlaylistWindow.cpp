#include "PlaylistWindow.h"

#include "ProjectMWrapper.h"
#include "imgui.h"

#include <Poco/Util/Application.h>
#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/DirectoryIterator.h>
#include <Poco/String.h>
#include <Poco/Process.h>
#include <Poco/Exception.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <sstream>
#include <string>

PlaylistWindow::PlaylistWindow()
    : _projectMWrapper(Poco::Util::Application::instance().getSubsystem<ProjectMWrapper>())
{
}

void PlaylistWindow::Show()
{
    _visible = true;
}

void PlaylistWindow::Draw()
{
    if (!_visible)
    {
        return;
    }

    UpdateSequence();
    ImGui::SetNextWindowSize(ImVec2(1240, 780), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("ThisIs395 VJ - Playlist", &_visible))
    {
        if (ImGui::BeginTabBar("PlaylistTabs"))
        {
            if (ImGui::BeginTabItem("Playlist"))
            {
                if (ImGui::BeginTabBar("LibraryTypes"))
                {
                    if (ImGui::BeginTabItem("Presets"))
                    {
                        DrawPlaylistTab();
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Sprites"))
                    {
                        DrawSpriteLibraryTab();
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Sequence"))
            {
                DrawSequenceTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Render"))
            {
                DrawRenderTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Export"))
            {
                DrawExportTab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void PlaylistWindow::DrawPlaylistTab()
{
    auto* playlist = _projectMWrapper.Playlist();
    if (!playlist)
    {
        ImGui::TextUnformatted("The projectM playlist is not available.");
        return;
    }

    const uint32_t count = projectm_playlist_size(playlist);
    const uint32_t current = projectm_playlist_get_position(playlist);
    if (_selected >= 0 && static_cast<uint32_t>(_selected) >= count)
    {
        _selected = -1;
    }

    ImGui::Text("%u presets", count);
    ImGui::SameLine();
    bool shuffle = projectm_playlist_get_shuffle(playlist);
    if (ImGui::Checkbox("Shuffle", &shuffle))
    {
        projectm_playlist_set_shuffle(playlist, shuffle);
        // The application's configuration handles persistent shuffle settings.
    }
    ImGui::SameLine();
    ImGui::Checkbox("Instant transitions", &_hardCut);

    ImGui::Separator();
    ImGui::BeginChild("PresetList", ImVec2(0, -135), true);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(std::min<uint32_t>(count, 2147483647u)));
    while (clipper.Step())
    {
        for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
        {
            char* filename = projectm_playlist_item(playlist, static_cast<uint32_t>(index));
            const std::string path = filename ? filename : "";
            if (filename)
            {
                projectm_playlist_free_string(filename);
            }

            auto lastSlash = path.find_last_of("/\\");
            const std::string name = lastSlash == std::string::npos ? path : path.substr(lastSlash + 1);
            const std::string label = (static_cast<uint32_t>(index) == current ? "▶ " : "   ")
                + name + "##preset_" + std::to_string(index);

            if (ImGui::Selectable(label.c_str(), _selected == index, ImGuiSelectableFlags_AllowDoubleClick))
            {
                _selected = index;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    projectm_playlist_set_position(playlist, static_cast<uint32_t>(index), _hardCut);
                }
            }
            ImGui::SameLine();
            ImGui::PushID(index);
            if (ImGui::SmallButton(_favorites.count(path) ? "*" : "Star"))
            {
                if (!_favorites.erase(path)) _favorites.insert(path);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("+"))
            {
                SequenceClip clip;
                clip.path = path;
                clip.sprite = false;
                clip.duration = _defaultDuration;
                clip.fade = _defaultFade;
                _sequence.push_back(clip);
                std::snprintf(_status, sizeof(_status), "Added preset to sequence.");
            }
            ImGui::PopID();
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", path.c_str());
            }
        }
    }
    ImGui::EndChild();

    if (ImGui::Button("Play selected") && _selected >= 0)
    {
        projectm_playlist_set_position(playlist, static_cast<uint32_t>(_selected), _hardCut);
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove selected") && _selected >= 0)
    {
        if (projectm_playlist_remove_preset(playlist, static_cast<uint32_t>(_selected)))
        {
            _selected = -1;
            std::snprintf(_status, sizeof(_status), "Preset removed from the current playlist.");
        }
    }

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##presetPath", "Enter full path to a .milk preset", _presetPath, sizeof(_presetPath));
    if (ImGui::Button("Add preset"))
    {
        std::string path(_presetPath);
        const auto first = path.find_first_not_of(" \t");
        if (first == std::string::npos)
        {
            std::snprintf(_status, sizeof(_status), "Enter a preset file path first.");
        }
        else
        {
            path = path.substr(first, path.find_last_not_of(" \t") - first + 1);
            if (projectm_playlist_add_preset(playlist, path.c_str(), false))
            {
                _presetPath[0] = '\0';
                std::snprintf(_status, sizeof(_status), "Added preset to the current playlist.");
            }
            else
            {
                std::snprintf(_status, sizeof(_status), "That preset is already in the playlist.");
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Changes are in-memory for this session.");
    if (_status[0])
    {
        ImGui::TextWrapped("%s", _status);
    }
}



void PlaylistWindow::ImportSpriteLibraryPath(const std::string& path, bool directory)
{
    // Library entries are references only; importing never creates a live projectM sprite.
    try
    {
        Poco::File input(path);
        if (!input.exists())
        {
            std::snprintf(_status, sizeof(_status), "Sprite path does not exist.");
            return;
        }
        std::size_t added = 0;
        const auto insertFile = [this, &added](const std::string& filename)
        {
            Poco::File file(filename);
            Poco::Path filePath(filename);
            if (!file.isFile() || !file.canRead() || Poco::icompare(filePath.getExtension(), "png") != 0)
                return;
            const std::string absolute = filePath.absolute().toString();
            if (std::find(_spriteLibrary.begin(), _spriteLibrary.end(), absolute) == _spriteLibrary.end())
            {
                _spriteLibrary.push_back(absolute);
                ++added;
            }
        };
        if (directory)
        {
            if (!input.isDirectory())
            {
                std::snprintf(_status, sizeof(_status), "Choose a sprite folder.");
                return;
            }
            // Intentionally non-recursive: a user can import subfolders separately.
            for (Poco::DirectoryIterator it(path), end; it != end; ++it)
                if (_spriteLibrary.size() < 10000)
                    insertFile(it.path().toString());
        }
        else
        {
            insertFile(path);
        }
        std::sort(_spriteLibrary.begin(), _spriteLibrary.end());
        std::snprintf(_status, sizeof(_status), "Imported %zu PNG sprite(s).", added);
    }
    catch (const Poco::Exception&)
    {
        std::snprintf(_status, sizeof(_status), "Unable to read the sprite file or folder.");
    }
}

void PlaylistWindow::DrawSpriteLibraryTab()
{
    ImGui::Text("%zu sprites in library", _spriteLibrary.size());
    ImGui::SameLine();
    ImGui::TextDisabled("Choose a PNG and press + to add it to Sequence.");
    ImGui::Separator();

    ImGui::BeginChild("SpriteLibraryList", ImVec2(0, -190), true);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(_spriteLibrary.size()));
    while (clipper.Step())
    {
        for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
        {
            const std::string path = _spriteLibrary[static_cast<std::size_t>(index)];
            const auto slash = path.find_last_of("/\\");
            const std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
            ImGui::PushID(index);
            if (ImGui::Selectable((name + "##sprite").c_str(), _selectedSprite == index))
                _selectedSprite = index;
            ImGui::SameLine();
            if (ImGui::SmallButton(_spriteFavorites.count(path) ? "*" : "Star"))
            {
                if (!_spriteFavorites.erase(path))
                    _spriteFavorites.insert(path);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("+"))
            {
                SequenceClip clip;
                clip.path = path;
                clip.sprite = true;
                clip.duration = _defaultDuration;
                clip.fade = _defaultFade;
                _sequence.push_back(clip);
                std::snprintf(_status, sizeof(_status), "Added sprite to sequence.");
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", path.c_str());
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    if (ImGui::Button("Add selected to sequence") &&
        _selectedSprite >= 0 && static_cast<std::size_t>(_selectedSprite) < _spriteLibrary.size())
    {
        SequenceClip clip;
        clip.path = _spriteLibrary[static_cast<std::size_t>(_selectedSprite)];
        clip.sprite = true;
        clip.duration = _defaultDuration;
        clip.fade = _defaultFade;
        _sequence.push_back(clip);
        std::snprintf(_status, sizeof(_status), "Added sprite to sequence.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove from library") &&
        _selectedSprite >= 0 && static_cast<std::size_t>(_selectedSprite) < _spriteLibrary.size())
    {
        _spriteFavorites.erase(_spriteLibrary[static_cast<std::size_t>(_selectedSprite)]);
        _spriteLibrary.erase(_spriteLibrary.begin() + _selectedSprite);
        _selectedSprite = -1;
    }

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##spriteLibraryPath", "Full path to a PNG sprite", _spriteLibraryPath, sizeof(_spriteLibraryPath));
    if (ImGui::Button("Add PNG to library") && _spriteLibraryPath[0])
    {
        ImportSpriteLibraryPath(_spriteLibraryPath, false);
        _spriteLibraryPath[0] = '\0';
    }
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##spriteFolderPath", "Full path to a folder of PNG sprites", _spriteFolderPath, sizeof(_spriteFolderPath));
    if (ImGui::Button("Import sprite folder") && _spriteFolderPath[0])
    {
        ImportSpriteLibraryPath(_spriteFolderPath, true);
        _spriteFolderPath[0] = '\0';
    }
    if (_status[0]) ImGui::TextWrapped("%s", _status);
    ImGui::TextDisabled("Sprite library is in-memory this session. Sequence sprite playback/rendering is a separate milestone.");
}

void PlaylistWindow::DrawSequenceTab()
{
    ImGui::TextUnformatted("Sequence editor");
    ImGui::SetNextItemWidth(190);
    ImGui::InputFloat("Default clip duration (seconds)", &_defaultDuration, 1.0f, 5.0f, "%.1f");
    _defaultDuration = std::max(0.1f, _defaultDuration);
    ImGui::SetNextItemWidth(190);
    ImGui::InputFloat("Default fade (seconds)", &_defaultFade, 0.5f, 1.0f, "%.1f");
    _defaultFade = std::max(0.0f, std::min(_defaultFade, 15.0f));
    ImGui::TextDisabled("Set fade to 0 for a hard cut.");
    if (ImGui::BeginTabBar("SequenceTypes"))
    {
        if (ImGui::BeginTabItem("Presets"))
        {
            DrawSequenceClips(false);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Sprites"))
        {
            ImGui::TextWrapped("Sprite sequence entries can be staged here. Live sprite playback "
                               "requires integration with the Sprite Manager from PR #2.");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##spriteInput", "Full path to PNG sprite", _spritePath, sizeof(_spritePath));
            if (ImGui::Button("+ Add sprite to sequence") && _spritePath[0] != '\0')
            {
                SequenceClip clip;
                clip.path = _spritePath;
                clip.sprite = true;
                clip.duration = _defaultDuration;
                clip.fade = _defaultFade;
                _sequence.push_back(clip);
                _spritePath[0] = '\0';
            }
            DrawSequenceClips(true);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::Separator();
    ImGui::Text("%zu clips across both tracks", _sequence.size());
    if (ImGui::Button("Previous"))
    {
        std::size_t position = _sequencePosition < _sequence.size() ? _sequencePosition : _sequence.size();
        while (position > 0)
        {
            --position;
            if (!_sequence[position].sprite)
            {
                CueSequencePreset(position);
                break;
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(_sequencePlaying ? "Restart" : "Play sequence"))
    {
        _sequencePlaying = true;
        _sequencePosition = 0;
        _sequenceStarted = 0;
        // The frame updater will cue the first preset.
    }
    ImGui::SameLine();
    if (ImGui::Button("Next"))
    {
        for (std::size_t index = _sequencePosition < _sequence.size() ? _sequencePosition + 1 : 0;
             index < _sequence.size(); ++index)
        {
            if (!_sequence[index].sprite)
            {
                CueSequencePreset(index);
                break;
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop"))
    {
        _sequencePlaying = false;
        _sequenceStarted = 0;
    }
    if (ImGui::Checkbox("Lock preset", &_sequenceLocked))
    {
        // A sequence lock pauses *automatic* advancement; manual Previous/Next still work.
        if (!_sequenceLocked && _sequencePlaying)
            _sequenceStarted = SDL_GetTicks64();
    }
    ImGui::SameLine();
    ImGui::TextDisabled(_sequenceLocked ? "Auto-advance paused" : "Auto-advance enabled");
    ImGui::TextDisabled("Preset playback is functional. Sprite clips are staged but not rendered.");
    ImGui::TextDisabled("Fades apply to incoming preset transitions.");
}

void PlaylistWindow::DrawSequenceClips(bool sprites)
{
    for (std::size_t i = 0; i < _sequence.size();)
    {
        SequenceClip& clip = _sequence[i];
        if (clip.sprite != sprites)
        {
            ++i;
            continue;
        }
        ImGui::PushID(static_cast<int>(i));
        const std::size_t slash = clip.path.find_last_of("/\\");
        const std::string name = slash == std::string::npos ? clip.path : clip.path.substr(slash + 1);
        ImGui::Text("%zu. %s", i + 1, name.c_str());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::InputFloat("seconds", &clip.duration, 1.0f, 5.0f, "%.1f");
        clip.duration = std::max(0.1f, clip.duration);
        if (!sprites)
        {
            ImGui::SetNextItemWidth(110);
            ImGui::InputFloat("Fade (s)", &clip.fade, 0.5f, 1.0f, "%.1f");
            clip.fade = std::max(0.0f, std::min(clip.fade, 15.0f));
        }
        ImGui::SameLine();
        bool removed = ImGui::SmallButton("Remove");
        if (!removed)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("Up") && i > 0)
            {
                std::swap(_sequence[i], _sequence[i - 1]);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Down") && i + 1 < _sequence.size())
            {
                std::swap(_sequence[i], _sequence[i + 1]);
            }
        }
        ImGui::PopID();
        if (removed)
        {
            _sequence.erase(_sequence.begin() + static_cast<std::ptrdiff_t>(i));
            _sequencePlaying = false;
            _sequenceStarted = 0;
        }
        else
        {
            ++i;
        }
    }
}

bool PlaylistWindow::CueSequencePreset(std::size_t index)
{
    if (index >= _sequence.size() || _sequence[index].sprite)
        return false;
    auto* playlist = _projectMWrapper.Playlist();
    if (!playlist)
        return false;

    const auto& clip = _sequence[index];
    const uint32_t count = projectm_playlist_size(playlist);
    for (uint32_t i = 0; i < count; ++i)
    {
        char* item = projectm_playlist_item(playlist, i);
        if (!item) continue;
        const bool matches = clip.path == item;
        projectm_playlist_free_string(item);
        if (matches)
        {
            const float fade = std::max(0.0f, clip.fade);
            projectm_set_soft_cut_duration(_projectMWrapper.ProjectM(), fade);
            projectm_playlist_set_position(playlist, i, _hardCut || fade <= 0.0f);
            _sequencePosition = index;
            _sequenceStarted = SDL_GetTicks64();
            // Manual next/previous only cues. Play sequence starts/resumes automation.
            return true;
        }
    }
    std::snprintf(_status, sizeof(_status), "A sequence preset is missing from the live playlist.");
    _sequencePlaying = false;
    return false;
}

void PlaylistWindow::UpdateSequence()
{
    if (!_sequencePlaying || _sequence.empty() || _sequenceLocked)
        return;

    const uint64_t now = SDL_GetTicks64();
    if (_sequenceStarted != 0 && _sequencePosition < _sequence.size() &&
        static_cast<double>(now - _sequenceStarted) <
            std::max(0.1f, _sequence[_sequencePosition].duration) * 1000.0)
        return;

    std::size_t next = _sequenceStarted == 0 ? _sequencePosition : _sequencePosition + 1;
    while (next < _sequence.size() && _sequence[next].sprite)
        ++next;
    if (next >= _sequence.size())
    {
        _sequencePlaying = false;
        _sequenceStarted = 0;
        return;
    }
    CueSequencePreset(next);
}

void PlaylistWindow::DrawExportTab()
{
    auto* playlist = _projectMWrapper.Playlist();
    ImGui::TextUnformatted("Export the current preset playlist");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##exportPath", "Output .m3u8 path (e.g. C:\\\\Users\\\\You\\\\playlist.m3u8)",
                             _exportPath, sizeof(_exportPath));
    if (ImGui::Button("Export current playlist (.m3u8)"))
    {
        std::string path(_exportPath);
        if (path.empty() || path.size() < 6 || path.substr(path.size() - 6) != ".m3u8")
        {
            std::snprintf(_status, sizeof(_status), "Enter an output path ending in .m3u8.");
        }
        else
        {
            // Do not truncate an existing user file without an explicit overwrite interaction.
            std::ifstream existing(path, std::ios::binary);
            if (existing.good())
            {
                std::snprintf(_status, sizeof(_status), "Output exists. Choose another filename.");
            }
            else
            {
                std::ofstream out(path, std::ios::binary);
                if (!out)
                {
                    std::snprintf(_status, sizeof(_status), "Could not create playlist file.");
                }
                else
                {
                    out << "#EXTM3U\n";
                    const uint32_t count = projectm_playlist_size(playlist);
                    for (uint32_t i = 0; i < count; ++i)
                    {
                        char* item = projectm_playlist_item(playlist, i);
                        if (!item) continue;
                        out << item << "\n";
                        projectm_playlist_free_string(item);
                    }
                    out.close();
                    std::snprintf(_status, sizeof(_status), out ? "Playlist exported." : "Write failed.");
                }
            }
        }
    }
    if (_status[0]) ImGui::TextWrapped("%s", _status);
    ImGui::Separator();
    ImGui::TextWrapped("This exports a UTF-8 M3U playlist of preset file paths, not an MP4. "
                       "Video rendering with synchronized audio and sprites is a later milestone.");
}


namespace
{
std::string RenderBaseName(const std::string& path)
{
    const auto slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string EscapeRenderJson(const std::string& input)
{
    std::ostringstream out;
    for (unsigned char character : input)
    {
        switch (character)
        {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (character < 0x20)
                {
                    constexpr char hex[] = "0123456789abcdef";
                    out << "\\u00" << hex[character >> 4] << hex[character & 0x0f];
                }
                else
                    out << static_cast<char>(character);
                break;
        }
    }
    return out.str();
}
}

void PlaylistWindow::DrawRenderTab()
{
    ImGui::TextUnformatted("Render workspace");
    ImGui::SameLine();
    ImGui::TextDisabled("Experimental FFmpeg export: local renderer + encoder required.");
    ImGui::Separator();

    const float available = ImGui::GetContentRegionAvail().x;
    const float inspectorWidth = std::max(310.0f, available * 0.34f);
    const float previewWidth = std::max(300.0f, available - inspectorWidth - 12.0f);
    ImGui::BeginChild("RenderPreview", ImVec2(previewWidth, 255.0f), true);
    ImGui::TextUnformatted("PROGRAM MONITOR");
    ImGui::Separator();
    ImGui::TextDisabled("Live projectM output stays in the main application window.");
    ImGui::TextDisabled("An independent offscreen preview is not available yet.");
    ImGui::Spacing();
    ImGui::Text("Playhead: %.2f s", _renderPlayhead);
    if (_renderSelected >= 0 && static_cast<std::size_t>(_renderSelected) < _sequence.size())
    {
        const auto& clip = _sequence[static_cast<std::size_t>(_renderSelected)];
        ImGui::TextWrapped("Selected: %s", RenderBaseName(clip.path).c_str());
        if (!clip.sprite && ImGui::Button("Cue selected preset in live output"))
        {
            auto* playlist = _projectMWrapper.Playlist();
            if (playlist)
            {
                bool found = false;
                for (uint32_t i = 0; i < projectm_playlist_size(playlist); ++i)
                {
                    char* item = projectm_playlist_item(playlist, i);
                    if (!item) continue;
                    const bool matches = clip.path == item;
                    projectm_playlist_free_string(item);
                    if (matches)
                    {
                        projectm_set_soft_cut_duration(_projectMWrapper.ProjectM(), clip.fade);
                        projectm_playlist_set_position(playlist, i, clip.fade <= 0.0f);
                        found = true;
                        break;
                    }
                }
                std::snprintf(_renderStatus, sizeof(_renderStatus), "%s",
                              found ? "Preset cued in live output." : "Preset not found in live playlist.");
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("RenderSettings", ImVec2(0, 255.0f), true);
    ImGui::TextUnformatted("OUTPUT SETTINGS");
    ImGui::Separator();
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Width", &_renderWidth, 0);
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Height", &_renderHeight, 0);
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("FPS", &_renderFps, 0);
    _renderWidth = std::max(16, std::min(_renderWidth, 7680));
    _renderHeight = std::max(16, std::min(_renderHeight, 4320));
    _renderFps = std::max(1, std::min(_renderFps, 240));
    const char* codecs[] = {"H.264", "H.265 / HEVC", "ProRes 422"};
    ImGui::SetNextItemWidth(155);
    ImGui::Combo("Codec", &_renderCodec, codecs, 3);
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Target Mbps", &_renderBitrateMbps, 0);
    _renderBitrateMbps = std::max(1, std::min(_renderBitrateMbps, 500));
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::TextUnformatted("TIMELINE");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    ImGui::SliderFloat("Zoom (px/s)", &_renderPixelsPerSecond, 4.0f, 80.0f, "%.0f");
    DrawRenderTimeline();

    ImGui::Separator();
    if (_renderSelected >= 0 && static_cast<std::size_t>(_renderSelected) < _sequence.size())
    {
        ImGui::PushID("RenderInspector");
        SequenceClip& clip = _sequence[static_cast<std::size_t>(_renderSelected)];
        ImGui::Text("CLIP INSPECTOR: %s", RenderBaseName(clip.path).c_str());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110);
        if (ImGui::InputFloat("Duration (s)", &clip.duration, 1.0f, 5.0f, "%.2f"))
            clip.duration = std::max(0.1f, clip.duration);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110);
        if (ImGui::InputFloat("Fade (s)", &clip.fade, 0.5f, 1.0f, "%.2f"))
            clip.fade = std::max(0.0f, std::min(clip.fade, clip.duration));
        if (clip.sprite)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("Sprite opacity automation not yet connected.");
        }
        ImGui::PopID();
    }
    else
    {
        ImGui::TextDisabled("Select a timeline clip to edit duration and fade.");
    }

    ImGui::Separator();
    ImGui::TextUnformatted("RENDER QUEUE / JOB");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##renderAudio", "Audio track source file path (not yet decoded)", _renderAudioPath, sizeof(_renderAudioPath));
    ImGui::SetNextItemWidth(125);
    ImGui::InputFloat("Audio start (s)", &_renderAudioStart, 1.0f, 5.0f, "%.2f");
    _renderAudioStart = std::max(0.0f, _renderAudioStart);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(125);
    ImGui::InputFloat("Audio duration (s)", &_renderAudioDuration, 1.0f, 10.0f, "%.2f");
    _renderAudioDuration = std::max(0.1f, _renderAudioDuration);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(125);
    ImGui::SliderFloat("Audio gain", &_renderAudioGain, 0.0f, 2.0f, "%.2f");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##renderVideo", "Planned output video path, e.g. show.mp4", _renderVideoPath, sizeof(_renderVideoPath));
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##renderJob", "Save render job path, e.g. show.render.json", _renderJobPath, sizeof(_renderJobPath));
    if (ImGui::Button("Save render job (.json)"))
        SaveRenderJob();
    ImGui::SameLine();
    ImGui::Separator();
    if (ImGui::Button("Install / repair render prerequisites"))
        SetupRenderPrerequisites();
    ImGui::SameLine();
    if (ImGui::Button("Render video with FFmpeg"))
        LaunchOfflineRender();
    ImGui::TextDisabled("Setup may install Python and FFmpeg via your OS package manager.");
    ImGui::TextDisabled("Render runs as a separate process; see its console for progress/errors.");
    if (_renderStatus[0])
        ImGui::TextWrapped("%s", _renderStatus);
}

void PlaylistWindow::DrawRenderTimeline()
{
    double presetTotal = 0.0, spriteTotal = 0.0;
    for (const auto& clip : _sequence)
    {
        if (clip.sprite) spriteTotal += std::max(0.1f, clip.duration);
        else presetTotal += std::max(0.1f, clip.duration);
    }
    const bool hasAudio = _renderAudioPath[0] != '\0';
    const double audioTotal = hasAudio ? static_cast<double>(_renderAudioStart + _renderAudioDuration) : 0.0;
    const double timelineTotal = std::max(std::max(presetTotal, spriteTotal), audioTotal);
    ImGui::Text("Presets: %.1fs   Sprites: %.1fs   Total: %.1fs",
                presetTotal, spriteTotal, timelineTotal);
    ImGui::BeginChild("RenderTimelineCanvas", ImVec2(0, 270), true, ImGuiWindowFlags_HorizontalScrollbar);
    const float labelWidth = 92.0f;
    const float scale = _renderPixelsPerSecond;
    const float canvasWidth = std::max(ImGui::GetContentRegionAvail().x,
                                      labelWidth + static_cast<float>(timelineTotal) * scale + 80.0f);
    ImGui::InvisibleButton("##timelineSurface", ImVec2(canvasWidth, 226.0f));
    const ImVec2 base = ImGui::GetItemRectMin();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float trackX = base.x + labelWidth;
    const float rulerY = base.y + 14.0f;
    const float presetY = base.y + 46.0f;
    const float spriteY = base.y + 106.0f;
    const float audioY = base.y + 166.0f;
    draw->AddText(ImVec2(base.x + 3, presetY + 12), IM_COL32(220, 220, 220, 255), "PRESETS");
    draw->AddText(ImVec2(base.x + 3, spriteY + 12), IM_COL32(220, 220, 220, 255), "SPRITES");
    draw->AddRectFilled(ImVec2(trackX, presetY), ImVec2(base.x + canvasWidth, presetY + 45),
                        IM_COL32(34, 46, 64, 255));
    draw->AddRectFilled(ImVec2(trackX, spriteY), ImVec2(base.x + canvasWidth, spriteY + 45),
                        IM_COL32(48, 37, 61, 255));
    draw->AddText(ImVec2(base.x + 3, audioY + 12), IM_COL32(220, 220, 220, 255), "AUDIO");
    draw->AddRectFilled(ImVec2(trackX, audioY), ImVec2(base.x + canvasWidth, audioY + 45), IM_COL32(34, 55, 47, 255));
    if (hasAudio)
    {
        const float a = trackX + _renderAudioStart * scale;
        const float b = a + _renderAudioDuration * scale;
        draw->AddRectFilled(ImVec2(a + 1, audioY + 3), ImVec2(b - 1, audioY + 42), IM_COL32(42, 130, 105, 255), 4.0f);
        draw->PushClipRect(ImVec2(a + 1, audioY + 3), ImVec2(b - 1, audioY + 42), true);
        const std::string audioName = RenderBaseName(_renderAudioPath);
        draw->AddText(ImVec2(a + 6, audioY + 13), IM_COL32(255, 255, 255, 255), audioName.c_str());
        draw->PopClipRect();
    }
    const int tickEvery = scale >= 35.0f ? 1 : (scale >= 12.0f ? 5 : 10);
    for (int sec = 0; sec <= static_cast<int>(timelineTotal + 10.0); sec += tickEvery)
    {
        const float x = trackX + sec * scale;
        draw->AddLine(ImVec2(x, rulerY), ImVec2(x, audioY + 45), IM_COL32(100, 100, 100, 85));
        char tick[24];
        std::snprintf(tick, sizeof(tick), "%ds", sec);
        draw->AddText(ImVec2(x + 3, base.y), IM_COL32(190, 190, 190, 255), tick);
    }

    double presetOffset = 0.0, spriteOffset = 0.0;
    for (std::size_t i = 0; i < _sequence.size(); ++i)
    {
        const auto& clip = _sequence[i];
        double& offset = clip.sprite ? spriteOffset : presetOffset;
        const float x1 = trackX + static_cast<float>(offset) * scale;
        const float x2 = x1 + std::max(0.1f, clip.duration) * scale;
        const float y = clip.sprite ? spriteY : presetY;
        const ImVec2 a(x1 + 1.0f, y + 3.0f), b(x2 - 1.0f, y + 42.0f);
        const bool selected = _renderSelected == static_cast<int>(i);
        draw->AddRectFilled(a, b, selected ? IM_COL32(63, 145, 192, 255) :
                            (clip.sprite ? IM_COL32(128, 79, 150, 255) : IM_COL32(57, 95, 145, 255)), 4.0f);
        draw->AddRect(a, b, selected ? IM_COL32(240, 240, 240, 255) : IM_COL32(150, 170, 190, 200), 4.0f);
        if (x2 - x1 > 45.0f)
        {
            const std::string label = RenderBaseName(clip.path);
            draw->PushClipRect(a, b, true);
            draw->AddText(ImVec2(x1 + 6.0f, y + 13.0f), IM_COL32(255, 255, 255, 255), label.c_str());
            draw->PopClipRect();
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            ImGui::IsMouseHoveringRect(a, b))
        {
            _renderSelected = static_cast<int>(i);
            _renderPlayhead = offset;
        }
        offset += std::max(0.1f, clip.duration);
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        ImGui::IsMouseHoveringRect(ImVec2(trackX, base.y), ImVec2(base.x + canvasWidth, presetY - 1.0f)))
    {
        _renderPlayhead = std::max(0.0, std::min(timelineTotal,
            static_cast<double>((ImGui::GetMousePos().x - trackX) / scale)));
        _renderSelected = -1;
    }
    const float playheadX = trackX + static_cast<float>(_renderPlayhead) * scale;
    draw->AddLine(ImVec2(playheadX, rulerY), ImVec2(playheadX, audioY + 45.0f),
                  IM_COL32(255, 210, 100, 255), 2.0f);
    ImGui::EndChild();
}

void PlaylistWindow::SaveRenderJob()
{
    const std::string path(_renderJobPath);
    if (path.size() < 5 || path.substr(path.size() - 5) != ".json")
    {
        std::snprintf(_renderStatus, sizeof(_renderStatus), "Choose a filename ending in .json.");
        return;
    }
    if (_sequence.empty())
    {
        std::snprintf(_renderStatus, sizeof(_renderStatus), "Add sequence clips before saving a render job.");
        return;
    }
    std::ifstream existing(path, std::ios::binary);
    if (existing.good())
    {
        std::snprintf(_renderStatus, sizeof(_renderStatus), "Job file exists; choose a different filename.");
        return;
    }
    std::ofstream out(path, std::ios::binary);
    if (!out)
    {
        std::snprintf(_renderStatus, sizeof(_renderStatus), "Could not create render job.");
        return;
    }
    out << std::fixed << std::setprecision(3);
    out << "{\n  \"schema\": \"thisis395.render-job.v1\",\n";
    out << "  \"status\": \"planned\",\n  \"encoderImplemented\": false,\n";
    out << "  \"output\": {\"path\": \"" << EscapeRenderJson(_renderVideoPath)
        << "\", \"width\": " << _renderWidth << ", \"height\": " << _renderHeight
        << ", \"fps\": " << _renderFps << ", \"codec\": \""
        << ( _renderCodec == 1 ? "hevc" : (_renderCodec == 2 ? "prores_422" : "h264"))
        << "\", \"bitrateMbps\": " << _renderBitrateMbps << "},\n";
    out << "  \"audioSource\": \"" << EscapeRenderJson(_renderAudioPath) << "\",\n";
    out << "  \"audioTrack\": {\"path\": \"" << EscapeRenderJson(_renderAudioPath)
        << "\", \"startSeconds\": " << _renderAudioStart
        << ", \"durationSeconds\": " << _renderAudioDuration
        << ", \"gain\": " << _renderAudioGain << "},\n";
    out << "  \"tracks\": {\n    \"presets\": [\n";
    bool first = true;
    double offset = 0.0;
    for (const auto& clip : _sequence)
    {
        if (clip.sprite) continue;
        if (!first) out << ",\n";
        out << "      {\"path\": \"" << EscapeRenderJson(clip.path) << "\", \"startSeconds\": "
            << offset << ", \"durationSeconds\": " << std::max(0.1f, clip.duration)
            << ", \"fadeSeconds\": " << std::max(0.0f, clip.fade) << "}";
        offset += std::max(0.1f, clip.duration);
        first = false;
    }
    out << "\n    ],\n    \"sprites\": [\n";
    first = true;
    offset = 0.0;
    for (const auto& clip : _sequence)
    {
        if (!clip.sprite) continue;
        if (!first) out << ",\n";
        out << "      {\"path\": \"" << EscapeRenderJson(clip.path) << "\", \"startSeconds\": "
            << offset << ", \"durationSeconds\": " << std::max(0.1f, clip.duration)
            << ", \"fadeSeconds\": " << std::max(0.0f, clip.fade) << "}";
        offset += std::max(0.1f, clip.duration);
        first = false;
    }
    out << "\n    ]\n  }\n}\n";
    out.close();
    std::snprintf(_renderStatus, sizeof(_renderStatus), "%s",
                  out ? "Render job saved (JSON only, no video generated)." : "Failed to write complete render job.");
}

void PlaylistWindow::SetupRenderPrerequisites()
{
    const std::string base = Poco::Path(Poco::Util::Application::instance().commandPath()).parent().toString();
    Poco::Path script(base);
#if defined(_WIN32)
    script.append("setup_render_prereqs.ps1");
    const std::string program = "powershell.exe";
    const std::vector<std::string> args = {"-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script.toString()};
#else
# if defined(__APPLE__)
    script = Poco::Path(base).parent().append("Resources").append("setup_render_prereqs.sh");
# else
    script.append("setup_render_prereqs.sh");
# endif
    const std::string program = "/bin/sh";
    const std::vector<std::string> args = {script.toString()};
#endif
    if (!Poco::File(script).exists())
    {
        std::snprintf(_renderStatus, sizeof(_renderStatus), "Prerequisite setup script not packaged.");
        return;
    }
    try
    {
        // Explicit user click only. Never invoke installation from startup or passive UI updates.
        Poco::Process::launch(program, args);
        std::snprintf(_renderStatus, sizeof(_renderStatus),
                      "Setup launched. Check package-manager prompts; restart the app when finished.");
    }
    catch (const Poco::Exception&)
    {
        std::snprintf(_renderStatus, sizeof(_renderStatus),
                      "Could not launch setup. See docs/ThisIs395_One_Click_Render.md.");
    }
}

void PlaylistWindow::LaunchOfflineRender()
{
    if (_renderJobPath[0] == '\0')
    {
        std::snprintf(_renderStatus, sizeof(_renderStatus), "Set a render-job .json path first.");
        return;
    }
    // Regenerate the manifest from the current timeline before launching the encoder.
    SaveRenderJob();
    if (std::strstr(_renderStatus, "Render job saved") == nullptr)
        return;

    const std::string base = Poco::Path(Poco::Util::Application::instance().commandPath()).parent().toString();
    Poco::Path backend(base);
    Poco::Path driver(base);
#if defined(_WIN32)
    backend.append("projectMSDL-offline.exe");
    driver.append("render_offline.py");
    const std::string program = "python.exe";
#elif defined(__APPLE__)
    backend.append("projectMSDL-offline");
    driver = Poco::Path(base).parent().append("Resources").append("render_offline.py");
    const std::string program = "python3";
#else
    backend.append("projectMSDL-offline");
    driver.append("render_offline.py");
    const std::string program = "python3";
#endif
    if (!Poco::File(backend).exists() || !Poco::File(driver).exists())
    {
        std::snprintf(_renderStatus, sizeof(_renderStatus),
                      "Missing packaged renderer or driver; install an updated build.");
        return;
    }
    try
    {
        Poco::Process::launch(program, {driver.toString(), _renderJobPath, "--backend",
                                       backend.toString(), "--ffmpeg", "ffmpeg"});
        std::snprintf(_renderStatus, sizeof(_renderStatus),
                      "Encoder process launched; this does not confirm video export succeeded.");
    }
    catch (const Poco::Exception&)
    {
        std::snprintf(_renderStatus, sizeof(_renderStatus),
                      "Could not launch Python. Run prerequisite setup, then restart the app.");
    }
}
