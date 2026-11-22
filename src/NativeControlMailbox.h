#pragma once

#include <Poco/File.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Path.h>
#include <Poco/NotificationCenter.h>
#include <Poco/Exception.h>
#include <Poco/Util/Application.h>

#include <fstream>
#include <sstream>
#include <string>

#include "notifications/PlaybackControlNotification.h"

// Local-only, single-inflight v1 command mailbox. Called exclusively on the SDL
// render thread, so libprojectM calls never race against rendering.
class NativeControlMailbox {
public:
    explicit NativeControlMailbox(std::string directory)
        : directory_(std::move(directory)) {}

    bool enabled() const { return !directory_.empty(); }

    void poll(projectm_handle renderer, projectm_playlist_handle playlist) {
        if (!enabled() || !renderer || !playlist) return;
        Poco::Path input(directory_);
        input.append("command.json");
        Poco::File command(input);
        if (!command.exists()) return;

        std::string id;
        Poco::JSON::Object::Ptr reply = new Poco::JSON::Object;
        reply->set("version", 1);
        try {
            if (command.getSize() > 8192)
                throw Poco::InvalidArgumentException("Command too large");
            std::ifstream stream(input.toString(), std::ios::binary);
            if (!stream) return;
            std::ostringstream content;
            content << stream.rdbuf();
            Poco::JSON::Parser parser;
            auto request = parser.parse(content.str()).extract<Poco::JSON::Object::Ptr>();
            id = request->getValue<std::string>("id");
            const auto version = request->getValue<int>("version");
            const auto action = request->getValue<std::string>("action");
            if (id.empty() || id.size() > 64 || version != 1)
                throw Poco::InvalidArgumentException("Unsupported request");
            reply->set("id", id);
            auto post = [](PlaybackControlNotification::Action action) {
                Poco::NotificationCenter::defaultCenter().postNotification(
                    new PlaybackControlNotification(action));
            };
            if (action == "ping") {
                // Handshake only.
            } else if (action == "next") {
                post(PlaybackControlNotification::Action::NextPreset);
            } else if (action == "previous") {
                post(PlaybackControlNotification::Action::PreviousPreset);
            } else if (action == "shuffle") {
                post(PlaybackControlNotification::Action::ToggleShuffle);
            } else if (action == "lock") {
                post(PlaybackControlNotification::Action::TogglePresetLocked);
            } else if (action == "cue") {
                const auto path = request->getValue<std::string>("path");
                bool found = false;
                for (uint32_t i = 0; i < projectm_playlist_size(playlist); ++i) {
                    char* raw = projectm_playlist_item(playlist, i);
                    if (!raw) continue;
                    const bool match = path == raw;
                    projectm_playlist_free_string(raw);
                    if (match) {
                        projectm_playlist_set_position(playlist, i, false);
                        found = true;
                        break;
                    }
                }
                if (!found) throw Poco::NotFoundException("Preset is not in the live playlist");
            } else {
                throw Poco::InvalidArgumentException("Unknown action");
            }
            reply->set("ok", true);
            reply->set("shuffle", projectm_playlist_get_shuffle(playlist));
            reply->set("locked", projectm_get_preset_locked(renderer));
            reply->set("presetIndex", static_cast<int>(projectm_playlist_get_position(playlist)));
        } catch (const Poco::Exception& e) {
            reply->set("id", id);
            reply->set("ok", false);
            reply->set("error", e.displayText());
        } catch (const std::exception& e) {
            reply->set("id", id);
            reply->set("ok", false);
            reply->set("error", e.what());
        }
        try {
            // Remove the command before publishing its acknowledgment.
            command.remove();
            Poco::Path output(directory_);
            output.append("response.json");
            Poco::Path temp(directory_);
            temp.append("response.tmp");
            std::ofstream stream(temp.toString(), std::ios::binary | std::ios::trunc);
            if (!stream) return;
            Poco::JSON::Stringifier::stringify(reply, stream);
            stream.close();
            if (!stream) return;
            Poco::File target(output);
            if (target.exists()) target.remove();
            Poco::File(temp).renameTo(output.toString());
        } catch (const Poco::Exception&) {
            // Keep rendering even if mailbox storage becomes unavailable.
        }
    }
private:
    std::string directory_;
};
