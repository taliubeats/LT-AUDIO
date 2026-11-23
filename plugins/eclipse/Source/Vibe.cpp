#include "Vibe.h"

namespace eclipse
{

void VibeSaturator::prepare (double sampleRate)
{
    sr = sampleRate;
    reset();
}

void VibeSaturator::reset()
{
    for (int c = 0; c < 2; ++c) { tone[c].z = band[c].z = low[c].z = dc[c].z = 0; dcX[c] = dcY[c] = 0; }
    env = 0;
}

float VibeSaturator::shape (Vibe t, float x, float d)
{
    const float y = x * d;
    switch (t)
    {
        case Vibe::tape:
        case Vibe::darkTape:  return std::tanh (y + 0.08f * y * y / (1.0f + std::abs (y)));
        case Vibe::warmTape:  { float s = std::tanh (y * 0.9f + 0.12f * y * y / (1.0f + y * y)); return s * (1.0f - 0.15f * s * s); }
        case Vibe::hardClip:  return juce::jlimit (-1.0f, 1.0f, y * (1.0f + 0.05f * std::abs (y)));
        case Vibe::fatClip:   return juce::jlimit (-0.95f, 0.95f, y) / 0.95f;
        case Vibe::tube:      return std::tanh (y + 0.25f * y * y);
        case Vibe::heavyTube: { float a = y + 0.6f * y * y; return std::tanh (a * 1.4f); }
        case Vibe::console:   { float c = juce::jlimit (-1.5f, 1.5f, y); return c - (c * c * c) / 6.75f; }
        case Vibe::sweet:     return std::tanh (y + 0.35f * y * y);
        case Vibe::fuzz:      { float a = std::tanh (y * 3.0f); return std::copysign (std::pow (std::abs (a), 0.6f), a); }
        case Vibe::exciter:
        case Vibe::silk:
        case Vibe::bright:
        case Vibe::glue:      return std::tanh (y);
    }
    return std::tanh (y);
}

void VibeSaturator::process (float* const* ch, int nCh, int n, Vibe t, float amount)
{
    amount = juce::jlimit (0.0f, 10.0f, amount);
    if (amount <= 0.0f && smoothedAmount <= 1.0e-4f) { smoothedAmount = 0; return; }

    nCh = juce::jmin (nCh, 2);
    const float k = 1.0f - std::exp (-1.0f / (0.02f * (float) sr));      // 20 ms smoothing

    // filters that depend on the character
    double toneHz = 18000, bandLo = 3000, lowHz = 150;
    switch (t)
    {
        case Vibe::warmTape:  toneHz = 11000; break;
        case Vibe::darkTape:  toneHz = 5200;  break;
        case Vibe::fuzz:      toneHz = 4500;  break;
        case Vibe::heavyTube: toneHz = 9000;  break;
        case Vibe::console:   toneHz = 15000; break;
        case Vibe::silk:      bandLo = 400;   toneHz = 3500; break;
        case Vibe::exciter:   bandLo = 3500;  break;
        case Vibe::bright:    bandLo = 5000;  break;
        default: break;
    }
    for (int c = 0; c < 2; ++c) { tone[c].set (sr, toneHz); band[c].set (sr, bandLo); low[c].set (sr, lowHz); }
    const float envA = std::exp (-1.0f / (0.004f * (float) sr)), envR = std::exp (-1.0f / (0.12f * (float) sr));

    for (int i = 0; i < n; ++i)
    {
        smoothedAmount += k * (amount - smoothedAmount);
        const float a = smoothedAmount / 10.0f;                       // 0..1
        const float drive = 1.0f + a * a * (t == Vibe::fuzz ? 30.0f : t == Vibe::hardClip || t == Vibe::fatClip ? 9.0f : 5.0f);
        const float comp = 1.0f / std::sqrt (drive);                   // keeps the level close when pushed

        float peak = 0;
        for (int c = 0; c < nCh; ++c) peak = juce::jmax (peak, std::abs (ch[c][i]));
        env = peak > env ? envA * env + (1 - envA) * peak : envR * env + (1 - envR) * peak;

        for (int c = 0; c < nCh; ++c)
        {
            const float x = ch[c][i];
            float y = 0;
            switch (t)
            {
                case Vibe::exciter:
                case Vibe::bright:
                {   // saturate only the highs and add them on top
                    const float lowPart = band[c].lp (x), high = x - lowPart;
                    const float h = shape (Vibe::tape, high, 1.0f + a * 8.0f) / (1.0f + a * 2.0f);
                    y = shape (Vibe::tape, x, 1.0f + a * 0.8f) / (1.0f + a * 0.3f) + h * a * (t == Vibe::bright ? 1.2f : 1.8f);
                    break;
                }
                case Vibe::silk:
                {   // mids get the drive, lows and highs stay cleaner
                    const float lowPart = band[c].lp (x), rest = x - lowPart;
                    const float mid = tone[c].lp (rest), top = rest - mid;
                    y = lowPart * (1.0f + a * 0.2f) + shape (Vibe::sweet, mid, drive) * comp * 1.2f + top;
                    break;
                }
                case Vibe::fatClip:
                {   // boost the sub before clipping: fat low end
                    const float sub = low[c].lp (x);
                    y = shape (t, x + sub * a * 1.5f, drive) * comp;
                    break;
                }
                case Vibe::glue:
                {   // program-dependent drive: louder hits get squeezed a bit and saturated
                    const float g = 1.0f / (1.0f + a * 3.0f * env);
                    y = shape (Vibe::tape, x * g * (1.0f + a * 1.5f), 1.0f + a) * (1.0f + a * 0.6f) * comp;
                    break;
                }
                default:
                    y = shape (t, x, drive) * comp;
                    if (toneHz < 17000)
                    {   // the more VIBE, the more the character's top-end roll-off
                        const float dark = tone[c].lp (y);
                        y += (dark - y) * a;
                    }
                    break;
            }
            // remove the DC that asymmetric curves create
            const float out = y - dcX[c] + 0.9995f * dcY[c];
            dcX[c] = y; dcY[c] = out;
            // equal-power blend with the dry signal for small amounts (smooth start from 0)
            const float wet = juce::jmin (1.0f, a * 4.0f);
            ch[c][i] = x * (1.0f - wet) + out * wet;
        }
    }
}

} // namespace eclipse
