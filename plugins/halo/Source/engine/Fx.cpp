#include "Fx.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <regex>

namespace halo
{

namespace
{
    constexpr double pi = 3.14159265358979323846;

    /** RBJ biquad, direct form I. */
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;

        static Biquad highPass (double sr, double f, double q = 0.7071)
        {
            double w = 2 * pi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
            Biquad b;
            b.b0 = (1 + c) / 2 / a0; b.b1 = -(1 + c) / a0; b.b2 = (1 + c) / 2 / a0;
            b.a1 = -2 * c / a0; b.a2 = (1 - al) / a0;
            return b;
        }

        static Biquad peak (double sr, double f, double q, double db)
        {
            double A = std::pow (10.0, db / 40), w = 2 * pi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
            double a0 = 1 + al / A;
            Biquad b;
            b.b0 = (1 + al * A) / a0; b.b1 = -2 * c / a0; b.b2 = (1 - al * A) / a0;
            b.a1 = -2 * c / a0; b.a2 = (1 - al / A) / a0;
            return b;
        }

        float process (float x)
        {
            double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return (float) y;
        }
    };
}

namespace fx
{

void force (std::vector<float>& y, int sr, double amount)
{
    if (y.empty() || amount <= 0) return;
    auto hp = Biquad::highPass (sr, 140);
    auto pres = Biquad::peak (sr, 3000, 0.9, 5.0 * amount);
    for (auto& v : y) v = pres.process (hp.process (v));

    float pk = 0;
    for (auto v : y) pk = std::max (pk, std::abs (v));
    if (pk <= 1e-9f) return;
    double drive = 1.0 + 2.0 * amount, norm = std::tanh (drive);
    for (auto& v : y) v = (float) (std::tanh (drive * v / pk) / norm * pk);

    // peak compressor
    double thresh = -24, ratio = 3 + 3 * amount, makeup = 4 + 4 * amount;
    double att = std::exp (-1.0 / (sr * 0.005)), rel = std::exp (-1.0 / (sr * 0.080)), env = 0;
    for (auto& v : y)
    {
        double a = std::abs (v);
        env = a > env ? att * env + (1 - att) * a : rel * env + (1 - rel) * a;
        double lvl = 20 * std::log10 (env + 1e-9);
        double gain = (lvl > thresh ? (thresh - lvl) * (1 - 1 / ratio) : 0.0) + makeup;
        v = (float) (v * std::pow (10.0, gain / 20));
    }
}

void gate (std::vector<float>& y, int sr, const std::vector<Region>& regions, double fadeIn, double fadeOut)
{
    std::vector<float> m (y.size(), 0.0f);
    for (auto& r : regions)
    {
        long a = (long) (r.start * sr), b = std::min ((long) y.size(), (long) (r.end * sr));
        a = std::max (0L, a);
        if (b <= a) continue;
        long fi = std::min ((long) (fadeIn * sr), (b - a) / 2), fo = std::min ((long) (fadeOut * sr), (b - a) / 2);
        for (long i = a; i < b; ++i)
        {
            float g = 1.0f;
            if (i - a < fi) g = (float) (i - a) / (float) fi;
            if (b - i <= fo) g = std::min (g, (float) (b - i) / (float) fo);
            m[(size_t) i] = std::max (m[(size_t) i], g);
        }
    }
    for (size_t i = 0; i < y.size(); ++i) y[i] *= m[i];
}

std::vector<float> phraseEndEcho (const std::vector<float>& y, const Analysis& an, double beat)
{
    std::vector<float> out (y.size(), 0.0f);
    const int sr = an.sampleRate;
    const long shift = (long) (beat * sr);
    auto& notes = an.notes;
    for (size_t i = 0; i < notes.size(); ++i)
    {
        bool last = i + 1 == notes.size() || (notes[i + 1].start - notes[i].end) * frameMs > 250;
        if (! last) continue;
        long e = (long) (notes[i].end * frameSec * sr) + (long) (0.08 * sr);
        long s = std::max ((long) (notes[i].start * frameSec * sr), e - (long) (0.7 * sr));
        e = std::min (e, (long) y.size());
        if (e - s < 64) continue;
        long len = e - s, f = std::min (len / 4, (long) (0.02 * sr));
        for (long k = 0; k < len; ++k)
        {
            float g = 1.0f;
            if (k < f) g = (float) k / (float) f;
            if (k > len - 3 * f) g = std::min (g, (float) (len - k) / (float) (3 * f));
            long d = s + shift + k;
            if (d >= 0 && d < (long) out.size()) out[(size_t) d] += y[(size_t) (s + k)] * g;
        }
    }
    return out;
}

std::vector<std::vector<Word>> splitPhrases (const std::vector<Word>& words, double gapSec)
{
    std::vector<std::vector<Word>> phrases;
    std::vector<Word> cur;
    for (auto& w : words)
    {
        if (! cur.empty())
        {
            bool newLine = ! w.text.empty() && std::isupper ((unsigned char) w.text[0]) && w.text != "I";
            if (w.start - cur.back().end > gapSec || newLine)
            {
                phrases.push_back (cur);
                cur.clear();
            }
        }
        cur.push_back (w);
    }
    if (! cur.empty()) phrases.push_back (cur);
    return phrases;
}

std::vector<Region> refuerzoRegions (const std::vector<Word>& words, const Analysis& an, int nWords)
{
    std::vector<Region> out;
    const int n = an.numFrames();
    auto extendEnd = [&] (double e)
    {
        int i = std::min ((int) (e / frameSec), n - 1);
        int limit = std::min (n, (int) ((e + 1.2) / frameSec));
        while (i >= 0 && i < limit && an.midi[(size_t) i] > 0) ++i;
        return std::max (e, i * frameSec) + 0.05;
    };

    if (! words.empty())
    {
        bool userMarked = std::any_of (words.begin(), words.end(), [] (const Word& w) { return w.reinforce; });
        if (userMarked)
        {
            // consecutive marked words form one region
            for (size_t i = 0; i < words.size();)
            {
                if (! words[i].reinforce) { ++i; continue; }
                size_t j = i;
                Region r { words[i].start, words[i].end, words[i].text };
                while (j + 1 < words.size() && words[j + 1].reinforce && words[j + 1].start - words[j].end < 0.4)
                {
                    ++j;
                    r.end = words[j].end;
                    r.text += " " + words[j].text;
                }
                r.end = extendEnd (r.end);
                out.push_back (r);
                i = j + 1;
            }
            return out;
        }
        for (auto& ph : splitPhrases (words))
        {
            auto first = ph.size() > (size_t) nWords ? ph.size() - (size_t) nWords : 0;
            Region r { ph[first].start, ph.back().end, {} };
            for (auto i = first; i < ph.size(); ++i) r.text += (i > first ? " " : "") + ph[i].text;
            r.end = extendEnd (r.end);
            out.push_back (r);
        }
        return out;
    }

    auto& notes = an.notes;
    for (size_t i = 0; i < notes.size(); ++i)
        if (i + 1 == notes.size() || (notes[i + 1].start - notes[i].end) * frameMs > 250)
        {
            double e = notes[i].end * frameSec + 0.05;
            out.push_back ({ std::max (0.0, e - 0.8), e, "" });
        }
    return out;
}

} // namespace fx

double bpmFromName (const std::string& name)
{
    std::smatch m;
    std::regex re ("(^|[^0-9])([0-9]{2,3})([^0-9]|$)");
    auto s = name;
    while (std::regex_search (s, m, re))
    {
        int v = std::stoi (m[2].str());
        if (v >= 60 && v <= 200) return v;
        s = m.suffix().str();
    }
    return 0;
}

} // namespace halo
