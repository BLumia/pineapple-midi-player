#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include "tml.h"

namespace pmidi {

struct MidiEventRaw {
    uint32_t tick;
    uint8_t type;
    uint8_t channel;
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

struct MidiSong {
    int division;
    std::vector<MidiEventRaw> events;
    std::vector<uint32_t> tempo_ticks;
    std::vector<uint32_t> tempo_values;
};

class MidiParser {
public:
    static tml_message* parseFile(const char* path);
};

}