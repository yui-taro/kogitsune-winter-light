#pragma once

#include "World.hpp"
#include "raylib.h"

namespace foxgame {

class GameAudio {
public:
    GameAudio();
    ~GameAudio();
    void Poll(const SoundEvents& events);

private:
    bool ready_ = false;
    Sound bell_{};
    Sound step_{};
    Sound sweep_{};
    Sound lantern_{};
    Sound stone_{};
    Sound capture_{};
    Sound fire_{};
    SoundEvents previous_{};
};

} // namespace foxgame
