// SPDX-FileCopyrightText: 2024 Gary Wang <git@blumia.net>
//
// SPDX-License-Identifier: MIT

#include "player.h"

#include <cstdio>
#include <cstdint>
#include <iostream>
#include <fstream>

#include <portaudio.h>

#include "opl.h"
#include "tsf.h"
#include "tml.h"
#include "dr_wav.h"

#ifndef TSF_RENDER_EFFECTSAMPLEBLOCK
#define TSF_RENDER_EFFECTSAMPLEBLOCK 64
#endif

constexpr std::int32_t SAMPLE_RATE = 44100;

Player * Player::m_player_instance = nullptr;

Player * Player::instance()
{
    if (!m_player_instance) {
        m_player_instance = new Player;
    }

    return m_player_instance;
}

void Player::play()
{
    PaError ret = Pa_IsStreamActive(m_stream);
    if (ret == 0) {
        fputs("Playback stream stopped, restarting...", stderr);
        bool succ = setupAndStartStream();
        if (!succ) {
            fputs("Not able to restart playback stream", stderr);
            return;
        }
    }
    m_isPlaying = true;
    if (mf_onIsPlayingChanged) mf_onIsPlayingChanged(m_isPlaying);
}

void Player::pause()
{
    m_isPlaying = false;
    if (mf_onIsPlayingChanged) mf_onIsPlayingChanged(m_isPlaying);
}

void Player::stop()
{
    pause();

    opl_clear(m_opl);
    if (m_tinySoundFont != NULL) tsf_note_off_all(m_tinySoundFont);
    m_currentPlaybackMSec = 0;
    if (m_tinyMidiLoader != NULL) m_currentPlaybackMessagePos = m_tinyMidiLoader;
}

bool Player::isPlaying() const
{
    return m_isPlaying;
}

void Player::seekTo(unsigned int ms)
{
    if (m_tinyMidiLoader) {
        opl_clear(m_opl);
        if (m_tinySoundFont) tsf_note_off_all(m_tinySoundFont);
        m_seekToMSec = ms;
        tml_message* search = m_tinyMidiLoader;
        while (search->next) {
            if (search->time > ms) break;
            search = search->next;
        }
        m_seekToMessagePos = search;
    }
}

bool Player::loop() const
{
    return m_loop;
}

void Player::setLoop(bool loop)
{
    m_loop = loop;
}

std::map<Player::InfoType, std::variant<int, unsigned int> > Player::midiInfo() const
{
    int usedChannels, usedPrograms, totalNotes;
    unsigned int timeFirstNoteMs, timeLengthMs;
    tml_get_info(m_tinyMidiLoader, &usedChannels, &usedPrograms, &totalNotes, &timeFirstNoteMs, &timeLengthMs);

    std::map<Player::InfoType, std::variant<int, unsigned int>> info;
    info[I_USED_CHANNELS] = usedChannels;
    info[I_USED_PROGRAMS] = usedPrograms;
    info[I_TOTAL_NOTES] = totalNotes;
    info[I_1ST_NOTE_MS] = timeFirstNoteMs;
    info[I_LENGTH_MS] = timeLengthMs;

    return info;
}

void Player::setVolume(float volume)
{
    m_volume = volume;

    if (m_tinySoundFont) tsf_set_volume(m_tinySoundFont, m_volume);
}

bool Player::loadMidiFile(const char *filePath)
{
    stop();

    tml_message * oldFile = m_tinyMidiLoader;
    m_tinyMidiLoader = tml_load_filename(filePath);
    if (oldFile) tml_free(oldFile);

    if (!m_tinyMidiLoader) {
        fprintf(stderr, "Could not load MIDI file\n");
        return false;
    }

    m_currentPlaybackMessagePos = m_tinyMidiLoader;
    return true;
}

bool Player::loadSF2File(const char *sf2Path)
{
    // Load the SoundFont from a file
    tsf * oldFile = m_tinySoundFont;
    m_tinySoundFont = tsf_load_filename(sf2Path);
    if (oldFile) tsf_close(oldFile);

    if (!m_tinySoundFont) {
        fprintf(stderr, "Could not load SoundFont\n");
        return false;
    }

    // Initialize preset on special 10th MIDI channel to use percussion sound bank (128) if available
    tsf_channel_set_bank_preset(m_tinySoundFont, 9, 128, 0);

    // Set the SoundFont rendering output mode
    tsf_set_output(m_tinySoundFont, TSF_STEREO_INTERLEAVED, SAMPLE_RATE, 0.0f);

    // Init volume
    tsf_set_volume(m_tinySoundFont, m_volume);

    return true;
}

bool Player::loadOP2File(const char *op2Path)
{
    std::ifstream file(op2Path, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    char * fileBuffer = new char[size];
    file.read(fileBuffer, size);
    int ret = opl_loadbank_op2(m_opl, fileBuffer, size);
    free(fileBuffer);
    return ret == 0;
}

bool Player::renderToWav(const char *filePath)
{
    drwav_data_format format;
    drwav wav;

    format.container     = drwav_container_riff;
    format.format        = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels      = 2;
    format.sampleRate    = SAMPLE_RATE;
    format.bitsPerSample = 32;

    if (!drwav_init_file_write(&wav, filePath, &format, NULL)) {
        printf("Failed to open file.\n");
        return false;
    }

    auto info = midiInfo();
    const unsigned int durationMs = std::get<unsigned int>(info[Player::I_LENGTH_MS]);

    float buffer[TSF_RENDER_EFFECTSAMPLEBLOCK * 2 * sizeof(float)];

    tml_message * curMsg = m_tinyMidiLoader;
    double curMs = 0;
    while (curMs <= durationMs) {
        std::tie(curMs, curMsg) = renderToBuffer((float *)buffer, curMsg, curMs, TSF_RENDER_EFFECTSAMPLEBLOCK);
        drwav_write_pcm_frames(&wav, TSF_RENDER_EFFECTSAMPLEBLOCK, buffer);
    }

    drwav_uninit(&wav);
    return true;
}

void Player::setPlaybackCallback(std::function<void (unsigned int)> cb)
{
    mf_playbackCallback = cb;
}

void Player::onIsPlayingChanged(std::function<void (bool)> cb)
{
    mf_onIsPlayingChanged = cb;
}

Player::Player()
{
    m_opl = opl_create();

    Pa_Initialize();
    setupAndStartStream();
}

Player::~Player()
{
    Pa_StopStream(m_stream);

    opl_destroy(m_opl);
}

std::tuple<double, tml_message *> Player::oplRenderToBuffer(float *buffer, tml_message *startMsg, double startMs, int sampleCount)
{
    const double playbackEnd = startMs + (sampleCount * (1000.0 / SAMPLE_RATE));
    double curMs = startMs;
    tml_message * curMsg = startMsg;

    for (; curMsg && curMs >= curMsg->time; curMsg = curMsg->next) {
        switch (curMsg->type) {
        case TML_PROGRAM_CHANGE: //channel program (preset) change (special handling for 10th MIDI channel with drums)
            opl_midi_changeprog(m_opl, curMsg->channel, curMsg->program);
            break;
        case TML_NOTE_ON: //play a note
            opl_midi_noteon(m_opl, curMsg->channel, curMsg->key, curMsg->velocity);
            break;
        case TML_NOTE_OFF: //stop a note
            opl_midi_noteoff(m_opl, curMsg->channel, curMsg->key);
            break;
        case TML_PITCH_BEND: //pitch wheel modification
            opl_midi_pitchwheel(m_opl, curMsg->channel, ( curMsg->pitch_bend - 8192 ) / 64 );
            break;
        case TML_CONTROL_CHANGE: //MIDI controller messages
            opl_midi_controller(m_opl, curMsg->channel, curMsg->control, curMsg->control_value );
            break;
        }
    }

    // Render the block of audio samples in float format
    int len = sampleCount * 2 * sizeof(short);
    void * tmpBuffer = malloc(len);
    short * shortBuffer = (short *)tmpBuffer;
    opl_render( m_opl, shortBuffer, sampleCount, 1 );
    for (int i = 0; i < sampleCount * 2; i++) {
        buffer[i] = (float) shortBuffer[i] / 32768.0f;
    }
    free(tmpBuffer);

    return std::make_tuple(playbackEnd, curMsg);
}

std::tuple<double, tml_message *> Player::renderToBuffer(float *buffer, tml_message *startMsg, double startMs, int sampleCount)
{
    if (!m_tinySoundFont) return oplRenderToBuffer(buffer, startMsg, startMs, sampleCount);

    const double playbackEnd = startMs + (sampleCount * (1000.0 / SAMPLE_RATE));
    double curMs = startMs;
    tml_message * curMsg = startMsg;

    for (; curMsg && curMs >= curMsg->time; curMsg = curMsg->next) {
        switch (curMsg->type) {
        case TML_PROGRAM_CHANGE: //channel program (preset) change (special handling for 10th MIDI channel with drums)
            tsf_channel_set_presetnumber(m_tinySoundFont, curMsg->channel, curMsg->program, (curMsg->channel == 9));
            break;
        case TML_NOTE_ON: //play a note
            tsf_channel_note_on(m_tinySoundFont, curMsg->channel, curMsg->key, curMsg->velocity / 127.0f);
            break;
        case TML_NOTE_OFF: //stop a note
            tsf_channel_note_off(m_tinySoundFont, curMsg->channel, curMsg->key);
            break;
        case TML_PITCH_BEND: //pitch wheel modification
            tsf_channel_set_pitchwheel(m_tinySoundFont, curMsg->channel, curMsg->pitch_bend);
            break;
        case TML_CONTROL_CHANGE: //MIDI controller messages
            tsf_channel_midi_control(m_tinySoundFont, curMsg->channel, curMsg->control, curMsg->control_value);
            break;
        }
    }

    // Render the block of audio samples in float format
    tsf_render_float(m_tinySoundFont, buffer, sampleCount, 0);

    return std::make_tuple(playbackEnd, curMsg);
}

bool Player::setupAndStartStream()
{
    Pa_OpenDefaultStream(&m_stream, 0, 2, paFloat32, SAMPLE_RATE, paFramesPerBufferUnspecified,
                         +[](const void *inputBuffer, void *outputBuffer,
                             unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo,
                             PaStreamCallbackFlags statusFlags, void *userData) -> int {
                             return ((Player *)userData)->streamCallback(inputBuffer, outputBuffer, framesPerBuffer);
                         }, this);
    PaError err = Pa_StartStream(m_stream);
    if (err != paNoError) {
        // TODO: logging?
        return false;
    }
    return true;
}

int Player::streamCallback(const void *inputBuffer, void *outputBuffer, unsigned long numFrames)
{
    if (m_tinyMidiLoader && m_isPlaying) {
        std::uint8_t * buffer = (std::uint8_t *)outputBuffer;
        // Check and apply seek
        if (m_seekToMessagePos) {
            m_currentPlaybackMessagePos = m_seekToMessagePos;
            m_seekToMessagePos = NULL;
            m_currentPlaybackMSec = m_seekToMSec;
            m_seekToMSec = 0;
        }

        //Number of samples to process
        int SampleBlock, SampleCount = numFrames;
        for (SampleBlock = TSF_RENDER_EFFECTSAMPLEBLOCK;
             SampleCount;
             SampleCount -= SampleBlock, buffer += (SampleBlock * (2 * sizeof(float))))
        {
            //We progress the MIDI playback and then process TSF_RENDER_EFFECTSAMPLEBLOCK samples at once
            if (SampleBlock > SampleCount) SampleBlock = SampleCount;

            std::tie(m_currentPlaybackMSec, m_currentPlaybackMessagePos)
                = renderToBuffer((float*)buffer, m_currentPlaybackMessagePos, m_currentPlaybackMSec, SampleBlock);
        }

        if (!m_currentPlaybackMessagePos) {
            m_currentPlaybackMSec = 0;
            m_currentPlaybackMessagePos = m_tinyMidiLoader;
            if (!m_loop) {
                pause();
            }
        }

        if (mf_playbackCallback) {
            mf_playbackCallback(m_currentPlaybackMSec);
        }
    } else {
        float* buffer = (float*)outputBuffer;
        std::fill(buffer, buffer + numFrames * 2, 0);
    }

    return 0;
}

