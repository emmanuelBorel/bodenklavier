#include "AudioEngine.h"

#include <iostream>

using namespace std;


AudioEngine::AudioEngine()
    : initialized(false)
{
}


AudioEngine::~AudioEngine()
{
    for (auto& entry : sounds)
    {
        if (entry.second != nullptr)
        {
            Mix_FreeChunk(
                entry.second
            );
        }
    }

    sounds.clear();
    activeChannels.clear();

    if (initialized)
    {
        Mix_CloseAudio();
        SDL_QuitSubSystem(
            SDL_INIT_AUDIO
        );
    }
}


bool AudioEngine::initialize(
    const string& directory
)
{
    soundDirectory =
        directory;


    if (
        SDL_InitSubSystem(
            SDL_INIT_AUDIO
        ) < 0
    )
    {
        cerr
            << "Erreur SDL Audio : "
            << SDL_GetError()
            << endl;

        return false;
    }


    if (
        Mix_OpenAudio(
            44100,
            MIX_DEFAULT_FORMAT,
            2,
            1024
        ) < 0
    )
    {
        cerr
            << "Erreur SDL_mixer : "
            << Mix_GetError()
            << endl;

        return false;
    }


    // Plusieurs notes peuvent etre jouees
    // simultanement.
    Mix_AllocateChannels(32);


    initialized = true;


    cout
        << "AudioEngine initialise."
        << endl;


    return true;
}


bool AudioEngine::loadSound(
    const string& note,
    const string& filename
)
{
    if (!initialized)
    {
        return false;
    }


    const string fullPath =
        soundDirectory
        + "/"
        + filename;


    Mix_Chunk* sound =
        Mix_LoadWAV(
            fullPath.c_str()
        );


    if (sound == nullptr)
    {
        cerr
            << "Impossible de charger "
            << note
            << " : "
            << fullPath
            << endl;

        cerr
            << Mix_GetError()
            << endl;

        return false;
    }


    sounds[note] =
        sound;


    cout
        << "Son charge : "
        << note
        << " -> "
        << fullPath
        << endl;


    return true;
}


void AudioEngine::play(
    const string& note
)
{
    if (!initialized)
    {
        return;
    }


    auto it =
        sounds.find(note);


    if (it == sounds.end())
    {
        cerr
            << "Aucun son pour la note : "
            << note
            << endl;

        return;
    }


    const int channel =
        Mix_PlayChannel(
            -1,
            it->second,
            0
        );


    if (channel == -1)
    {
        cerr
            << "Erreur lecture "
            << note
            << " : "
            << Mix_GetError()
            << endl;

        return;
    }


    activeChannels[note] =
        channel;
}


void AudioEngine::stop(
    const string& note
)
{
    auto it =
        activeChannels.find(note);


    if (
        it ==
        activeChannels.end()
    )
    {
        return;
    }


    Mix_HaltChannel(
        it->second
    );


    activeChannels.erase(
        it
    );
}


bool AudioEngine::isReady() const
{
    return initialized;
}