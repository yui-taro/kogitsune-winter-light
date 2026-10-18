#pragma once

#include "World.hpp"
#include "raylib.h"
#include <array>
#include <string>
#include <vector>

namespace foxgame {

enum class MusicCue { Title, Prologue, Forest, Shrine, Ending };

class GameAudio {
public:
    GameAudio();
    ~GameAudio();
    void Poll(const SoundEvents& events);
    void LoadNarration(const std::array<std::string, 4>& paths);
    void PlayNarration(int index);
    void StopNarration();
    void PauseNarration();
    void ResumeNarration();
    bool LoadMusicPack(const std::string& path);
    void UpdateMusic(MusicCue cue, float delta, bool focused, bool paused, bool narrationActive);

private:
    bool ready_ = false;
    Sound bell_{};
    Sound step_{};
    Sound sweep_{};
    Sound lantern_{};
    Sound stone_{};
    Sound capture_{};
    Sound fire_{};
    std::array<Sound, 4> narration_{};
    int currentNarration_ = -1;
    std::array<Music, 5> music_{};
    std::array<std::vector<unsigned char>, 5> musicData_{};
    int currentCue_ = -1;
    int outgoingCue_ = -1;
    float musicFade_ = 1.0f;
    bool musicSuspended_ = false;
    SoundEvents previous_{};
};

} // namespace foxgame
