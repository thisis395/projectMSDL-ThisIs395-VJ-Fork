#include "SpriteManagerWindow.h"

#include "ProjectMWrapper.h"

#include <projectM-4/user_sprites.h>

#include "imgui.h"

#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/String.h>
#include <Poco/Util/Application.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace
{
constexpr std::size_t kMaxManagedSprites = 16;

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

        for (std::size_t i = 0; i < _sprites.size();)
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

bool SpriteManagerWindow::AddFile(const std::string& path)
{
    if (_sprites.size() >= kMaxManagedSprites)
    {
        _status = "The current Sprite Manager supports up to 16 sprites.";
        return false;
    }

    const Poco::Path filePath(path);
    if (Poco::icompare(filePath.getExtension(), "png") != 0)
    {
        _status = "Please select a PNG image.";
        return false;
    }

    for (const auto& existing : _sprites)
    {
        if (existing.path == path)
        {
            _status = "This PNG is already loaded into the visualizer.";
            return false;
        }
    }

    Sprite sprite;
    sprite.path = path;
    std::string loadError;
    if (!_projectMWrapper.PrepareSpriteTexture(path, sprite.textureName, loadError))
    {
        _status = "PNG import failed: " + loadError;
        return false;
    }

    if (!CreateSprite(sprite))
    {
        _projectMWrapper.ReleaseSpriteTexture(sprite.textureName);
        _status = "libprojectM rejected the sprite. Check the renderer output.";
        return false;
    }

    // A successful sprite ID alone is NOT proof that libprojectM loaded the
    // selected image; its filesystem fallback may use a blank placeholder.
    if (!_projectMWrapper.SpriteTextureDelivered(sprite.textureName))
    {
        projectm_sprite_destroy(_projectMWrapper.ProjectM(), sprite.id);
        _projectMWrapper.ReleaseSpriteTexture(sprite.textureName);
        _status = "projectM did not request the imported PNG pixels. No sprite was added.";
        return false;
    }

    _sprites.push_back(std::move(sprite));
    _status = "PNG decoded and texture delivered to projectM.";
    return true;
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

void SpriteManagerWindow::RemoveSprite(std::size_t index)
{
    if (index >= _sprites.size())
    {
        return;
    }

    if (_sprites[index].id != 0)
    {
        projectm_sprite_destroy(_projectMWrapper.ProjectM(), _sprites[index].id);
    }

    _projectMWrapper.ReleaseSpriteTexture(_sprites[index].textureName);
    _sprites.erase(_sprites.begin() + static_cast<std::ptrdiff_t>(index));
    _status = "Sprite removed.";
}


std::vector<SpriteManagerWindow::Controls> SpriteManagerWindow::Snapshot() const
{
    std::vector<Controls> items;
    items.reserve(_sprites.size());
    for (const auto& sprite : _sprites)
    {
        items.push_back({sprite.path, sprite.x, sprite.y, sprite.scale,
                         sprite.opacity, sprite.rotation, sprite.gain, sprite.audioBand});
    }
    return items;
}

bool SpriteManagerWindow::UpdateManagedSprite(std::size_t index, const Controls& settings)
{
    if (index >= _sprites.size())
    {
        _status = "Sprite no longer exists. Refresh the live list.";
        return false;
    }
    const auto valid = [](float value, float low, float high) {
        return std::isfinite(value) && value >= low && value <= high;
    };
    if (!valid(settings.x, 0.0f, 1.0f) ||
        !valid(settings.y, 0.0f, 1.0f) ||
        !valid(settings.scale, 0.02f, 2.0f) ||
        !valid(settings.opacity, 0.0f, 1.0f) ||
        !valid(settings.rotation, -3.141593f, 3.141593f) ||
        !valid(settings.gain, 0.0f, 3.0f) ||
        settings.audioBand < 0 || settings.audioBand > 3)
    {
        _status = "Sprite settings are outside the supported ranges.";
        return false;
    }

    Sprite& sprite = _sprites[index];
    const Sprite previous = sprite;
    sprite.x = settings.x;
    sprite.y = settings.y;
    sprite.scale = settings.scale;
    sprite.opacity = settings.opacity;
    sprite.rotation = settings.rotation;
    sprite.gain = settings.gain;

    if (previous.audioBand != settings.audioBand)
    {
        // Band selection is compiled into the MilkDrop expression. Re-create
        // it on the same render thread rather than mutating expression code.
        if (sprite.id != 0)
            projectm_sprite_destroy(_projectMWrapper.ProjectM(), sprite.id);
        sprite.id = 0;
        sprite.audioBand = settings.audioBand;
        if (!CreateSprite(sprite))
        {
            sprite = previous;
            sprite.id = 0; // The old ID was already destroyed.
            if (!CreateSprite(sprite))
                _status = "Unable to create sprite; restoring previous audio band also failed.";
            else
                _status = "Unable to switch audio band; restored previous settings.";
            return false;
        }
    }
    else
    {
        ApplyVariables(sprite);
    }
    _status = "Live sprite settings applied.";
    return true;
}

bool SpriteManagerWindow::RemoveManagedSprite(std::size_t index)
{
    if (index >= _sprites.size())
    {
        _status = "Sprite no longer exists. Refresh the live list.";
        return false;
    }
    RemoveSprite(index);
    return true;
}
