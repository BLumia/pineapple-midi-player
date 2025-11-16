// https://github.com/nothings/stb
// revision: ae721c50eaf761660b4f90cc590453cdb0c2acd0
// required by TinySoundFont to support SF3
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
// https://github.com/schellingb/TinySoundFont
// revision: 21f84301f8aea16e06c2cd2e777383a2aae71732
#define TSF_IMPLEMENTATION
#include "tsf.h"
// https://github.com/mackron/dr_libs
// revision: da35f9d6c7374a95353fd1df1d394d44ab66cf01
// only required by the wav file renderer part
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
// https://github.com/mattiasgustavsson/dos-like
// revision: a15e72402c99ed507ef48a5efebb390cc4c2dd65
// maybe this can become an easter egg ;p
#define OPL_IMPLEMENTATION
#include "opl.h"
