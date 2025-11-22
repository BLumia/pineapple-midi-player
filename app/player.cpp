// SPDX-FileCopyrightText: 2025 Gary Wang <git@blumia.net>
//
// SPDX-License-Identifier: MIT

#include "player.h"

#include <cstdio>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <unordered_set>

#include <portaudio.h>

#include "opl.h"
#include "tsf.h"
#include "dr_wav.h"
#include "midi_parser.h"

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

void Player::onSeek(unsigned int ms)
{
    if (m_tinyMidiLoader) {
        opl_clear(m_opl);
        if (m_tinySoundFont) tsf_note_off_all(m_tinySoundFont);
        tml_message* search = m_tinyMidiLoader;
        while (search->next) {
            if (search->time > ms) break;
            search = search->next;
        }
        m_currentPlaybackMessagePos = search;
        m_currentPlaybackMSec = (double)ms;
    }
}

std::map<Player::InfoType, std::variant<int, unsigned int> > Player::midiInfo() const
{
    std::unordered_set<int> channels;
    std::unordered_set<int> programs;
    int totalNotes = 0;
    unsigned int timeFirstNoteMs = 0;
    bool firstFound = false;
    unsigned int timeLengthMs = 0;

    for (tml_message* Msg = m_tinyMidiLoader; Msg; Msg = Msg->next) {
        timeLengthMs = Msg->time;
        if (Msg->type == pmidi::E_PROGRAM_CHANGE) programs.insert((int)Msg->program);
        if (Msg->type == pmidi::E_NOTE_ON) {
            if (!firstFound) { timeFirstNoteMs = timeLengthMs; firstFound = true; }
            channels.insert((int)Msg->channel);
            totalNotes++;
        }
    }
    if (!firstFound) timeFirstNoteMs = 0;

    std::map<Player::InfoType, std::variant<int, unsigned int>> info;
    info[I_USED_CHANNELS] = (int)channels.size();
    info[I_USED_PROGRAMS] = (int)programs.size();
    info[I_TOTAL_NOTES] = totalNotes;
    info[I_1ST_NOTE_MS] = timeFirstNoteMs;
    info[I_LENGTH_MS] = timeLengthMs;
    return info;
}

bool Player::loadMidiFile(const char *filePath)
{
    stop();

    if (m_tinySoundFont) {
        // Just in case other MIDI file might already channel parameters via e.g. PROGRAM_CHANGE event,
        // let's do a full state reset here but keep the loaded SoundFont.
        tsf_reset(m_tinySoundFont);
        // Re-do the default bank preset for percussion since we just reset the full state which include
        // channel state.
        tsf_channel_set_bank_preset(m_tinySoundFont, 9, 128, 0);
    }

    tml_message * oldFile = m_tinyMidiLoader;
    m_meta = pmidi::MetaBundle();
    m_tinyMidiLoader = pmidi::MidiParser::parseFile(filePath, &m_meta);
    if (oldFile) tml_free(oldFile);

    if (!m_tinyMidiLoader) {
        fprintf(stderr, "Could not load MIDI file\n");
        return false;
    }

    m_currentPlaybackMessagePos = m_tinyMidiLoader;
    return true;
}

const pmidi::MetaBundle& Player::midiMeta() const
{
    return m_meta;
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
    int sr = (m_audioSettings.sampleRate > 0) ? m_audioSettings.sampleRate : SAMPLE_RATE;
    tsf_set_output(m_tinySoundFont, TSF_STEREO_INTERLEAVED, sr, 0.0f);

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

Player::Player() : AbstractPlayer()
{
    m_opl = opl_create();
}

Player::~Player()
{
    opl_destroy(m_opl);
}

std::tuple<double, tml_message *> Player::oplRenderToBuffer(float *buffer, tml_message *startMsg, double startMs, int sampleCount)
{
    const double sr = (m_audioSettings.sampleRate > 0) ? (double)m_audioSettings.sampleRate : (double)SAMPLE_RATE;
    const double playbackEnd = startMs + (sampleCount * (1000.0 / sr));
    double curMs = startMs;
    tml_message * curMsg = startMsg;

    for (; curMsg && curMs >= curMsg->time; curMsg = curMsg->next) {
        switch (curMsg->type) {
        case pmidi::E_PROGRAM_CHANGE:
            opl_midi_changeprog(m_opl, (curMsg->channel % 16), curMsg->program);
            break;
        case pmidi::E_NOTE_ON:
            opl_midi_noteon(m_opl, (curMsg->channel % 16), curMsg->key, curMsg->velocity);
            break;
        case pmidi::E_NOTE_OFF:
            opl_midi_noteoff(m_opl, (curMsg->channel % 16), curMsg->key);
            break;
        case pmidi::E_PITCH_BEND:
            opl_midi_pitchwheel(m_opl, (curMsg->channel % 16), ( curMsg->pitch_bend - 8192 ) / 64 );
            break;
        case pmidi::E_CONTROL_CHANGE:
            opl_midi_controller(m_opl, (curMsg->channel % 16), curMsg->control, curMsg->control_value );
            break;
        }
    }

    // Render the block of audio samples in float format
    short shortBuffer[TSF_RENDER_EFFECTSAMPLEBLOCK * 2];
    opl_render( m_opl, shortBuffer, sampleCount, 1 );
    for (int i = 0; i < sampleCount * 2; i++) {
        buffer[i] = (float) shortBuffer[i] / 32768.0f;
    }

    return std::make_tuple(playbackEnd, curMsg);
}

std::tuple<double, tml_message *> Player::renderToBuffer(float *buffer, tml_message *startMsg, double startMs, int sampleCount)
{
    if (!m_tinySoundFont) return oplRenderToBuffer(buffer, startMsg, startMs, sampleCount);

    const double sr = (m_audioSettings.sampleRate > 0) ? (double)m_audioSettings.sampleRate : (double)SAMPLE_RATE;
    const double playbackEnd = startMs + (sampleCount * (1000.0 / sr));
    double curMs = startMs;
    tml_message * curMsg = startMsg;

    for (; curMsg && curMs >= curMsg->time; curMsg = curMsg->next) {
        switch (curMsg->type) {
        case pmidi::E_PROGRAM_CHANGE:
            tsf_channel_set_presetnumber(m_tinySoundFont, curMsg->channel, curMsg->program, ((curMsg->channel % 16) == 9));
            break;
        case pmidi::E_NOTE_ON: //play a note
            tsf_channel_note_on(m_tinySoundFont, curMsg->channel, curMsg->key, curMsg->velocity / 127.0f);
            break;
        case pmidi::E_NOTE_OFF: //stop a note
            tsf_channel_note_off(m_tinySoundFont, curMsg->channel, curMsg->key);
            break;
        case pmidi::E_PITCH_BEND: //pitch wheel modification
            tsf_channel_set_pitchwheel(m_tinySoundFont, curMsg->channel, curMsg->pitch_bend);
            break;
        case pmidi::E_CONTROL_CHANGE: //MIDI controller messages
            tsf_channel_midi_control(m_tinySoundFont, curMsg->channel, curMsg->control, curMsg->control_value);
            break;
        }
    }

    // Render the block of audio samples in float format
    tsf_render_float(m_tinySoundFont, buffer, sampleCount, 0);

    return std::make_tuple(playbackEnd, curMsg);
}

void Player::onStop()
{
    opl_clear(m_opl);
    if (m_tinySoundFont != NULL)
        tsf_note_off_all(m_tinySoundFont);
    m_currentPlaybackMSec = 0;
    if (m_tinyMidiLoader != NULL)
        m_currentPlaybackMessagePos = m_tinyMidiLoader;
}

void Player::seekTo(unsigned int ms)
{
    if (m_tinyMidiLoader) {
        opl_clear(m_opl);
        if (m_tinySoundFont)
            tsf_note_off_all(m_tinySoundFont);
        m_seekToMSec.store(ms);
        tml_message *search = m_tinyMidiLoader;
        while (search->next) {
            if (search->time > ms)
                break;
            search = search->next;
        }
        m_seekToMessagePos.store(search);
    }
}

unsigned int Player::currentPlaybackPositionMs() const
{
    return m_uiPlaybackMs.load();
}

void Player::renderAudio(float *buffer, unsigned long numFrames)
{
    if (!m_tinyMidiLoader) {
        std::fill(buffer, buffer + numFrames * 2, 0);
        return;
    }

    std::uint8_t * byteBuffer = (std::uint8_t *)buffer;
    // Check and apply seek
    if (m_seekToMessagePos.load() != NULL) {
        tml_message* target = m_seekToMessagePos.exchange(NULL);
        unsigned int targetMs = m_seekToMSec.exchange(0);
        m_currentPlaybackMessagePos = target;
        m_currentPlaybackMSec = (double)targetMs;
    }

    // Number of samples to process
    int SampleBlock, SampleCount = numFrames;
    for (SampleBlock = TSF_RENDER_EFFECTSAMPLEBLOCK; SampleCount;
         SampleCount -= SampleBlock, byteBuffer += (SampleBlock * (2 * sizeof(float)))) {
        // We progress the MIDI playback and then process TSF_RENDER_EFFECTSAMPLEBLOCK samples at once
        if (SampleBlock > SampleCount)
            SampleBlock = SampleCount;

        std::tie(m_currentPlaybackMSec, m_currentPlaybackMessagePos) =
                renderToBuffer((float *)byteBuffer, m_currentPlaybackMessagePos,
                               m_currentPlaybackMSec, SampleBlock);
    }

    if (!m_currentPlaybackMessagePos) {
        m_currentPlaybackMSec = 0;
        m_currentPlaybackMessagePos = m_tinyMidiLoader;
        if (!m_loop.load()) {
            m_isPlaying.store(false);
        }
    }
    m_uiPlaybackMs.store((unsigned int)m_currentPlaybackMSec);
}

bool Player::applyAudioSettings(const AudioSettings &settings)
{
    bool result = AbstractPlayer::applyAudioSettings(settings);
    if (result && m_tinySoundFont) {
        int sr = (m_audioSettings.sampleRate > 0) ? m_audioSettings.sampleRate : SAMPLE_RATE;
        tsf_set_output(m_tinySoundFont, TSF_STEREO_INTERLEAVED, sr, 0.0f);
        fprintf(stderr, "TSF output reconfigured to sr=%d\n", sr);
    }
    return result;
}
