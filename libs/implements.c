// https://github.com/nothings/stb
// revision: ae721c50eaf761660b4f90cc590453cdb0c2acd0
// required by TinySoundFont to support SF3
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
// https://github.com/schellingb/TinySoundFont
// revision: 92a8f0e9fe3c98358be7d8564db21fc4b1142d04
#define TSF_IMPLEMENTATION
#include "tsf.h"
// TinyMidiLoader is a part of the TinySoundFont project
#define TML_IMPLEMENTATION
#include "tml.h"
// https://github.com/mackron/dr_libs
// revision: da35f9d6c7374a95353fd1df1d394d44ab66cf01
// only required by the wav file renderer part
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
