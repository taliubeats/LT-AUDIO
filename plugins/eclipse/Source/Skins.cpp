#include "Skins.h"

namespace eclipse
{

const char* familyName (Family f)
{
    static const char* n[] = { "KICK", "SNARE", "CLAP", "HAT", "OPEN HAT", "TOM", "PERC", "CYMBAL", "SAMPLE" };
    return n[(int) f];
}

const char* familyLabel (Family f)
{
    static const char* n[] = { "Kick", "Snare", "Clap", "Hat", "Open Hat", "Tom", "Perc", "Cymbal", "Sample" };
    return n[(int) f];
}

const char* vibeName (Vibe v)
{
    static const char* n[] = { "TAPE", "WARM TAPE", "HARD CLIP", "TUBE", "EXCITER", "CONSOLE", "DARK TAPE",
                               "HEAVY TUBE", "SWEET", "SILK", "FAT CLIP", "BRIGHT", "FUZZ", "GLUE" };
    return n[(int) v];
}

namespace
{
juce::Colour hex (const char* h) { return juce::Colour::fromString (juce::String ("ff") + juce::String (h + 1)); }
juce::Colour mix (juce::Colour a, juce::Colour b, float t) { return a.interpolatedWith (b, t); }

void setFamilies (Skin& s, std::initializer_list<const char*> c)
{
    int i = 0;
    for (auto* h : c) s.fam[i++] = hex (h);
}

const char* defaultFam[] = { "#FF6A3D", "#FFC23D", "#FF4F96", "#3DD2FF", "#5B82FF", "#A36BFF", "#3DE08F", "#D6D6E0" };

Skin neutral (const char* id, const char* name, std::initializer_list<const char*> c, bool light)
{
    Skin s;
    s.id = id; s.name = name; s.caption = "Cinta neutra"; s.neutral = true; s.light = light; s.vibe = Vibe::tape;
    juce::Colour* slots[] = { &s.room, &s.chassis1, &s.chassis2, &s.panel1, &s.panel2, &s.tx, &s.dim, &s.lcd1, &s.lcd2,
                              &s.key1, &s.key2, &s.keyEdge, &s.keyTx, &s.pad1, &s.pad2, &s.padEdge, &s.padEmpty,
                              &s.cap1, &s.cap2, &s.skirt1, &s.skirt2, &s.ringOff, &s.acc, &s.screw };
    int i = 0;
    for (auto* h : c) *slots[i++] = hex (h);
    s.hi = light ? juce::Colours::white : juce::Colours::white.withAlpha (light ? 1.0f : 0.08f);
    s.lo = juce::Colours::black.withAlpha (light ? 0.15f : 0.8f);
    s.lcdLine = juce::Colours::white.withAlpha (0.035f);
    for (int f = 0; f < numFamilies; ++f) s.fam[f] = hex (defaultFam[f]);
    return s;
}

Skin artist (const char* id, const char* name, const char* caption, Vibe vibe, const char* base, const char* accent,
             bool light, std::initializer_list<const char*> fam)
{
    Skin s;
    s.id = id; s.name = name; s.caption = juce::String (juce::CharPointer_UTF8 (caption)); s.vibe = vibe; s.light = light;
    const auto b = hex (base), a = hex (accent), W = juce::Colours::white, K = juce::Colours::black;
    s.acc = a;
    if (! light)
    {
        s.room = mix (b, K, .7f); s.chassis1 = mix (b, W, .07f); s.chassis2 = mix (b, K, .35f);
        s.panel1 = mix (b, W, .1f); s.panel2 = mix (b, K, .08f); s.hi = W.withAlpha (0.08f); s.lo = K.withAlpha (0.8f);
        s.tx = hex ("#F3F3F6"); s.dim = mix (b, W, .48f); s.lcd1 = mix (b, K, .8f); s.lcd2 = mix (b, K, .62f);
        s.key1 = mix (b, W, .17f); s.key2 = mix (b, W, .06f); s.keyEdge = mix (b, W, .25f); s.keyTx = hex ("#ECECF2");
        s.pad1 = mix (b, W, .14f); s.pad2 = mix (b, W, .04f); s.padEdge = mix (b, K, .6f); s.padEmpty = mix (b, W, .08f);
        s.cap1 = mix (b, W, .32f); s.cap2 = mix (b, K, .15f); s.skirt1 = mix (b, W, .16f); s.skirt2 = mix (b, K, .45f);
        s.ringOff = mix (b, W, .12f); s.screw = mix (b, W, .25f);
    }
    else
    {
        s.room = mix (b, K, .25f); s.chassis1 = mix (b, W, .55f); s.chassis2 = mix (b, K, .04f);
        s.panel1 = mix (b, W, .75f); s.panel2 = mix (b, W, .35f); s.hi = W; s.lo = K.withAlpha (0.15f);
        s.tx = mix (a, K, .7f); s.dim = mix (b, K, .42f); s.lcd1 = mix (a, K, .88f); s.lcd2 = mix (a, K, .8f);
        s.key1 = mix (b, W, .85f); s.key2 = mix (b, W, .3f); s.keyEdge = mix (b, K, .12f); s.keyTx = mix (a, K, .65f);
        s.pad1 = mix (b, W, .6f); s.pad2 = mix (b, K, .04f); s.padEdge = mix (b, K, .22f); s.padEmpty = mix (b, W, .45f);
        s.cap1 = W; s.cap2 = mix (b, K, .12f); s.skirt1 = mix (b, W, .6f); s.skirt2 = mix (b, K, .22f);
        s.ringOff = mix (b, K, .08f); s.screw = mix (b, K, .2f);
    }
    s.lcdLine = W.withAlpha (0.035f);
    setFamilies (s, fam);
    return s;
}

std::vector<Skin> build()
{
    std::vector<Skin> v;
    //                     room       chassis1   chassis2   panel1     panel2     tx         dim        lcd1       lcd2       key1       key2       keyEdge    keyTx      pad1       pad2       padEdge    padEmpty   cap1       cap2       skirt1     skirt2     ringOff    acc        screw
    v.push_back (neutral ("negra",  "NEGRA",  { "#050506","#18181c","#0c0c0f","#1c1c21","#121216","#F2F2F7","#7d7d8a","#07070a","#0d0d12","#2b2b31","#1a1a1f","#3a3a42","#d9d9e2","#2a2a30","#18181c","#0a0a0c","#1d1d22","#4a4a52","#1c1c20","#2c2c32","#0e0e11","#26262c","#9A4DFF","#3a3a42" }, false));
    v.push_back (neutral ("blanca", "BLANCA", { "#cfd0d6","#f7f7fa","#e3e4ea","#fdfdff","#eeeef3","#1b1b24","#8b8c99","#15151b","#1f1f27","#ffffff","#e2e3e9","#c9cad3","#2a2a35","#f4f4f7","#dcdde4","#b9bac4","#e9eaef","#ffffff","#c9cad2","#e9e9ee","#b4b5bf","#d6d7de","#8B3DFF","#b9bac4" }, true));
    v.push_back (neutral ("gris",   "GRIS",   { "#17181b","#5a5e66","#3b3e44","#4d5158","#3c3f45","#f1f2f4","#b2b6bd","#0b0c0e","#15171a","#6b7078","#474b52","#7d828b","#eef0f3","#5b5f67","#3f4248","#26282c","#4a4e55","#d9dce1","#7b8088","#5d6168","#2c2e33","#33363b","#A45CFF","#8a8f97" }, false));
    v[2].hi = juce::Colours::white.withAlpha (0.16f);

    v.push_back (artist ("verde",    "VERDE",    "Cinta c\xc3\xa1lida + soft clip", Vibe::warmTape,  "#0B120B", "#39FF14", false, { "#39FF14","#B6FF3D","#7DFF6A","#2EE6A0","#22C55E","#9BE15D","#5CFF9D","#D9FFD0" }));
    v.push_back (artist ("isla",     "ISLA",     "Hard clip con punch",            Vibe::hardClip,  "#1E2B26", "#7EC8E3", false, { "#FF7A3D","#FFC94D","#FF8A80","#7EC8E3","#B9E3F0","#A6D785","#6CCB7A","#F2F2EC" }));
    v.push_back (artist ("suite",    "SUITE",    "V\xc3\xa1lvula suave",            Vibe::tube,      "#2A0A12", "#D4AF37", false, { "#F4C76B","#E9A27C","#F28BA0","#F7D89A","#E7B6A0","#C99B6D","#F0C27B","#FFF1D6" }));
    v.push_back (artist ("rosa",     "ROSA",     "Exciter brillante",              Vibe::exciter,   "#F3C3D8", "#FF3F98", true,  { "#FF3F98","#FF7AB8","#E23A8A","#C24F8F","#FF9CC8","#C04A8E","#FF6FB0","#B85C8C" }));
    v.push_back (artist ("disco",    "DISCO",    "Consola vieja",                  Vibe::console,   "#170A24", "#FF2BD6", false, { "#FF2BD6","#B46BFF","#FF6AA8","#7DE3FF","#9D8CFF","#E28CFF","#61F2C2","#F3E8FF" }));
    v.push_back (artist ("neblina",  "NEBLINA",  "Cinta oscura lo-fi",             Vibe::darkTape,  "#1A2027", "#9AD8FF", false, { "#9AD8FF","#E6F4FF","#B8C7D9","#7FC4F0","#A9D9F5","#8FA6C1","#B3E1F2","#F1F6FA" }));
    v.push_back (artist ("calle",    "CALLE",    "V\xc3\xa1lvula pesada",           Vibe::heavyTube, "#0D0B0B", "#FF1E2D", false, { "#FF1E2D","#F2F2F2","#FF5A5F","#FF8C6B","#D9D9D9","#B31B24","#FFB3B3","#FFFFFF" }));
    v.push_back (artist ("sabores",  "SABORES",  "Arm\xc3\xb3nicos dulces",         Vibe::sweet,     "#F7EFDF", "#FF6B81", true,  { "#FF6A3D","#F5A623","#FF6B81","#2FB596","#5E9EFF","#9B6BFF","#3FBF7F","#B8A27A" }));
    v.push_back (artist ("espina",   "ESPINA",   "Seda en los medios",             Vibe::silk,      "#140809", "#C2185B", false, { "#D81B60","#E8B04A","#F06292","#F4C06A","#E79B4F","#B0174F","#F28A4C","#F7E3C0" }));
    v.push_back (artist ("billete",  "BILLETE",  "Clip duro, graves gordos",       Vibe::fatClip,   "#0F2619", "#E2B33C", false, { "#E8BE4A","#4CC27A","#E2B33C","#6FD08F","#3FAE6A","#C9A64A","#58C985","#F1E6B8" }));
    v.push_back (artist ("santo",    "SANTO",    "Brillante y limpia",             Vibe::bright,    "#F1EEE8", "#C99A2E", true,  { "#C99A2E","#4FA3D9","#D9A441","#5BB8EA","#5C9DCB","#B8933F","#6FAED6","#A8946A" }));
    v.push_back (artist ("infierno", "INFIERNO", "Fuzz oscuro",                    Vibe::fuzz,      "#0A0A14", "#2D6BFF", false, { "#3D7BFF","#8A4DFF","#B05CFF","#4FC3FF","#6A5BFF","#7A3DD6","#3DE0C8","#C9D6FF" }));
    v.push_back (artist ("bombo",    "BOMBO",    "Glue de bus con punch",          Vibe::glue,      "#0E1116", "#00A3FF", false, { "#00A3FF","#E9F6FF","#5CCBFF","#00D1FF","#4F8BFF","#7FB2FF","#3DE0C8","#FFFFFF" }));
    return v;
}
} // namespace

const std::vector<Skin>& allSkins()
{
    static const std::vector<Skin> skins = build();
    return skins;
}

int skinIndex (const juce::String& id)
{
    const auto& s = allSkins();
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i].id == id) return (int) i;
    return 0;
}

const Skin& skinById (const juce::String& id) { return allSkins()[(size_t) skinIndex (id)]; }

} // namespace eclipse
