#include "PlaylistWindow.h"

#include "ProjectMWrapper.h"
#include "imgui.h"

#include <Poco/Util/Application.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
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
    ImGui::SetNextWindowSize(ImVec2(1120, 680), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("ThisIs395 VJ - Playlist", &_visible))
    {
        if (ImGui::BeginTabBar("PlaylistTabs"))
        {
            if (ImGui::BeginTabItem("Playlist + Sequence"))
            {
                const float available = ImGui::GetContentRegionAvail().x;
                const float leftWidth = std::max(320.0f, available * 0.42f);
                ImGui::BeginChild("SequencePanel", ImVec2(leftWidth, 0), true);
                DrawSequenceTab();
                ImGui::EndChild();
                ImGui::SameLine();
                ImGui::BeginChild("LibraryPanel", ImVec2(0, 0), true);
                DrawPlaylistTab();
                ImGui::EndChild();
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
    if (ImGui::Button("Play preset sequence"))
    {
        if (_sequence.empty())
        {
            std::snprintf(_status, sizeof(_status), "Add presets to the sequence first.");
        }
        else
        {
            _sequencePlaying = true;
            _sequencePosition = 0;
            _sequenceStarted = 0;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop sequence"))
    {
        _sequencePlaying = false;
        _sequenceStarted = 0;
    }
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

void PlaylistWindow::UpdateSequence()
{
    if (!_sequencePlaying || _sequence.empty())
        return;
    if (_sequencePosition >= _sequence.size())
    {
        _sequencePlaying = false;
        return;
    }
    const uint64_t now = SDL_GetTicks64();
    if (_sequenceStarted != 0 &&
        static_cast<double>(now - _sequenceStarted) < _sequence[_sequencePosition].duration * 1000.0)
        return;

    if (_sequenceStarted != 0)
        ++_sequencePosition;
    while (_sequencePosition < _sequence.size() && _sequence[_sequencePosition].sprite)
        ++_sequencePosition;
    if (_sequencePosition >= _sequence.size())
    {
        _sequencePlaying = false;
        return;
    }

    auto* playlist = _projectMWrapper.Playlist();
    const auto& path = _sequence[_sequencePosition].path;
    const uint32_t count = projectm_playlist_size(playlist);
    bool found = false;
    for (uint32_t i = 0; i < count; ++i)
    {
        char* item = projectm_playlist_item(playlist, i);
        if (!item)
            continue;
        const bool matches = path == item;
        projectm_playlist_free_string(item);
        if (matches)
        {
            const float fadeSeconds = _sequence[_sequencePosition].fade;
            projectm_set_soft_cut_duration(_projectMWrapper.ProjectM(), fadeSeconds);
            projectm_playlist_set_position(playlist, i, _hardCut || fadeSeconds <= 0.0f);
            found = true;
            break;
        }
    }
    if (!found)
    {
        _sequencePlaying = false;
        std::snprintf(_status, sizeof(_status), "A sequence preset is missing from the live playlist.");
    }
    else
    {
        _sequenceStarted = now;
    }
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
