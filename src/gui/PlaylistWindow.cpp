#include "PlaylistWindow.h"

#include "ProjectMWrapper.h"
#include "imgui.h"

#include <Poco/Util/Application.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
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

    ImGui::SetNextWindowSize(ImVec2(760, 540), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("ThisIs395 VJ - Playlist", &_visible))
    {
        if (ImGui::BeginTabBar("PlaylistTabs"))
        {
            if (ImGui::BeginTabItem("Playlist"))
            {
                DrawPlaylistTab();
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

void PlaylistWindow::DrawExportTab()
{
    ImGui::TextUnformatted("Playlist video export");
    ImGui::Separator();
    ImGui::TextWrapped("Planned: render presets, sprite overlays, and synchronized audio "
                       "to an offline video using deterministic frame timing and FFmpeg.");
    ImGui::Spacing();
    ImGui::TextDisabled("Target: H.264 MP4 / 1080p60, with resolution, FPS, "
                        "codec, range, quality, and audio controls.");
    ImGui::Spacing();
    ImGui::TextWrapped("Export is not implemented yet. This tab is a placeholder, "
                       "not a recorder or video encoder.");
}
