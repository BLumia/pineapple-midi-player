// SPDX-FileCopyrightText: 2024 Gary Wang <git@blumia.net>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <map>
#include <variant>
#include <functional>

#include "tsf.h"
#include "opl.h"
#include "tml.h"

typedef void PaStream;
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

    ~Player();

    void play();
    void pause();
    void stop();
    bool isPlaying() const;
    void seekTo(unsigned int ms);
    bool loop() const;
    void setLoop(bool loop);
    std::map<enum InfoType, std::variant<int, unsigned int> > midiInfo() const;

    void setVolume(float volume);

    bool loadMidiFile(const char * filePath);
    bool loadSF2File(const char * sf2Path);
    bool loadOP2File(const char * op2Path);

    bool renderToWav(const char * filePath);

    void setPlaybackCallback(std::function<void(unsigned int)> cb);
    void onIsPlayingChanged(std::function<void(bool)> cb);

private:
    Player();

    std::tuple<double, tml_message *> oplRenderToBuffer(float * buffer, tml_message * startMsg, double startMs, int sampleCount);
    std::tuple<double, tml_message *> renderToBuffer(float * buffer, tml_message * startMsg, double startMs, int sampleCount);

    static Player * m_player_instance;
    int streamCallback(const void *inputBuffer, void *outputBuffer, unsigned long numFrames);

    float m_volume = 1;
    bool m_isPlaying = false;
    bool m_loop = true;

    PaStream * m_stream;
    tml_message* m_tinyMidiLoader = NULL;
    tsf* m_tinySoundFont = NULL;
    opl_t* m_opl = NULL;
    tml_message* m_currentPlaybackMessagePos;
    double m_currentPlaybackMSec = 0;
    tml_message* m_seekToMessagePos = NULL;
    double m_seekToMSec = 0;
    std::function<void(unsigned int)> mf_playbackCallback;
    std::function<void(bool)> mf_onIsPlayingChanged;
};
