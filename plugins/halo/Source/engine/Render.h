#pragma once

#include "Composer.h"

namespace halo
{

enum class Style { natural, falsetto, chest, breathy, whisper, shout, count };
enum class VoiceType { same, higher, lower, chipmunk, demon, count };

const char* styleName (Style);
const char* voiceTypeName (VoiceType);

/** A harmony line turned into a new performance: own scoops, glides, vibrato, drift and a smooth timing warp. */
struct Performance
{
    std::vector<double> f0;        // Hz per output frame (0 = unvoiced)
    std::vector<double> srcFrame;  // fractional source frame for sp/ap per output frame
};

struct PerformOptions
{
    double humanize = 0.6;         // 0 = copy the lead's expression, 1 = fully independent take
    double delayMs = 10.0;         // constant lag behind the lead
    double vibratoScale = 1.0;
    double detuneCents = 0.0;      // fixed offset (choir spread)
    Style style = Style::natural;
    unsigned long long seed = 1;
};

Performance perform (const Analysis&, const std::vector<HarmonyNote>&, const PerformOptions&);

/** WORLD re-synthesis of the voice with the new performance (FAST quality). Returns mono at an.sampleRate. */
std::vector<float> renderWorld (const Analysis&, const Performance&, double meanShiftSemis, Style, VoiceType,
                                unsigned long long noiseSeed = 1);

} // namespace halo
