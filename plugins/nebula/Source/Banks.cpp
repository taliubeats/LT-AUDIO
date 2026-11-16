#include "Banks.h"

namespace nebula
{

namespace { constexpr float pi = juce::MathConstants<float>::pi; }

juce::File BankLibrary::root()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile ("Application Support");
   #endif
    return base.getChildFile ("LT Audio").getChildFile ("Presets").getChildFile ("LT NEBULA");
}

BankLibrary::BankLibrary (std::vector<lt::presets::ParamPreset> f) : factory (std::move (f))
{
    rescan();
}

juce::Image BankLibrary::makeCover (int style, int seed, int size)
{
    juce::Image img (juce::Image::ARGB, size, size, true);
    juce::Graphics g (img);
    juce::Random r (seed);
    auto s = (float) size, c = s * 0.5f;
    g.fillAll (juce::Colour (0xff07050d));
    for (int i = 0; i < size / 2; ++i)   // background stars
    {
        auto x = r.nextFloat() * s, y = r.nextFloat() * s, d = 0.5f + r.nextFloat() * 1.3f;
        g.setColour (juce::Colours::white.withAlpha (0.2f + 0.6f * r.nextFloat()));
        g.fillEllipse (x, y, d, d);
    }
    using lt::ui::Glass;
    switch (style % 5)
    {
        case 0:   // spiral galaxy (purple)
        {
            Glass::drawGlow (g, { c, c }, s * 0.55f, juce::Colour (0xff7b2cff), 0.5f);
            for (int i = 0; i < 2600; ++i)
            {
                auto arm = r.nextInt (2);
                auto t = std::pow (r.nextFloat(), 0.8f);
                auto a = (float) arm * pi + t * 4.4f + (r.nextFloat() - 0.5f) * (1.2f - 0.6f * t);
                auto rad = t * s * 0.46f;
                auto x = c + std::cos (a) * rad, y = c + std::sin (a) * rad * 0.62f;
                auto d = 0.6f + r.nextFloat() * 1.6f;
                g.setColour (juce::Colour (0xffd9b8ff).interpolatedWith (juce::Colours::white, r.nextFloat()).withAlpha (0.25f + 0.6f * r.nextFloat()));
                g.fillEllipse (x, y, d, d);
            }
            Glass::drawGlow (g, { c, c }, s * 0.18f, juce::Colours::white, 0.9f);
            break;
        }
        case 1:   // golden planet
        {
            auto R = s * 0.33f;
            juce::ColourGradient body (juce::Colour (0xfff7d58a), c - R * 0.4f, c - R * 0.4f, juce::Colour (0xff5a3a10), c + R, c + R, true);
            g.setGradientFill (body);
            g.fillEllipse (c - R, c - R, R * 2, R * 2);
            for (int i = 0; i < 6; ++i)
            {
                g.setColour (juce::Colour (0x22000000));
                g.fillRect (c - R, c - R * 0.6f + (float) i * R * 0.25f, R * 2, R * 0.06f);
            }
            break;
        }
        case 2:   // black hole
        {
            Glass::drawGlow (g, { c, c }, s * 0.45f, juce::Colour (0xff8a7aa8), 0.5f);
            g.setColour (juce::Colour (0xffeae6f5).withAlpha (0.8f));
            g.drawEllipse (c - s * 0.24f, c - s * 0.24f, s * 0.48f, s * 0.48f, s * 0.02f);
            g.setColour (juce::Colours::black);
            g.fillEllipse (c - s * 0.22f, c - s * 0.22f, s * 0.44f, s * 0.44f);
            break;
        }
        case 3:   // blue nebula
            for (int i = 0; i < 40; ++i)
                Glass::drawGlow (g, { c + (r.nextFloat() - 0.5f) * s * 0.8f, c + (r.nextFloat() - 0.5f) * s * 0.8f }, s * (0.1f + 0.2f * r.nextFloat()),
                                 juce::Colour (0xff2f7fff).interpolatedWith (juce::Colour (0xff7ff0ff), r.nextFloat()), 0.25f);
            break;
        default:  // aurora
            for (int band = 0; band < 3; ++band)
                for (int x = 0; x < size; x += 2)
                {
                    auto y = s * (0.35f + 0.12f * (float) band) + std::sin ((float) x / s * 6.0f + (float) band) * s * 0.06f;
                    juce::ColourGradient cg (juce::Colour (0xff3cffa0).withAlpha (0.0f), (float) x, y - s * 0.25f,
                                             juce::Colour (0xff3cffa0).withAlpha (0.35f), (float) x, y, false);
                    g.setGradientFill (cg);
                    g.fillRect ((float) x, y - s * 0.25f, 2.0f, s * 0.25f);
                }
            g.setColour (juce::Colour (0xff0b1a12));
            g.fillRect (0.0f, s * 0.82f, s, s * 0.18f);
            break;
    }
    return img;
}

Bank BankLibrary::loadFolder (const juce::File& folder, const juce::String& name) const
{
    Bank b;
    b.folder = folder;
    b.name = name;
    auto meta = juce::JSON::parse (folder.getChildFile ("bank.json"));
    if (meta.isObject())
    {
        if (meta["name"].toString().isNotEmpty()) b.name = meta["name"].toString();
        if (meta["author"].toString().isNotEmpty()) b.author = meta["author"].toString();
    }
    else b.author = "me";
    auto coverFile = folder.getChildFile ("cover.png");
    if (coverFile.existsAsFile()) b.cover = juce::ImageFileFormat::loadFrom (coverFile);
    if (b.cover.isNull())
    {
        auto up = b.name.toUpperCase();
        auto style = up.contains ("DARK") || up.contains ("VOID") || up.contains ("HOLE") ? 2
                   : up.contains ("AURORA") || up.contains ("NORTH") ? 4
                   : up.contains ("AMBIENT") || up.contains ("CLOUD") || up.contains ("SKY") ? 3
                   : up.contains ("COSMO") || up.contains ("GALAX") ? 0
                   : 1 + (int) ((juce::uint32) b.name.hashCode() % 4u);
        b.cover = makeCover (style, b.name.hashCode(), 256);
    }

    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.ltpreset");
    files.sort();
    for (auto& f : files)
    {
        auto v = juce::JSON::parse (f);
        if (! v.isObject()) continue;
        auto p = lt::presets::ParamPreset::fromVar (v);
        if (p.name.isEmpty()) p.name = f.getFileNameWithoutExtension();
        p.factory = false;
        p.file = f;
        b.presets.push_back (p);
    }
    return b;
}

void BankLibrary::rescan()
{
    list.clear();
    if (factoryCover.isNull()) factoryCover = makeCover (0, 7, 256);

    Bank cosmos;
    cosmos.name = "LT COSMOS";
    cosmos.factory = true;
    cosmos.cover = factoryCover;
    cosmos.presets = factory;
    list.push_back (cosmos);

    auto base = root();
    auto mine = loadFolder (base, "MY BANK");
    mine.name = "MY BANK";
    auto myCover = base.getChildFile ("cover.png");
    if (! myCover.existsAsFile()) mine.cover = makeCover (1, 3, 256);
    list.push_back (mine);

    auto dirs = base.findChildFiles (juce::File::findDirectories, false);
    dirs.sort();
    for (auto& d : dirs)
        list.push_back (loadFolder (d, d.getFileName()));

    favourites.clear();
    auto fav = juce::JSON::parse (base.getChildFile ("favorites.json"));
    if (auto* arr = fav.getArray())
        for (auto& v : *arr) favourites.add (v.toString());
}

int BankLibrary::indexOfBank (const juce::String& name) const
{
    for (int i = 0; i < (int) list.size(); ++i) if (list[(size_t) i].name == name) return i;
    return -1;
}

bool BankLibrary::isFavourite (const juce::String& bank, const juce::String& preset) const
{
    return favourites.contains (bank + "/" + preset);
}

void BankLibrary::toggleFavourite (const juce::String& bank, const juce::String& preset)
{
    auto key = bank + "/" + preset;
    if (favourites.contains (key)) favourites.removeString (key); else favourites.add (key);
    saveFavourites();
}

void BankLibrary::saveFavourites() const
{
    juce::Array<juce::var> arr;
    for (auto& f : favourites) arr.add (f);
    root().createDirectory();
    root().getChildFile ("favorites.json").replaceWithText (juce::JSON::toString (arr));
}

int BankLibrary::createBank (const juce::String& rawName)
{
    auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty() || name == "MY BANK" || name == "LT COSMOS") return -1;
    auto folder = root().getChildFile (name);
    if (! folder.createDirectory()) return -1;
    auto meta = std::make_unique<juce::DynamicObject>();
    meta->setProperty ("name", rawName.trim());
    meta->setProperty ("author", "me");
    folder.getChildFile ("bank.json").replaceWithText (juce::JSON::toString (juce::var (meta.release())));
    rescan();
    return indexOfBank (rawName.trim());
}

bool BankLibrary::deleteBank (int i)
{
    if (! juce::isPositiveAndBelow (i, (int) list.size()) || i < 2) return false;   // LT COSMOS and MY BANK stay
    auto ok = list[(size_t) i].folder.moveToTrash() || list[(size_t) i].folder.deleteRecursively();
    rescan();
    return ok;
}

bool BankLibrary::setCover (int i, const juce::File& image)
{
    if (! juce::isPositiveAndBelow (i, (int) list.size()) || list[(size_t) i].factory) return false;
    auto img = juce::ImageFileFormat::loadFrom (image);
    if (img.isNull()) return false;
    // square crop, 512 px
    auto side = juce::jmin (img.getWidth(), img.getHeight());
    auto crop = img.getClippedImage ({ (img.getWidth() - side) / 2, (img.getHeight() - side) / 2, side, side });
    juce::Image out (juce::Image::ARGB, 512, 512, true);
    { juce::Graphics g (out); g.drawImage (crop, juce::Rectangle<float> (0, 0, 512, 512)); }
    auto file = list[(size_t) i].folder.getChildFile ("cover.png");
    file.deleteFile();
    juce::FileOutputStream os (file);
    auto ok = os.openedOk() && juce::PNGImageFormat().writeImageToStream (out, os);
    os.flush();
    rescan();
    return ok;
}

bool BankLibrary::savePreset (int bankIndex, lt::presets::ParamPreset p)
{
    if (! juce::isPositiveAndBelow (bankIndex, (int) list.size()) || list[(size_t) bankIndex].factory) bankIndex = 1;
    p.name = p.name.trim();
    if (p.name.isEmpty()) return false;
    auto folder = list[(size_t) bankIndex].folder;
    folder.createDirectory();
    auto ok = folder.getChildFile (juce::File::createLegalFileName (p.name) + ".ltpreset").replaceWithText (juce::JSON::toString (p.toVar ("NEBULA")));
    rescan();
    return ok;
}

bool BankLibrary::deletePreset (int bankIndex, const juce::String& name)
{
    if (! juce::isPositiveAndBelow (bankIndex, (int) list.size()) || list[(size_t) bankIndex].factory) return false;
    for (auto& p : list[(size_t) bankIndex].presets)
        if (p.name == name) { auto ok = p.file.deleteFile(); rescan(); return ok; }
    return false;
}

bool BankLibrary::exportBank (int i, const juce::File& dest) const
{
    if (! juce::isPositiveAndBelow (i, (int) list.size())) return false;
    auto& b = list[(size_t) i];
    juce::ZipFile::Builder zip;
    auto meta = std::make_unique<juce::DynamicObject>();
    meta->setProperty ("name", b.name);
    meta->setProperty ("author", b.author);
    auto metaText = juce::JSON::toString (juce::var (meta.release()));
    zip.addEntry (new juce::MemoryInputStream (metaText.toRawUTF8(), metaText.getNumBytesAsUTF8(), true), 9, "bank.json", juce::Time::getCurrentTime());

    juce::MemoryOutputStream png;
    juce::PNGImageFormat().writeImageToStream (b.cover, png);
    zip.addEntry (new juce::MemoryInputStream (png.getData(), png.getDataSize(), true), 9, "cover.png", juce::Time::getCurrentTime());

    for (auto& p : b.presets)
    {
        auto text = juce::JSON::toString (p.toVar ("NEBULA"));
        zip.addEntry (new juce::MemoryInputStream (text.toRawUTF8(), text.getNumBytesAsUTF8(), true), 9,
                      juce::File::createLegalFileName (p.name) + ".ltpreset", juce::Time::getCurrentTime());
    }
    dest.deleteFile();
    juce::FileOutputStream os (dest);
    if (! os.openedOk()) return false;
    double progress = 0;
    return zip.writeToStream (os, &progress);
}

int BankLibrary::importFile (const juce::File& f)
{
    if (f.hasFileExtension ("ltpreset"))
    {
        auto target = root();
        target.createDirectory();
        f.copyFileTo (target.getChildFile (f.getFileName()));
        rescan();
        return 1;
    }
    if (! f.hasFileExtension ("ltbank")) return -1;

    juce::ZipFile zip (f);
    juce::String name = f.getFileNameWithoutExtension();
    if (auto* e = zip.getEntry ("bank.json"))
        if (std::unique_ptr<juce::InputStream> in { zip.createStreamForEntry (*e) })
        {
            auto v = juce::JSON::parse (in->readEntireStreamAsString());
            if (v["name"].toString().isNotEmpty()) name = v["name"].toString();
        }
    auto folder = root().getChildFile (juce::File::createLegalFileName (name));
    folder.createDirectory();
    zip.uncompressTo (folder, true);
    rescan();
    return indexOfBank (name);
}

} // namespace nebula
