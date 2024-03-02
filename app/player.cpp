#include "player.h"

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#define TSF_IMPLEMENTATION
#include "tsf.h"
#define TML_IMPLEMENTATION
#include "tml.h"

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
    SDL_PauseAudio(0);
}

void Player::pause()
{
    SDL_PauseAudio(1);
}

void Player::stop()
{
    pause();
    if (m_tinySoundFont != NULL) tsf_note_off_all(m_tinySoundFont);
    m_currentPlaybackMSec = 0;
    if (m_tinyMidiLoader != NULL) m_currentPlaybackMessagePos = m_tinyMidiLoader;
}

bool Player::isPlaying() const
{
    return SDL_GetAudioStatus() == SDL_AUDIO_PLAYING;
}

void Player::seekTo(unsigned int ms)
{
    if (m_tinySoundFont && m_tinyMidiLoader) {
        tsf_note_off_all(m_tinySoundFont);
        m_seekToMSec = ms;
        tml_message* search = m_tinyMidiLoader;
        while (search->next) {
            if (search->time > ms) break;
            search = search->next;
        }
        m_seekToMessagePos = search;
    }
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

    m_tinyMidiLoader = tml_load_filename(filePath);
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
    m_tinySoundFont = tsf_load_filename(sf2Path);
    if (!m_tinySoundFont) {
        fprintf(stderr, "Could not load SoundFont\n");
        return false;
    }

    // Initialize preset on special 10th MIDI channel to use percussion sound bank (128) if available
    tsf_channel_set_bank_preset(m_tinySoundFont, 9, 128, 0);

    // Set the SoundFont rendering output mode
    tsf_set_output(m_tinySoundFont, TSF_STEREO_INTERLEAVED, m_sdlOutputAudioSpec.freq, 0.0f);

    // Init volume
    tsf_set_volume(m_tinySoundFont, m_volume);

    return true;
}

void Player::setPlaybackCallback(std::function<void (unsigned int)> cb)
{
    mf_playbackCallback = cb;
}

Player::Player()
{
    m_sdlOutputAudioSpec.freq = 44100;
    m_sdlOutputAudioSpec.format = AUDIO_F32;
    m_sdlOutputAudioSpec.channels = 2;
    m_sdlOutputAudioSpec.samples = 4096;
    m_sdlOutputAudioSpec.callback = [](void *data, uint8_t *stream, int len){
        Player::instance()->sdl_audioCallback(data, stream, len);
    };

    // Initialize the audio system
    if (SDL_AudioInit(TSF_NULL) < 0) {
        fprintf(stderr, "Could not initialize audio hardware or driver\n");
        exit(1001);
    }

    // Request the desired audio output format
    if (SDL_OpenAudio(&m_sdlOutputAudioSpec, TSF_NULL) < 0) {
        fprintf(stderr, "Could not open the audio hardware or the desired audio output format\n");
        exit(1002);
    }
}

void Player::sdl_audioCallback(void *data, uint8_t *stream, int len)
{
    // Check and apply seek
    if (m_seekToMessagePos) {
        m_currentPlaybackMessagePos = m_seekToMessagePos;
        m_seekToMessagePos = NULL;
        m_currentPlaybackMSec = m_seekToMSec;
        m_seekToMSec = NULL;
    }

    //Number of samples to process
    int SampleBlock, SampleCount = (len / (2 * sizeof(float))); //2 output channels
    for (SampleBlock = TSF_RENDER_EFFECTSAMPLEBLOCK;
         SampleCount;
         SampleCount -= SampleBlock, stream += (SampleBlock * (2 * sizeof(float))))
    {
        //We progress the MIDI playback and then process TSF_RENDER_EFFECTSAMPLEBLOCK samples at once
        if (SampleBlock > SampleCount) SampleBlock = SampleCount;

        //Loop through all MIDI messages which need to be played up until the current playback time
        for (m_currentPlaybackMSec += SampleBlock * (1000.0 / 44100.0);
             m_currentPlaybackMessagePos && m_currentPlaybackMSec >= m_currentPlaybackMessagePos->time;
             m_currentPlaybackMessagePos = m_currentPlaybackMessagePos->next)
        {
            switch (m_currentPlaybackMessagePos->type) {
            case TML_PROGRAM_CHANGE: //channel program (preset) change (special handling for 10th MIDI channel with drums)
                tsf_channel_set_presetnumber(m_tinySoundFont, m_currentPlaybackMessagePos->channel, m_currentPlaybackMessagePos->program, (m_currentPlaybackMessagePos->channel == 9));
                break;
            case TML_NOTE_ON: //play a note
                tsf_channel_note_on(m_tinySoundFont, m_currentPlaybackMessagePos->channel, m_currentPlaybackMessagePos->key, m_currentPlaybackMessagePos->velocity / 127.0f);
                break;
            case TML_NOTE_OFF: //stop a note
                tsf_channel_note_off(m_tinySoundFont, m_currentPlaybackMessagePos->channel, m_currentPlaybackMessagePos->key);
                break;
            case TML_PITCH_BEND: //pitch wheel modification
                tsf_channel_set_pitchwheel(m_tinySoundFont, m_currentPlaybackMessagePos->channel, m_currentPlaybackMessagePos->pitch_bend);
                break;
            case TML_CONTROL_CHANGE: //MIDI controller messages
                tsf_channel_midi_control(m_tinySoundFont, m_currentPlaybackMessagePos->channel, m_currentPlaybackMessagePos->control, m_currentPlaybackMessagePos->control_value);
                break;
            }
        }

        // Render the block of audio samples in float format
        tsf_render_float(m_tinySoundFont, (float*)stream, SampleBlock, 0);
    }

    if (mf_playbackCallback) {
        mf_playbackCallback(m_currentPlaybackMSec);
    }

    // Loop
    if (!m_currentPlaybackMessagePos) {
        m_currentPlaybackMSec = 0;
        m_currentPlaybackMessagePos = m_tinyMidiLoader;
    }
}

