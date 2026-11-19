#include "Presets.h"
#include "QuasarData.h"

namespace quasar
{

juce::var Preset::toVar() const
{
    auto o = std::make_unique<juce::DynamicObject>();
    o->setProperty ("format", "LTQ");
    o->setProperty ("version", 1);
    o->setProperty ("name", name);
    o->setProperty ("category", category);
    o->setProperty ("style", style);
    o->setProperty ("author", author);
    o->setProperty ("description", description);
    auto vals = std::make_unique<juce::DynamicObject>();
    for (auto& [id, v] : values) vals->setProperty (id, v);
    o->setProperty ("values", juce::var (vals.release()));
    if (! lfoShapes.isVoid()) o->setProperty ("lfo", lfoShapes);
    if (! mods.isVoid()) o->setProperty ("mods", mods);
    if (! fx.isVoid()) o->setProperty ("fx", fx);
    juce::Array<juce::var> macros;
    for (auto& m : macroNames) macros.add (m);
    o->setProperty ("macros", macros);
    juce::Array<juce::var> smp;
    for (auto& s : samplePaths) smp.add (s);
    o->setProperty ("samples", smp);
    return juce::var (o.release());
}

Preset Preset::fromVar (const juce::var& v)
{
    Preset p;
    p.name = v.getProperty ("name", "Untitled").toString();
    p.category = v.getProperty ("category", "").toString().toUpperCase();
    p.style = v.getProperty ("style", "").toString().toUpperCase();
    p.author = v.getProperty ("author", "").toString();
    p.description = v.getProperty ("description", "").toString();
    if (auto* vals = v.getProperty ("values", {}).getDynamicObject())
        for (auto& prop : vals->getProperties()) p.values[prop.name.toString()] = (float) prop.value;
    p.lfoShapes = v.getProperty ("lfo", {});
    p.mods = v.getProperty ("mods", {});
    p.fx = v.getProperty ("fx", {});
    if (auto* a = v.getProperty ("macros", {}).getArray()) for (auto& m : *a) p.macroNames.add (m.toString());
    if (auto* a = v.getProperty ("samples", {}).getArray())
        for (int i = 0; i < juce::jmin (3, a->size()); ++i) p.samplePaths[(size_t) i] = (*a)[i].toString();
    return p;
}

// ------------------------------------------------------------------ library

PresetLibrary::PresetLibrary() { rescan(); }

juce::File PresetLibrary::rootFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("LT Audio").getChildFile ("Presets").getChildFile ("LT QUASAR");
}
juce::File PresetLibrary::userFolder()  { return rootFolder().getChildFile ("User"); }
juce::File PresetLibrary::banksFolder() { return rootFolder().getChildFile ("Banks"); }

const juce::StringArray& PresetLibrary::categories()
{
    static const juce::StringArray c { "BASS", "REESE", "808", "SUB", "PLUCK", "KEYS", "LEAD", "PAD", "BELL", "ARP", "CHORD", "FX", "VOX" };
    return c;
}

const juce::StringArray& PresetLibrary::styles()
{
    static const juce::StringArray s { "TRAP", "REGGAETON", "R&B", "LO-FI", "DRILL", "AFRO" };
    return s;
}

void PresetLibrary::loadFactory()
{
    for (int i = 0; i < QuasarData::namedResourceListSize; ++i)
    {
        juce::String file (QuasarData::originalFilenames[i]);
        if (! file.endsWith (".json")) continue;
        int size = 0;
        auto* data = QuasarData::getNamedResource (QuasarData::namedResourceList[i], size);
        auto v = juce::JSON::parse (juce::String::fromUTF8 (data, size));
        Bank b;
        b.factory = true;
        b.name = v.getProperty ("name", file.upToFirstOccurrenceOf (".", false, false)).toString();
        b.author = v.getProperty ("author", "LT SOUNDS").toString();
        b.description = v.getProperty ("description", "").toString();
        b.colour = juce::Colour::fromString (v.getProperty ("colour", "ff7b2cff").toString());
        if (auto* a = v.getProperty ("presets", {}).getArray())
            for (auto& pv : *a)
            {
                auto p = Preset::fromVar (pv);
                p.bank = b.name; p.factory = true;
                if (p.author.isEmpty()) p.author = b.author;
                b.presets.push_back (std::move (p));
            }
        all.push_back (std::move (b));
    }
    std::stable_sort (all.begin(), all.end(), [] (const Bank& a, const Bank& b)
    {
        auto rank = [] (const juce::String& n) { return n == "QUASAR FACTORY" ? 0 : 1; };
        return rank (a.name) < rank (b.name);
    });
}

void PresetLibrary::loadFolderBank (const juce::File& folder, bool isUser)
{
    Bank b;
    b.folder = folder;
    b.name = isUser ? juce::String ("USER") : folder.getFileName().toUpperCase();
    b.author = isUser ? juce::String ("YOU") : juce::String();
    b.colour = juce::Colour (0xff7b2cff).withRotatedHue ((float) (folder.getFileName().hashCode() % 100) / 300.0f);
    auto info = folder.getChildFile ("bank.json");
    if (info.existsAsFile())
    {
        auto v = juce::JSON::parse (info);
        b.name = v.getProperty ("name", b.name).toString().toUpperCase();
        b.author = v.getProperty ("author", b.author).toString();
        b.description = v.getProperty ("description", "").toString();
        if (v.hasProperty ("colour")) b.colour = juce::Colour::fromString (v["colour"].toString());
    }
    auto cover = folder.getChildFile ("cover.png");
    if (cover.existsAsFile()) b.cover = juce::ImageFileFormat::loadFrom (cover);
    auto presetDir = folder.getChildFile ("presets").isDirectory() ? folder.getChildFile ("presets") : folder;
    for (auto& f : presetDir.findChildFiles (juce::File::findFiles, false, "*.ltq"))
    {
        auto p = Preset::fromVar (juce::JSON::parse (f));
        p.file = f; p.bank = b.name;
        // samples inside a bank are stored relative to the bank folder
        for (auto& s : p.samplePaths)
            if (s.isNotEmpty() && ! juce::File::isAbsolutePath (s)) s = folder.getChildFile (s).getFullPathName();
        b.presets.push_back (std::move (p));
    }
    std::sort (b.presets.begin(), b.presets.end(), [] (const Preset& x, const Preset& y) { return x.name.compareNatural (y.name) < 0; });
    all.push_back (std::move (b));
}

void PresetLibrary::rescan()
{
    all.clear();
    loadFactory();
    userFolder().createDirectory();
    banksFolder().createDirectory();
    loadFolderBank (userFolder(), true);
    for (auto& d : banksFolder().findChildFiles (juce::File::findDirectories, false))
        loadFolderBank (d, false);

    auto lists = rootFolder().getChildFile ("lists.json");
    favourites.clear(); recent.clear();
    if (lists.existsAsFile())
    {
        auto v = juce::JSON::parse (lists);
        if (auto* a = v.getProperty ("favourites", {}).getArray()) for (auto& s : *a) favourites.add (s.toString());
        if (auto* a = v.getProperty ("recent", {}).getArray()) for (auto& s : *a) recent.add (s.toString());
    }
}

void PresetLibrary::saveLists() const
{
    auto o = std::make_unique<juce::DynamicObject>();
    juce::Array<juce::var> f, r;
    for (auto& s : favourites) f.add (s);
    for (auto& s : recent) r.add (s);
    o->setProperty ("favourites", f);
    o->setProperty ("recent", r);
    rootFolder().createDirectory();
    rootFolder().getChildFile ("lists.json").replaceWithText (juce::JSON::toString (juce::var (o.release())));
}

void PresetLibrary::toggleFavourite (const Preset& p)
{
    if (favourites.contains (p.key())) favourites.removeString (p.key());
    else favourites.add (p.key());
    saveLists();
}

void PresetLibrary::addRecent (const Preset& p)
{
    recent.removeString (p.key());
    recent.insert (0, p.key());
    while (recent.size() > 30) recent.remove (recent.size() - 1);
    saveLists();
}

const Preset* PresetLibrary::find (const juce::String& bank, const juce::String& name) const
{
    for (auto& b : all)
        if (bank.isEmpty() || b.name == bank)
            for (auto& p : b.presets)
                if (p.name == name) return &p;
    return nullptr;
}

std::vector<const Preset*> PresetLibrary::allPresets() const
{
    std::vector<const Preset*> v;
    for (auto& b : all) for (auto& p : b.presets) v.push_back (&p);
    return v;
}

namespace
{
    juce::String safeName (const juce::String& n)
    {
        auto s = juce::File::createLegalFileName (n.trim());
        return s.isEmpty() ? juce::String ("Untitled") : s;
    }
}

bool PresetLibrary::saveUserPreset (Preset p, const juce::String& bankName)
{
    juce::File folder = userFolder();
    for (auto& b : all)
        if (b.name == bankName && b.folder != juce::File()) folder = b.folder.getChildFile ("presets").isDirectory() ? b.folder.getChildFile ("presets") : b.folder;
    folder.createDirectory();
    auto f = folder.getChildFile (safeName (p.name) + ".ltq");
    auto ok = f.replaceWithText (juce::JSON::toString (p.toVar()));
    rescan();
    return ok;
}

bool PresetLibrary::deletePreset (const Preset& p)
{
    if (p.factory || ! p.file.existsAsFile()) return false;
    auto ok = p.file.moveToTrash();
    rescan();
    return ok;
}

bool PresetLibrary::createBank (const juce::String& name, const juce::String& author, const juce::String& description)
{
    auto folder = banksFolder().getChildFile (safeName (name));
    if (folder.exists()) return false;
    folder.getChildFile ("presets").createDirectory();
    auto o = std::make_unique<juce::DynamicObject>();
    o->setProperty ("name", name.toUpperCase());
    o->setProperty ("author", author);
    o->setProperty ("description", description);
    folder.getChildFile ("bank.json").replaceWithText (juce::JSON::toString (juce::var (o.release())));
    rescan();
    return true;
}

bool PresetLibrary::exportBank (const juce::String& bankName, const juce::File& target, juce::String& error) const
{
    const Bank* bank = nullptr;
    for (auto& b : all) if (b.name == bankName) bank = &b;
    if (bank == nullptr) { error = "Bank not found"; return false; }

    juce::ZipFile::Builder zip;
    std::vector<std::unique_ptr<juce::MemoryBlock>> keep;
    auto addText = [&] (const juce::String& path, const juce::String& text)
    {
        auto mb = std::make_unique<juce::MemoryBlock> (text.toRawUTF8(), text.getNumBytesAsUTF8());
        zip.addEntry (new juce::MemoryInputStream (*mb, false), 9, path, juce::Time::getCurrentTime());
        keep.push_back (std::move (mb));
    };
    auto info = std::make_unique<juce::DynamicObject>();
    info->setProperty ("name", bank->name);
    info->setProperty ("author", bank->author);
    info->setProperty ("description", bank->description);
    info->setProperty ("colour", bank->colour.toString());
    addText ("bank.json", juce::JSON::toString (juce::var (info.release())));
    juce::StringArray added;
    for (auto& p : bank->presets)
    {
        auto copy = p;
        for (int i = 0; i < 3; ++i)
        {
            juce::File f (copy.samplePaths[(size_t) i]);
            if (copy.samplePaths[(size_t) i].isNotEmpty() && f.existsAsFile())
            {
                auto rel = "samples/" + f.getFileName();
                if (! added.contains (rel)) { zip.addFile (f, 9, rel); added.add (rel); }
                copy.samplePaths[(size_t) i] = rel;
            }
        }
        addText ("presets/" + safeName (p.name) + ".ltq", juce::JSON::toString (copy.toVar()));
    }
    if (bank->folder.getChildFile ("cover.png").existsAsFile()) zip.addFile (bank->folder.getChildFile ("cover.png"), 9, "cover.png");
    target.deleteFile();
    juce::FileOutputStream out (target);
    if (! out.openedOk()) { error = "Cannot write " + target.getFullPathName(); return false; }
    return zip.writeToStream (out, nullptr);
}

bool PresetLibrary::importBank (const juce::File& file, juce::String& error, juce::String* importedName)
{
    juce::ZipFile zip (file);
    if (zip.getNumEntries() == 0) { error = "Not a QUASAR bank (.ltqbank)"; return false; }
    juce::String name = file.getFileNameWithoutExtension();
    if (auto* e = zip.getEntry ("bank.json"))
        if (std::unique_ptr<juce::InputStream> in { zip.createStreamForEntry (*e) })
            name = juce::JSON::parse (in->readEntireStreamAsString()).getProperty ("name", name).toString();
    auto folder = banksFolder().getChildFile (safeName (name));
    for (int k = 2; folder.exists(); ++k) folder = banksFolder().getChildFile (safeName (name) + " " + juce::String (k));
    // only plain relative paths inside the bank are accepted
    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        auto* e = zip.getEntry (i);
        if (e->filename.contains ("..") || juce::File::isAbsolutePath (e->filename)) { error = "Invalid bank file"; return false; }
    }
    auto r = zip.uncompressTo (folder, true);
    if (r.failed()) { error = r.getErrorMessage(); folder.deleteRecursively(); return false; }
    if (importedName) *importedName = name.toUpperCase();
    rescan();
    return true;
}

bool PresetLibrary::exportPreset (const Preset& p, const juce::File& target) const
{
    return target.replaceWithText (juce::JSON::toString (p.toVar()));
}

bool PresetLibrary::importPreset (const juce::File& f, juce::String& error)
{
    auto v = juce::JSON::parse (f);
    if (v.getProperty ("format", "").toString() != "LTQ") { error = "Not a QUASAR preset (.ltq)"; return false; }
    userFolder().createDirectory();
    auto ok = f.copyFileTo (userFolder().getChildFile (f.getFileName()));
    rescan();
    return ok;
}

} // namespace quasar
