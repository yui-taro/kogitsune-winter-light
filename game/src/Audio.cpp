#include "Audio.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace foxgame {
namespace {

constexpr int kSampleRate = 22050;
constexpr float kTau = 6.28318530718f;

enum class Tone { Bell, Step, Sweep, Lantern, Stone, Capture, Fire };

Sound MakeSound(Tone tone, float seconds) {
    int count = static_cast<int>(seconds * kSampleRate);
    std::vector<std::int16_t> samples(static_cast<size_t>(count));
    std::uint32_t noiseState = 0x19ae593bu;
    float smoothNoise = 0.0f;
    for (int i = 0; i < count; ++i) {
        float t = static_cast<float>(i) / kSampleRate;
        noiseState = noiseState * 1664525u + 1013904223u;
        float noise = static_cast<float>((noiseState >> 16) & 0xffffu) / 32768.0f - 1.0f;
        smoothNoise += (noise - smoothNoise) * 0.16f;
        float value = 0.0f;
        switch (tone) {
            case Tone::Bell:
                value = std::sin(kTau * 935.0f * t) * std::exp(-2.7f * t) * 0.65f +
                        std::sin(kTau * 1402.0f * t) * std::exp(-4.5f * t) * 0.24f +
                        std::sin(kTau * 1878.0f * t) * std::exp(-7.0f * t) * 0.10f;
                break;
            case Tone::Step:
                value = (smoothNoise * 0.65f + noise * 0.16f) * std::exp(-22.0f * t);
                break;
            case Tone::Sweep:
                value = (smoothNoise * 0.42f + noise * 0.10f) *
                        std::sin(std::min(1.0f, t / seconds) * 3.14159f);
                break;
            case Tone::Lantern:
                value = std::sin(kTau * (420.0f + 240.0f * t) * t) * std::exp(-4.5f * t) * 0.35f +
                        std::sin(kTau * 840.0f * t) * std::exp(-7.0f * t) * 0.15f;
                break;
            case Tone::Stone:
                value = std::sin(kTau * (96.0f - 25.0f * t) * t) * std::exp(-2.2f * t) * 0.36f +
                        smoothNoise * std::exp(-2.5f * t) * 0.20f;
                break;
            case Tone::Capture:
                value = std::sin(kTau * (250.0f - 100.0f * t) * t) * std::exp(-5.0f * t) * 0.36f;
                break;
            case Tone::Fire:
                value = (smoothNoise * 0.16f + std::sin(kTau * 180.0f * t) * 0.1f) *
                        std::exp(-1.7f * t);
                break;
        }
        value *= std::min(1.0f, t * 350.0f);
        samples[static_cast<size_t>(i)] = static_cast<std::int16_t>(
            std::clamp(value, -1.0f, 1.0f) * 28000.0f);
    }
    Wave wave{static_cast<unsigned int>(count), kSampleRate, 16, 1, samples.data()};
    return LoadSoundFromWave(wave);
}

void SafePlay(Sound sound) { if (IsSoundValid(sound)) PlaySound(sound); }
void SafeUnload(Sound sound) { if (IsSoundValid(sound)) UnloadSound(sound); }
void SafeVolume(Sound sound, float volume) { if (IsSoundValid(sound)) SetSoundVolume(sound, volume); }

} // namespace

GameAudio::GameAudio() {
    InitAudioDevice();
    ready_ = IsAudioDeviceReady();
    if (!ready_) return;
    bell_ = MakeSound(Tone::Bell, 1.7f);
    step_ = MakeSound(Tone::Step, 0.16f);
    sweep_ = MakeSound(Tone::Sweep, 0.34f);
    lantern_ = MakeSound(Tone::Lantern, 0.72f);
    stone_ = MakeSound(Tone::Stone, 1.2f);
    capture_ = MakeSound(Tone::Capture, 0.7f);
    fire_ = MakeSound(Tone::Fire, 1.2f);
    SafeVolume(bell_, 0.38f);
    SafeVolume(step_, 0.12f);
    SafeVolume(sweep_, 0.24f);
    SafeVolume(lantern_, 0.38f);
    SafeVolume(stone_, 0.32f);
    SafeVolume(capture_, 0.33f);
    SafeVolume(fire_, 0.43f);
}

GameAudio::~GameAudio() {
    if (!ready_) return;
    SafeUnload(bell_);
    SafeUnload(step_);
    SafeUnload(sweep_);
    SafeUnload(lantern_);
    SafeUnload(stone_);
    SafeUnload(capture_);
    SafeUnload(fire_);
    CloseAudioDevice();
}

void GameAudio::Poll(const SoundEvents& events) {
    if (!ready_) return;
    if (events.bell > previous_.bell) SafePlay(bell_);
    if (events.step > previous_.step) SafePlay(step_);
    if (events.sweep > previous_.sweep) SafePlay(sweep_);
    if (events.lantern > previous_.lantern) SafePlay(lantern_);
    if (events.stone > previous_.stone) SafePlay(stone_);
    if (events.capture > previous_.capture) SafePlay(capture_);
    if (events.fire > previous_.fire) SafePlay(fire_);
    previous_ = events;
}

} // namespace foxgame
