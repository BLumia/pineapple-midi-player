// SPDX-FileCopyrightText: 2025 Gary Wang <git@blumia.net>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <vector>
#include <cstdint>
#include <string>

// Data types and functions starts with tml_ are compatibility API for older tml.h API usage.
typedef struct tml_message
{
    unsigned int time;
    unsigned char type; 
    unsigned short channel;
    union
    {
        struct { union { char key, control, program, channel_pressure; }; union { char velocity, key_pressure, control_value; }; };
        struct { unsigned short pitch_bend; };
    };
    struct tml_message* next;
} tml_message;

static inline void tml_free(tml_message* f) { if (f) free(f); }

namespace pmidi {

enum MidiEventType {
    E_NOTE_OFF = 0x80,
    E_NOTE_ON = 0x90,
    E_POLYPHONIC_KEY_PRESSURE = 0xA0,
    E_CONTROL_CHANGE = 0xB0,
    E_PROGRAM_CHANGE = 0xC0,
    E_CHANNEL_PRESSURE = 0xD0,
    E_PITCH_BEND = 0xE0,
};

// https://www.mixagesoftware.com/en/midikit/help/HTML/meta_events.html
enum MetaEventType {
    ME_COPYRIGHT = 0x02,
    ME_TRACK_NAME = 0x03,
    ME_INSTRUMENT_NAME = 0x04,
    ME_LYRIC = 0x05,
    ME_MARKER = 0x06,
    ME_TEMPO = 0x51,
    ME_TIME_SIGNATURE = 0x58,
    ME_KEY_SIGNATURE = 0x59,
    // Deprecated, but still used by many existing MIDI files.
    ME_MIDI_PORT = 0x21, // VGMTrans is using MIDI Port event.
};

struct MidiEventRaw {
    uint32_t tick;
    uint8_t type;
    uint16_t channel;
    uint8_t key;
    uint8_t velocity;
    uint16_t pitch_bend;
    uint8_t control;
    uint8_t control_value;
};

struct TrackMeta {
    std::string name;
    std::string instrument;
};

struct MetaBundle {
    std::vector<TrackMeta> tracks;
    std::vector<std::pair<uint32_t,std::string>> markers;
    std::vector<std::pair<uint32_t,std::string>> lyrics;
    std::string copyright;
    uint8_t timesig_n = 0;
    uint8_t timesig_d_exp = 0;
    uint8_t timesig_clocks = 0;
    uint8_t timesig_thirtyseconds = 0;
    int8_t keysig = 0;
    uint8_t keysig_mode = 0; // 0=major, 1=minor
};

struct MidiSong {
    int division;
    std::vector<MidiEventRaw> events;
    std::vector<uint32_t> tempo_ticks;
    std::vector<uint32_t> tempo_values;
};

class MidiParser {
public:
    static tml_message* parseFile(const char* path, MetaBundle* meta = nullptr);
};

}