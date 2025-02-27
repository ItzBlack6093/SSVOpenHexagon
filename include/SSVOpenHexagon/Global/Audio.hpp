// Copyright (c) 2013-2020 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: https://opensource.org/licenses/AFL-3.0

#pragma once

#include "SSVOpenHexagon/Utils/UniquePtr.hpp"

#include <string>
#include <functional>

namespace sf {
class PlaybackDevice;
class SoundBuffer;
} // namespace sf

namespace hg {

class Audio
{
public:
    using SoundBufferGetter =
        std::function<sf::SoundBuffer*(const std::string&)>;

    using MusicPathGetter =
        std::function<const std::string*(const std::string&)>;

private:
    class AudioImpl;

    Utils::UniquePtr<AudioImpl> _impl;

    [[nodiscard]] const AudioImpl& impl() const noexcept;
    [[nodiscard]] AudioImpl& impl() noexcept;

public:
    explicit Audio(sf::PlaybackDevice& playbackDevice,
        const SoundBufferGetter& soundBufferGetter,
        const MusicPathGetter& musicPathGetter);

    ~Audio();

    void setSoundVolume(const float volume);
    void setMusicVolume(const float volume);

    void resumeMusic();
    void pauseMusic();
    void stopMusic();

    void setMusicPlayingOffsetSeconds(const float seconds);
    void setMusicPlayingOffsetMilliseconds(const int milliseconds);

    [[nodiscard]] float getMusicPlayingOffsetSeconds() const;
    [[nodiscard]] int getMusicPlayingOffsetMilliseconds() const;

    void stopSounds();

    void playSoundOverride(const std::string& id);
    void playPackSoundOverride(
        const std::string& packId, const std::string& id);

    void playSoundAbort(const std::string& id);
    void playPackSoundAbort(const std::string& packId, const std::string& id);

    [[nodiscard]] bool loadAndPlayMusic(const std::string& packId,
        const std::string& id, const float playingOffsetSeconds);

    void setCurrentMusicPitch(const float pitch);
};

} // namespace hg
