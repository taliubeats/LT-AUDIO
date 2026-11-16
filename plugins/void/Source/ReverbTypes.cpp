#include "ReverbTypes.h"

namespace lt_void
{

const juce::StringArray& TypeInfo::names()
{
    static const juce::StringArray n { "CLOUD", "PLATE", "HALL", "ROOM", "CHAMBER", "VINTAGE",
                                       "SPRING", "SHIMMER", "REVERSE", "GATED", "LO-FI", "VOID" };
    return n;
}

juce::String TypeInfo::description (int t)
{
    static const char* d[] = {
        "huge atmospheric infinite space, slow bloom, lush deep modulation",
        "bright dense metal plate, instant smooth tail, perfect for vocals and snares",
        "large concert hall, natural early reflections, wide and deep",
        "small real room, tight early reflections, puts sounds in a space",
        "classic echo chamber, warm and dense, vintage studio character",
        "80s digital rack reverb, grainy 12-bit tail with chorus shimmer",
        "spring tank, metallic drip and boing, dub and surf",
        "octave-up shimmer, heavenly rising pads",
        "reverse reverb, the tail swells in before every sound",
        "80s gated reverb, big burst then a hard stop",
        "dusty lo-fi space, crushed, filtered, with tape hiss",
        "the void: endless dark monochrome space",
    };
    return d[juce::jlimit (0, count - 1, t)];
}

TypeCharacter TypeInfo::character (int t)
{
    TypeCharacter c;
    switch ((ReverbType) t)
    {
        case ReverbType::cloud:    c.sizeScale = 1.45f; c.modScale = 1.6f; c.bloom = 1.6f; break;
        case ReverbType::plate:    c.sizeScale = 0.55f; c.modScale = 0.6f; c.bloom = 0.2f; break;
        case ReverbType::hall:     c.sizeScale = 1.05f; c.erGain = 0.35f; c.erSpread = 1.0f; c.modScale = 0.8f; break;
        case ReverbType::room:     c.sizeScale = 0.38f; c.erGain = 0.6f; c.erSpread = 0.35f; c.modScale = 0.4f; c.bloom = 0.3f; break;
        case ReverbType::chamber:  c.sizeScale = 0.62f; c.erGain = 0.45f; c.erSpread = 0.55f; c.modScale = 0.6f; c.maxDamp = 9000.0f; break;
        case ReverbType::vintage:  c.sizeScale = 0.85f; c.modScale = 2.2f; c.crush = 1; c.maxDamp = 9500.0f; break;
        case ReverbType::spring:   c.sizeScale = 0.32f; c.modScale = 0.3f; c.dispersion = 0.72f; c.loopLowCut = 140.0f; c.maxDamp = 5500.0f; c.bloom = 0.1f; break;
        case ReverbType::shimmer:  c.sizeScale = 1.3f; c.modScale = 1.2f; c.shimmer = 0.42f; c.bloom = 1.3f; break;
        case ReverbType::reverse:  c.sizeScale = 1.0f; c.modScale = 0.8f; c.reverse = true; c.outputGain = 2.0f; break;
        case ReverbType::gated:    c.sizeScale = 0.7f; c.erGain = 0.5f; c.erSpread = 0.5f; c.modScale = 0.5f; c.gated = true; c.bloom = 0.1f; c.outputGain = 1.2f; break;
        case ReverbType::lofi:     c.sizeScale = 0.75f; c.modScale = 2.6f; c.crush = 2; c.maxDamp = 4000.0f; c.loopLowCut = 200.0f; break;
        case ReverbType::voidType: c.sizeScale = 1.9f; c.modScale = 1.0f; c.bloom = 2.0f; c.maxDamp = 7000.0f; break;
        case ReverbType::count: break;
    }
    return c;
}

TypeInfo::Colours TypeInfo::colours (int t)
{
    using C = juce::Colour;
    switch ((ReverbType) t)
    {
        case ReverbType::cloud:    return { C (0xff9b5cff), C (0xff6a2bd9), C (0xff2a0e57), C (0xff08050f) };
        case ReverbType::plate:    return { C (0xffffd60a), C (0xffffe36b), C (0xffb3ecff), C (0xff4fc3f7), true };
        case ReverbType::hall:     return { C (0xffe8c27a), C (0xffd9b36a), C (0xff1b2a55), C (0xff050b1c) };
        case ReverbType::room:     return { C (0xffff9f43), C (0xffd9731f), C (0xff4a2208), C (0xff0f0703) };
        case ReverbType::chamber:  return { C (0xff3ee07f), C (0xff1fa85a), C (0xff0b3d22), C (0xff030c07) };
        case ReverbType::vintage:  return { C (0xffff7a3d), C (0xffe0482a), C (0xff4d1408), C (0xff100503) };
        case ReverbType::spring:   return { C (0xffe0896b), C (0xffb4593e), C (0xff3d1c12), C (0xff0d0806) };
        case ReverbType::shimmer:  return { C (0xfff3b5e0), C (0xffd78fc6), C (0xff4a2a44), C (0xff0e080d) };
        case ReverbType::reverse:  return { C (0xffff3b5c), C (0xffd1153a), C (0xff4a0614), C (0xff0f0204) };
        case ReverbType::gated:    return { C (0xff22e5ff), C (0xff0fb5d1), C (0xff053a45), C (0xff020a0d) };
        case ReverbType::lofi:     return { C (0xffd8b88f), C (0xffb39166), C (0xff3d3020), C (0xff0e0b07) };
        case ReverbType::voidType:
        case ReverbType::count:    break;
    }
    return { C (0xffffffff), C (0xffbdbdbd), C (0xff2a2a2a), C (0xff030303) };
}

} // namespace lt_void
