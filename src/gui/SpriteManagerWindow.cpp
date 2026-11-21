#include "SpriteManagerWindow.h"

#include "ProjectMWrapper.h"

#include "imgui.h"

#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/String.h>
#include <Poco/Util/Application.h>

#include <algorithm>
#include <string>
#include <utility>

namespace
{
constexpr size_t kMaxManagedSprites = 16;

const char* AudioBandVariable(int band)
{
    switch (band)
    {
        case 1: return "bass_att";
        case 2: return "mid_att";
        case 3: return "treb_att";
        default: return "0";
    }
}
}

SpriteManagerWindow::SpriteManagerWindow(ProjectMGUI&)
    : _projectMWrapper(Poco::Util::Application::instance().getSubsystem<ProjectMWrapper>())
{
    _fileChooser.Title("Select transparent PNG sprite");
    _fileChooser.AllowedExtensions({"png"});
}

void SpriteManagerWindow::Show()
{
    _visible = true;
}

void SpriteManagerWindow::Draw()
{
    if (!_visible)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(500, 550), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("ThisIs395 VJ - Sprite Manager", &_visible))
    {
        if (ImGui::Button("Add PNG..."))
        {
            _fileChooser.Show();
        }

        ImGui::SameLine();
        ImGui::Text("%zu / %zu sprites", _sprites.size(), kMaxManagedSprites);
        ImGui::TextDisabled("PNG alpha transparency; reactive bands use projectM's audio analysis.");

        if (!_status.empty())
        {
            ImGui::TextWrapped("%s", _status.c_str());
        }

        ImGui::Separator();

        for (size_t i = 0; i < _sprites.size();)
        {
            Sprite& sprite = _sprites[i];
            ImGui::PushID(static_cast<int>(i));

            if (ImGui::CollapsingHeader(sprite.textureName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::TextDisabled("%s", sprite.path.c_str());
                bool changed = false;
                changed |= ImGui::SliderFloat("X", &sprite.x, 0.0f, 1.0f);
                changed |= ImGui::SliderFloat("Y", &sprite.y, 0.0f, 1.0f);
                changed |= ImGui::SliderFloat("Scale", &sprite.scale, 0.02f, 2.0f);
                changed |= ImGui::SliderFloat("Opacity", &sprite.opacity, 0.0f, 1.0f);
                changed |= ImGui::SliderAngle("Rotation", &sprite.rotation, -180.0f, 180.0f);
                changed |= ImGui::SliderFloat("Audio gain", &sprite.gain, 0.0f, 3.0f);

                const char* bandNames[] = {"Off", "Bass (smoothed)", "Mids (smoothed)", "Treble (smoothed)"};
                if (ImGui::Combo("Audio band", &sprite.audioBand, bandNames, 4))
                {
                    // The per-frame expression is compiled on sprite creation.
                    // Replace the sprite when switching bands.
                    if (sprite.id)
                    {
                        projectm_sprite_destroy(_projectMWrapper.ProjectM(), sprite.id);
                        sprite.id = 0;
                    }
                    if (!CreateSprite(sprite))
                    {
                        _status = "Unable to re-create sprite after changing audio band.";
                    }
                }
                else if (changed)
                {
                    ApplyVariables(sprite);
                }

                if (ImGui::Button("Remove sprite"))
                {
                    RemoveSprite(i);
                    ImGui::PopID();
                    continue;
                }
            }

            ImGui::PopID();
            ++i;
        }
    }

    ImGui::End();

    if (_fileChooser.Draw() && !_fileChooser.SelectedFiles().empty())
    {
        AddFile(_fileChooser.SelectedFiles().front().path());
    }
}

void SpriteManagerWindow::AddFile(const std::string& path)
{
    if (_sprites.size() >= kMaxManagedSprites)
    {
        _status = "The current Sprite Manager supports up to 16 sprites.";
        return;
    }

    Poco::File file(path);
    Poco::Path filePath(path);

    if (!file.exists() || !file.isFile() || !file.canRead() ||
        Poco::icompare(filePath.getExtension(), "png") != 0)
    {
        _status = "Please select a readable PNG image.";
        return;
    }

    const std::string textureName = filePath.getBaseName();
    for (const auto& existing : _sprites)
    {
        if (Poco::icompare(existing.textureName, textureName) == 0)
        {
            _status = "A sprite with that PNG basename is already loaded; rename one of the files.";
            return;
        }
    }

    // Texture lookup uses filename stems, so make the selected directory
    // available before asking libprojectM to create its MilkDrop sprite.
    _projectMWrapper.AddSpriteTextureDirectory(filePath.parent().toString());

    Sprite sprite;
    sprite.path = path;
    sprite.textureName = textureName;

    if (!CreateSprite(sprite))
    {
        _status = "libprojectM rejected the sprite. Check the PNG and the application log.";
        return;
    }

    _sprites.push_back(std::move(sprite));
    _status = "Added sprite. Audio reaction is enabled by default.";
}

bool SpriteManagerWindow::CreateSprite(Sprite& sprite)
{
    const std::string definition =
        "img=" + sprite.textureName + "\n"
        "init_1=burn=0;blendmode=4;\n"
        "code_1=x=vjx;y=vjy;rot=vjrotation;a=vjopacity;\n"
        "code_2=sx=vjscale*(1+vjgain*max(0," +
        std::string(AudioBandVariable(sprite.audioBand)) +
        "-1));sy=sx;\n";

    sprite.id = projectm_sprite_create(_projectMWrapper.ProjectM(), "milkdrop", definition.c_str());

    if (sprite.id == 0)
    {
        return false;
    }

    ApplyVariables(sprite);
    return true;
}

void SpriteManagerWindow::ApplyVariables(const Sprite& sprite)
{
    if (sprite.id == 0)
    {
        return;
    }

    auto* handle = _projectMWrapper.ProjectM();
    projectm_sprite_set_var(handle, sprite.id, "vjx", sprite.x);
    projectm_sprite_set_var(handle, sprite.id, "vjy", sprite.y);
    projectm_sprite_set_var(handle, sprite.id, "vjscale", sprite.scale);
    projectm_sprite_set_var(handle, sprite.id, "vjopacity", sprite.opacity);
    projectm_sprite_set_var(handle, sprite.id, "vjrotation", sprite.rotation);
    projectm_sprite_set_var(handle, sprite.id, "vjgain", sprite.gain);
}

void SpriteManagerWindow::RemoveSprite(size_t index)
{
    if (index >= _sprites.size())
    {
        return;
    }

    if (_sprites[index].id != 0)
    {
        projectm_sprite_destroy(_projectMWrapper.ProjectM(), _sprites[index].id);
    }

    _sprites.erase(_sprites.begin() + static_cast<std::ptrdiff_t>(index));
    _status = "Sprite removed.";
}
