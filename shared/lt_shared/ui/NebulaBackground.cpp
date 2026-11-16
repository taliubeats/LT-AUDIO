namespace lt::ui
{

namespace
{
    inline float hash2 (int x, int y, int seed)
    {
        auto h = (juce::uint32) (x * 374761393 + y * 668265263 + seed * 1442695041);
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= h >> 16;
        return (float) (h & 0xffffff) / (float) 0xffffff;
    }

    inline float smooth (float t) { return t * t * (3.0f - 2.0f * t); }

    float valueNoise (float x, float y, int seed)
    {
        auto xi = (int) std::floor (x), yi = (int) std::floor (y);
        auto xf = smooth (x - (float) xi), yf = smooth (y - (float) yi);
        auto a = hash2 (xi, yi, seed),     b = hash2 (xi + 1, yi, seed);
        auto c = hash2 (xi, yi + 1, seed), d = hash2 (xi + 1, yi + 1, seed);
        return juce::jmap (yf, juce::jmap (xf, a, b), juce::jmap (xf, c, d));
    }

    float fbm (float x, float y, int seed, int octaves = 5)
    {
        float sum = 0.0f, amp = 0.5f, norm = 0.0f;
        for (int i = 0; i < octaves; ++i)
        {
            sum += amp * valueNoise (x, y, seed + i * 31);
            norm += amp;
            x = x * 2.03f + 17.1f;
            y = y * 2.03f + 9.7f;
            amp *= 0.5f;
        }
        return sum / norm;
    }

    inline float smoothstep (float e0, float e1, float x)
    {
        auto t = juce::jlimit (0.0f, 1.0f, (x - e0) / (e1 - e0));
        return smooth (t);
    }
}

juce::Image NebulaBackground::renderNebula (int w, int h, const Palette& p, int seed, float bandTilt, float density)
{
    juce::Image img (juce::Image::ARGB, w, h, true);
    juce::Image::BitmapData data (img, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            auto u = (float) x / (float) w, v = (float) y / (float) h;

            // domain warp for wispy, organic clouds
            auto wx = fbm (u * 3.0f, v * 3.0f, seed + 101, 4);
            auto wy = fbm (u * 3.0f + 5.2f, v * 3.0f + 1.3f, seed + 202, 4);
            auto n  = fbm (u * 2.6f + wx * 1.6f, v * 2.6f + wy * 1.6f, seed);
            auto n2 = fbm (u * 4.0f + 3.1f, v * 4.0f + 7.7f, seed + 77, 4);

            // a soft diagonal "milky way" band, like the reference designs
            auto bandCentre = 0.5f + bandTilt * (u - 0.5f) + 0.08f * (wx - 0.5f);
            auto d = (v - bandCentre) / 0.36f;
            auto band = std::exp (-d * d);

            auto cloud = smoothstep (0.38f, 0.78f, n) * (0.35f + 0.65f * band) * density;
            auto core  = smoothstep (0.66f, 0.92f, n) * band * density;

            auto col = p.nebulaB.interpolatedWith (p.nebulaA, juce::jlimit (0.0f, 1.0f, n2 * 1.3f - 0.1f));
            col = col.interpolatedWith (p.nebulaC, core * 0.7f);

            auto alpha = juce::jlimit (0.0f, 1.0f, cloud * 0.85f + core * 0.4f);
            data.setPixelColour (x, y, col.withAlpha (alpha));
        }
    }

    return img;
}

NebulaBackground::NebulaBackground (int s) : random (s), seed (s)
{
    setInterceptsMouseClicks (false, false);
    setOpaque (true);
    startTimerHz (30);
}

NebulaBackground::~NebulaBackground() = default;

void NebulaBackground::setPalette (const Palette& p)
{
    palette = p;
    layerFar  = renderNebula (320, 180, palette, seed, -0.35f, 1.0f);
    layerNear = renderNebula (256, 144, palette, seed + 9, -0.2f, 0.7f);
    repaint();
}

void NebulaBackground::resized()
{
    if (layerFar.isNull())
        setPalette (palette);
    createStars();
}

void NebulaBackground::createStars()
{
    stars.clear();
    auto area = (float) (getWidth() * getHeight());
    auto count = (int) juce::jlimit (60.0f, 700.0f, area / 1500.0f);

    for (int i = 0; i < count; ++i)
    {
        auto big = random.nextFloat() < 0.06f;
        stars.push_back ({ { random.nextFloat() * (float) getWidth(), random.nextFloat() * (float) getHeight() },
                           big ? 1.6f + random.nextFloat() * 1.4f : 0.5f + random.nextFloat() * 1.0f,
                           random.nextFloat() * juce::MathConstants<float>::twoPi,
                           0.4f + random.nextFloat() * 1.6f });
    }
}

void NebulaBackground::timerCallback()
{
    if (! isShowing())
        return;

    auto dt = 1.0 / 30.0;
    if (! frozen)
        time += dt * driftSpeed * (1.0 + energy * 1.5);

    flash *= 0.90f;

    // spawn particles according to amount + energy
    auto spawnRate = (particleAmount * 1.5f + energy * 3.0f + flash * 6.0f);
    auto toSpawn = (int) spawnRate + (random.nextFloat() < (spawnRate - std::floor (spawnRate)) ? 1 : 0);

    for (int i = 0; i < toSpawn && particles.size() < 400; ++i)
    {
        auto angle = random.nextFloat() * juce::MathConstants<float>::twoPi;
        auto speed = 4.0f + random.nextFloat() * 14.0f;
        particles.push_back ({ { random.nextFloat() * (float) getWidth(), random.nextFloat() * (float) getHeight() },
                               { std::cos (angle) * speed, std::sin (angle) * speed - 4.0f },
                               1.0f, 0.6f + random.nextFloat() * 1.6f });
    }

    for (auto& pt : particles)
    {
        if (! frozen)
            pt.pos += pt.vel * (float) dt * (1.0f + energy);
        pt.life -= (float) dt * 0.35f;
    }

    particles.erase (std::remove_if (particles.begin(), particles.end(),
                                     [] (const Particle& pt) { return pt.life <= 0.0f; }),
                     particles.end());

    repaint();
}

void NebulaBackground::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.fillAll (palette.background);

    auto t = (float) time;

    // far layer: large, slow drift and very slow rotation-like sway
    if (layerFar.isValid())
    {
        auto scale = 1.25f;
        auto w = b.getWidth() * scale, h = b.getHeight() * scale;
        auto ox = -(w - b.getWidth()) * 0.5f + std::sin (t * 0.031f) * b.getWidth() * 0.05f;
        auto oy = -(h - b.getHeight()) * 0.5f + std::cos (t * 0.023f) * b.getHeight() * 0.04f;
        g.setOpacity (juce::jlimit (0.0f, 1.0f, 0.85f + energy * 0.15f + flash * 0.1f));
        g.drawImage (layerFar, { ox, oy, w, h }, juce::RectanglePlacement::stretchToFit);
    }

    // near layer drifts the other way for parallax
    if (layerNear.isValid())
    {
        auto scale = 1.4f;
        auto w = b.getWidth() * scale, h = b.getHeight() * scale;
        auto ox = -(w - b.getWidth()) * 0.5f - std::sin (t * 0.047f + 1.0f) * b.getWidth() * 0.08f;
        auto oy = -(h - b.getHeight()) * 0.5f - std::cos (t * 0.037f) * b.getHeight() * 0.06f;
        g.setOpacity (juce::jlimit (0.0f, 1.0f, 0.45f + energy * 0.25f + flash * 0.2f));
        g.drawImage (layerNear, { ox, oy, w, h }, juce::RectanglePlacement::stretchToFit);
    }

    g.setOpacity (1.0f);

    // stars
    for (auto& s : stars)
    {
        auto tw = 0.55f + 0.45f * std::sin (t * s.speed * 1.7f + s.phase);
        auto a = juce::jlimit (0.0f, 1.0f, tw * (palette.isLight ? 0.8f : 0.9f));
        g.setColour (palette.star.withAlpha (a));
        g.fillEllipse (s.pos.x - s.size * 0.5f, s.pos.y - s.size * 0.5f, s.size, s.size);

        if (s.size > 1.6f)
            Glass::drawGlow (g, s.pos, s.size * 4.0f, palette.star, 0.25f * tw);
    }

    // particles
    for (auto& pt : particles)
    {
        auto a = juce::jlimit (0.0f, 1.0f, pt.life * (0.6f + flash * 0.4f));
        g.setColour (palette.nebulaC.withAlpha (a));
        g.fillEllipse (pt.pos.x, pt.pos.y, pt.size, pt.size);
    }

    // vignette keeps edges calm and text readable
    if (! palette.isLight)
    {
        juce::ColourGradient vig (juce::Colours::transparentBlack, b.getCentreX(), b.getCentreY(),
                                  juce::Colours::black.withAlpha (0.55f), b.getX(), b.getY(), true);
        g.setGradientFill (vig);
        g.fillRect (b);
    }
    else
    {
        juce::ColourGradient vig (juce::Colours::white.withAlpha (0.08f), b.getCentreX(), b.getCentreY(),
                                  juce::Colour (0xff2aa9e0).withAlpha (0.25f), b.getX(), b.getY(), true);
        g.setGradientFill (vig);
        g.fillRect (b);
    }
}

} // namespace lt::ui
