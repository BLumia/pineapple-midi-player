// SPDX-FileCopyrightText: 2025 Gary Wang <git@blumia.net>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <map>
#include <variant>
#include <functional>
#include <atomic>

#include "abstractplayer.h"
#include "tsf.h"
#include "opl.h"
#include "midi_parser.h"

class Player : public AbstractPlayer
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

    void seekTo(unsigned int ms);
    std::map<enum InfoType, std::variant<int, unsigned int> > midiInfo() const;
    const pmidi::MetaBundle& midiMeta() const;
    unsigned int currentPlaybackPositionMs() const;
    StreamState applyAudioSettings(const AudioSettings &settings);

    bool loadMidiFile(const char * filePath);
    bool loadSF2File(const char * sf2Path);
    bool loadOP2File(const char * op2Path);

    bool renderToWav(const char * filePath);

protected:
    // Virtual function overrides
    void renderAudio(float *buffer, unsigned long numFrames) override;
    void onStop() override;
    void onSeek(unsigned int ms) override;

private:
    Player();

    std::tuple<double, tml_message *> oplRenderToBuffer(float * buffer, tml_message * startMsg, double startMs, int sampleCount);
    std::tuple<double, tml_message *> renderToBuffer(float * buffer, tml_message * startMsg, double startMs, int sampleCount);

    static Player * m_player_instance;

    tml_message* m_tinyMidiLoader = NULL;
    pmidi::MetaBundle m_meta;
    tsf* m_tinySoundFont = NULL;
    opl_t* m_opl = NULL;
    tml_message* m_currentPlaybackMessagePos;
    double m_currentPlaybackMSec = 0;
    std::atomic<tml_message*> m_seekToMessagePos{NULL};
    std::atomic<unsigned int> m_seekToMSec{0};
    std::atomic<unsigned int> m_uiPlaybackMs{0};
};
