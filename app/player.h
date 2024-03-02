#pragma once

#include <map>
#include <variant>
#include <functional>

#include "minisdl_audio.h"
#include "tsf.h"
#include "tml.h"

class Player
{
public:
    enum InfoType {
        I_USED_CHANNELS,
        I_USED_PROGRAMS,
        I_TOTAL_NOTES,
        I_1ST_NOTE_MS,
        I_LENGTH_MS
    };

    static Player * instance();

    void play();
    void pause();
    void stop();
    bool isPlaying() const;
    void seekTo(unsigned int ms);
    std::map<enum InfoType, std::variant<int, unsigned int> > midiInfo() const;

    void setVolume(float volume);

    bool loadMidiFile(const char * filePath);
    bool loadSF2File(const char * sf2Path);

    void setPlaybackCallback(std::function<void(unsigned int)> cb);

private:
    Player();

    static Player * m_player_instance;
    void sdl_audioCallback(void* data, uint8_t *stream, int len);

    float m_volume = 1;

    SDL_AudioSpec m_sdlOutputAudioSpec;
    tml_message* m_tinyMidiLoader = NULL;
    tsf* m_tinySoundFont = NULL;
    tml_message* m_currentPlaybackMessagePos;
    double m_currentPlaybackMSec = 0;
    tml_message* m_seekToMessagePos = NULL;
    double m_seekToMSec = 0;
    std::function<void(unsigned int)> mf_playbackCallback;
};
