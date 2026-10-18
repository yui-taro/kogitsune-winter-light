#include "Audio.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
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
    for (Music music : music_) {
        if (IsMusicValid(music)) UnloadMusicStream(music);
    }
    StopNarration();
    for (Sound sound : narration_) SafeUnload(sound);
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

void GameAudio::LoadNarration(const std::array<std::string, 4>& paths) {
    if (!ready_) return;
    StopNarration();
    for (size_t i = 0; i < narration_.size(); ++i) {
        SafeUnload(narration_[i]);
        narration_[i] = LoadSound(paths[i].c_str());
        SafeVolume(narration_[i], 0.85f);
    }
}

void GameAudio::PlayNarration(int index) {
    if (!ready_) return;
    StopNarration();
    if (index < 0 || index >= static_cast<int>(narration_.size())) return;
    currentNarration_ = index;
    SafePlay(narration_[index]);
}

void GameAudio::StopNarration() {
    if (!ready_ || currentNarration_ < 0) return;
    if (IsSoundValid(narration_[currentNarration_])) StopSound(narration_[currentNarration_]);
    currentNarration_ = -1;
}

void GameAudio::PauseNarration() {
    if (ready_ && currentNarration_ >= 0 && IsSoundValid(narration_[currentNarration_]))
        PauseSound(narration_[currentNarration_]);
}

void GameAudio::ResumeNarration() {
    if (ready_ && currentNarration_ >= 0 && IsSoundValid(narration_[currentNarration_]))
        ResumeSound(narration_[currentNarration_]);
}

bool GameAudio::LoadMusicPack(const std::string& path) {
    if (!ready_ || IsMusicValid(music_[0])) return false;
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::vector<unsigned char> pack(std::istreambuf_iterator<char>{file}, {});
    constexpr unsigned char magic[8] = {'K', 'W', 'B', 'G', 'M', '1', 0, 0};
    constexpr size_t headerSize = 8 + 5 * 4;
    if (pack.size() < headerSize || !std::equal(std::begin(magic), std::end(magic), pack.begin())) return false;
    size_t offset = headerSize;
    for (size_t index = 0; index < music_.size(); ++index) {
        size_t length = 0;
        for (int byte = 0; byte < 4; ++byte)
            length |= static_cast<size_t>(pack[8 + index * 4 + byte]) << (byte * 8);
        if (length == 0 || length > pack.size() - offset) return false;
        auto& data = musicData_[index];
        data.assign(pack.begin() + offset, pack.begin() + offset + length);
        std::uint32_t state = 0x7A53C18Du ^ (static_cast<std::uint32_t>(index + 1) * 0x9E3779B9u);
        for (unsigned char& value : data) {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            value ^= static_cast<unsigned char>(state);
        }
        if (data.size() < 4 || !std::equal(data.begin(), data.begin() + 4, "OggS")) return false;
        music_[index] = LoadMusicStreamFromMemory(".ogg", data.data(), static_cast<int>(data.size()));
        if (!IsMusicValid(music_[index])) return false;
        offset += length;
    }
    return offset == pack.size();
}

void GameAudio::UpdateMusic(MusicCue cue, float delta, bool focused, bool paused, bool narrationActive) {
    if (!ready_) return;
    int requested = static_cast<int>(cue);
    if (requested != currentCue_) {
        if (outgoingCue_ >= 0 && IsMusicValid(music_[outgoingCue_])) StopMusicStream(music_[outgoingCue_]);
        outgoingCue_ = currentCue_;
        currentCue_ = requested;
        musicFade_ = 0.0f;
        if (IsMusicValid(music_[currentCue_])) {
            SetMusicVolume(music_[currentCue_], 0.0f);
            PlayMusicStream(music_[currentCue_]);
        }
        musicSuspended_ = false;
    }
    bool suspend = !focused || paused;
    if (suspend != musicSuspended_) {
        for (int index : {currentCue_, outgoingCue_}) {
            if (index < 0 || !IsMusicValid(music_[index])) continue;
            if (suspend) PauseMusicStream(music_[index]);
            else ResumeMusicStream(music_[index]);
        }
        musicSuspended_ = suspend;
    }
    if (suspend) return;
    musicFade_ = std::min(1.0f, musicFade_ + std::clamp(delta, 0.0f, 0.05f) / 1.1f);
    float level = narrationActive ? 0.15f : cue == MusicCue::Ending ? 0.48f : 0.36f;
    if (IsMusicValid(music_[currentCue_])) {
        SetMusicVolume(music_[currentCue_], level * musicFade_);
        UpdateMusicStream(music_[currentCue_]);
    }
    if (outgoingCue_ >= 0 && IsMusicValid(music_[outgoingCue_])) {
        SetMusicVolume(music_[outgoingCue_], level * (1.0f - musicFade_));
        UpdateMusicStream(music_[outgoingCue_]);
        if (musicFade_ >= 1.0f) {
            StopMusicStream(music_[outgoingCue_]);
            outgoingCue_ = -1;
        }
    }
}

} // namespace foxgame
