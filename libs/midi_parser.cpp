// SPDX-FileCopyrightText: 2025 Gary Wang <git@blumia.net>
//
// SPDX-License-Identifier: MIT

#include "midi_parser.h"
#include <fstream>
#include <algorithm>
#include <cstring>

namespace pmidi {

static uint32_t read_be32(const uint8_t* p) { return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | (uint32_t)p[3]; }
static uint16_t read_be16(const uint8_t* p) { return (uint16_t)p[0]<<8 | (uint16_t)p[1]; }

static int read_vlq(const uint8_t* buf, int size, int& off) {
    uint32_t res = 0;
    int i = 0;
    for (; i < 4 && off < size; i++) {
        uint8_t c = buf[off++];
        if (c & 0x80) res = ((res | (c & 0x7F)) << 7);
        else return (int)(res | c);
    }
    return -1;
}

struct TrackCtx {
    const uint8_t* data;
    int size;
    int off;
    int last_status;
    uint32_t ticks;
    uint8_t current_port;
};

static std::string bytes_to_string(const uint8_t* p, int len) { return std::string((const char*)p, (size_t)len); }

static void parse_tracks(const std::vector<uint8_t>& file, int num_tracks, int division, MidiSong& song, MetaBundle* meta) {
    int off = 14;
    song.division = division;
    song.events.clear();

    for (int t = 0; t < num_tracks; ++t) {
        if (off + 8 > (int)file.size()) break;
        const uint8_t* th = &file[off];
        if (th[0] != 'M' || th[1] != 'T' || th[2] != 'r' || th[3] != 'k') break;
        int track_len = (int)read_be32(th+4);
        off += 8;
        if (off + track_len > (int)file.size()) break;
        TrackCtx ctx{&file[off], track_len, 0, 0, 0, 0};
        if (meta) { if ((int)meta->tracks.size() < t+1) meta->tracks.resize(t+1); }
        while (ctx.off < ctx.size) {
            int dt = read_vlq(ctx.data, ctx.size, ctx.off);
            if (dt < 0) { /* malformed VLQ */ break; }
            ctx.ticks += (uint32_t)dt;
            if (ctx.off >= ctx.size) break;
            int status = ctx.data[ctx.off++];
            if ((status & 0x80) == 0) {
                if ((ctx.last_status & 0x80) == 0) break;
                ctx.off--;
                status = ctx.last_status;
            } else ctx.last_status = status;
            if (status == 0xFF) {
                if (ctx.off >= ctx.size) break;
                int meta_type = ctx.data[ctx.off++];
                int len = read_vlq(ctx.data, ctx.size, ctx.off);
                if (len < 0 || ctx.off + len > ctx.size) { /* invalid meta length */ break; }
                const uint8_t* metadata = ctx.data + ctx.off;
                if (meta_type == ME_TEMPO && len == 3) {
                    uint32_t tempo = (uint32_t)metadata[0]<<16 | (uint32_t)metadata[1]<<8 | (uint32_t)metadata[2];
                    song.tempo_changes.push_back({ctx.ticks, tempo});
                } else if (meta_type == ME_MIDI_PORT && len == 1) {
                    ctx.current_port = metadata[0];
                } else if (meta && meta_type == ME_TRACK_NAME) {
                    meta->tracks[t].name = bytes_to_string(metadata, len);
                } else if (meta && meta_type == ME_INSTRUMENT_NAME) {
                    meta->tracks[t].instrument = bytes_to_string(metadata, len);
                } else if (meta && meta_type == ME_MARKER) {
                    meta->markers.emplace_back(ctx.ticks, bytes_to_string(metadata, len));
                } else if (meta && meta_type == ME_LYRIC) {
                    meta->lyrics.emplace_back(ctx.ticks, bytes_to_string(metadata, len));
                } else if (meta && meta_type == ME_COPYRIGHT) {
                    meta->copyright = bytes_to_string(metadata, len);
                } else if (meta && meta_type == ME_TIME_SIGNATURE && len >= 4) {
                    meta->timesig_n = metadata[0];
                    meta->timesig_d_exp = metadata[1];
                    meta->timesig_clocks = metadata[2];
                    meta->timesig_thirtyseconds = metadata[3];
                } else if (meta && meta_type == ME_KEY_SIGNATURE && len >= 2) {
                    meta->keysig = (int8_t)metadata[0];
                    meta->keysig_mode = metadata[1];
                }
                ctx.off += len;
                continue;
            }
            if (status == 0xF0 || status == 0xF7) {
                int len = read_vlq(ctx.data, ctx.size, ctx.off);
                if (len < 0 || ctx.off + len > ctx.size) { /* invalid sysex length */ break; }
                ctx.off += len;
                continue;
            }
            int param1 = -1, param2 = 0;
            if (ctx.off >= ctx.size) break;
            param1 = ctx.data[ctx.off++] & 0x7F;
            uint8_t type = (uint8_t)(status & 0xF0);
            uint8_t ch = (uint8_t)(status & 0x0F);
            uint16_t mapped_ch = (uint16_t)(((uint16_t)ctx.current_port << 4) + ch);
            MidiEventRaw ev{};
            ev.tick = ctx.ticks;
            ev.type = type;
            ev.channel = mapped_ch;
            if (type == E_PITCH_BEND) {
                if (ctx.off >= ctx.size) break;
                param2 = ctx.data[ctx.off++] & 0x7F;
                ev.key = (uint8_t)param1;
                ev.pitch_bend = (uint16_t)(((param2 & 0x7F) << 7) | (param1 & 0x7F));
                song.events.push_back(ev);
            } else if (type == E_PROGRAM_CHANGE || type == E_CHANNEL_PRESSURE) {
                ev.key = (uint8_t)param1;
                ev.velocity = 0;
                song.events.push_back(ev);
            } else if (type == E_NOTE_ON || type == E_NOTE_OFF || type == E_POLYPHONIC_KEY_PRESSURE || type == E_CONTROL_CHANGE) {
                if (ctx.off >= ctx.size) break;
                param2 = ctx.data[ctx.off++] & 0x7F;
                ev.key = (uint8_t)param1;
                ev.velocity = (uint8_t)param2;
                if (type == E_CONTROL_CHANGE) { ev.control = ev.key; ev.control_value = ev.velocity; }
                song.events.push_back(ev);
            } else {
            }
        }
        off += track_len;
    }
    std::sort(song.tempo_changes.begin(), song.tempo_changes.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
}

static int tick_to_ms(const MidiSong& song, uint32_t tick) {
    double ticks2time = 500000.0 / (1000.0 * song.division);
    uint32_t tempo_ticks = 0;
    int tempo_msec = 0;
    for (size_t i = 0; i < song.tempo_changes.size(); ++i) {
        if (song.tempo_changes[i].first > tick) break;
        int msec = tempo_msec + (int)((song.tempo_changes[i].first - tempo_ticks) * ticks2time);
        tempo_msec = msec;
        tempo_ticks = song.tempo_changes[i].first;
        ticks2time = (double)song.tempo_changes[i].second / (1000.0 * song.division);
    }
    int msec = tempo_msec + (int)((tick - tempo_ticks) * ticks2time);
    return msec;
}

static tml_message* build_messages(const MidiSong& song, MetaBundle* meta) {
    if (song.events.empty()) return nullptr;
    std::vector<MidiEventRaw> events = song.events;
    std::stable_sort(events.begin(), events.end(), [](const MidiEventRaw& a, const MidiEventRaw& b){ return a.tick < b.tick; });
    double ticks2time = 500000.0 / (1000.0 * song.division);
    uint32_t tempo_ticks = 0;
    int tempo_msec = 0;
    size_t tempo_idx = 0;
    tml_message* arr = (tml_message*)malloc(sizeof(tml_message) * events.size());
    for (size_t i = 0; i < events.size(); ++i) {
        const auto& e = events[i];
        while (tempo_idx < song.tempo_changes.size() && song.tempo_changes[tempo_idx].first <= e.tick) {
            double t = (double)song.tempo_changes[tempo_idx].second;
            int delta_ticks = song.tempo_changes[tempo_idx].first - tempo_ticks;
            int msec_to_add = (int)(delta_ticks * ticks2time);
            tempo_msec += msec_to_add;
            tempo_ticks = song.tempo_changes[tempo_idx].first;
            ticks2time = t / (1000.0 * song.division);
            tempo_idx++;
        }
        int msec = tempo_msec + (int)((e.tick - tempo_ticks) * ticks2time);
        arr[i].time = (unsigned int)msec;
        arr[i].type = e.type;
        arr[i].channel = e.channel;
        if (e.type == E_NOTE_ON || e.type == E_NOTE_OFF) { arr[i].key = (char)e.key; arr[i].velocity = (char)e.velocity; }
        else if (e.type == E_POLYPHONIC_KEY_PRESSURE) { arr[i].key = (char)e.key; arr[i].key_pressure = (char)e.velocity; }
        else if (e.type == E_CONTROL_CHANGE) { arr[i].control = (char)e.control; arr[i].control_value = (char)e.control_value; }
        else if (e.type == E_PROGRAM_CHANGE) { arr[i].program = (char)e.key; }
        else if (e.type == E_CHANNEL_PRESSURE) { arr[i].channel_pressure = (char)e.key; }
        else if (e.type == E_PITCH_BEND) { arr[i].pitch_bend = e.pitch_bend; }
        arr[i].next = (i + 1 < events.size()) ? &arr[i+1] : nullptr;
    }
    if (meta) {
        for (auto& mk : meta->markers) mk.first = (uint32_t)tick_to_ms(song, mk.first);
        for (auto& ly : meta->lyrics) ly.first = (uint32_t)tick_to_ms(song, ly.first);
    }
    return arr;
}

tml_message* MidiParser::parseFile(const char* path, MetaBundle* outMeta) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return nullptr;
    f.seekg(0, std::ios::end);
    std::streamsize sz = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf;
    buf.resize((size_t)sz);
    if (!f.read((char*)buf.data(), sz)) return nullptr;
    if (buf.size() < 14) return nullptr;
    if (buf[0] != 'M' || buf[1] != 'T' || buf[2] != 'h' || buf[3] != 'd') return nullptr;
    if (buf[7] != 6) return nullptr;
    int num_tracks = (int)read_be16(&buf[10]);
    int division = (int)read_be16(&buf[12]);
    if ((division & 0x8000) != 0) return nullptr;
    MidiSong song;
    parse_tracks(buf, num_tracks, division, song, outMeta);
    return build_messages(song, outMeta);
}

}
