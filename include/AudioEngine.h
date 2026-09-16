#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include <map>
#include <string>

class AudioEngine
{
    public:
    AudioEngine();
    ~AudioEngine();

    bool initialize(
        const std::string& soundDirectory
    );

    bool loadSound(
        const std::string& note,
        const std::string& filname
    );

    void play(
        const std::string& note
    );
    void stop(
        const std::string& note
    );

    bool isReady() const;

    private:
    std::map<std::string, Mix_Chunk*> sounds;
    std::map<std::string, int> activeChannels;

    std::string soundDirectory;
    bool initialized;
};
#endif