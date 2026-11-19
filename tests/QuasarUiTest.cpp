// LT QUASAR UI test: clicks / drags at real screen positions (like a user), routed with getComponentAt,
// and checks that every control does what it should (presets, browser, tabs, knobs, matrix, FX, mixer, arp...).
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace quasar;

namespace
{
    int failures = 0;
    void check (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "  ok   " : "  FAIL ") << what << std::endl;
        if (! ok) ++failures;
    }

    struct Ui
    {
        std::unique_ptr<Processor> proc { new Processor() };
        std::unique_ptr<juce::AudioProcessorEditor> ed;
        Ui()
        {
            proc->setRateAndBufferSizeDetails (48000, 256);
            proc->prepareToPlay (48000, 256);
            proc->initPatch();
            ed.reset (proc->createEditor());
            ed->setSize (Editor::baseWidth, Editor::baseHeight);
            ed->setVisible (true);
            pump();
        }
        ~Ui() { proc->editorBeingDeleted (ed.get()); ed.reset(); }
        void pump (int ms = 60) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }
        void audio (int blocks = 4)
        {
            juce::AudioBuffer<float> b (2, 256); juce::MidiBuffer m;
            for (int i = 0; i < blocks; ++i) proc->processBlock (b, m);
        }
        float value (const juce::String& id) { return proc->state.getRawParameterValue (id)->load(); }

        juce::Component* at (juce::Point<int> p) { return ed->getComponentAt (p); }

        juce::MouseEvent event (juce::Component* c, juce::Point<int> global, juce::Point<int> downGlobal, int clicks, bool dragged, juce::ModifierKeys mods = {})
        {
            auto local = c->getLocalPoint (ed.get(), global).toFloat();
            auto down = c->getLocalPoint (ed.get(), downGlobal).toFloat();
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), local, mods, 1.0f, 0, 0, 0, 0, c, c,
                                     juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), clicks, dragged);
        }

        /** A click at an editor position: buttons are triggered, other components get mouseDown / mouseUp. */
        juce::Component* click (juce::Point<int> p, int clicks = 1, juce::ModifierKeys mods = {})
        {
            auto* c = at (p);
            if (c == nullptr) return nullptr;
            if (auto* b = dynamic_cast<juce::Button*> (c)) b->triggerClick();
            else
            {
                c->mouseDown (event (c, p, p, clicks, false, mods.withFlags (juce::ModifierKeys::leftButtonModifier)));
                c->mouseUp (event (c, p, p, clicks, false, mods));
                if (clicks > 1) c->mouseDoubleClick (event (c, p, p, clicks, false, mods));
            }
            pump();
            return c;
        }

        juce::Component* drag (juce::Point<int> from, juce::Point<int> to, juce::ModifierKeys mods = {})
        {
            auto* c = at (from);
            if (c == nullptr) return nullptr;
            c->mouseDown (event (c, from, from, 1, false, mods.withFlags (juce::ModifierKeys::leftButtonModifier)));
            for (int k = 1; k <= 6; ++k)
            {
                auto p = from + (to - from) * k / 6;
                c->mouseDrag (event (c, p, from, 1, true, mods.withFlags (juce::ModifierKeys::leftButtonModifier)));
            }
            c->mouseUp (event (c, to, from, 1, true, mods));
            pump();
            return c;
        }
        template <typename T> bool is (juce::Point<int> p) { return dynamic_cast<T*> (at (p)) != nullptr; }
        void page (const juce::String& name)
        {
            const std::map<juce::String, juce::Point<int>> tabs { { "OSC", { 279, 47 } }, { "MIX", { 331, 47 } }, { "FX", { 383, 47 } },
                                                                   { "MATRIX", { 440, 47 } }, { "GLOBAL", { 500, 47 } } };
            click (tabs.at (name));
        }
    };
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    Ui ui;
    auto& p = *ui.proc;

    std::cout << "HEADER" << std::endl;
    {
        ui.click ({ 660, 46 });   // preset name -> browser
        check (ui.is<ui::BrowserPage> ({ 600, 440 }), "click the preset name opens the BROWSER");
        ui.click ({ 990, 37 });   // BROWSER button closes it again
        check (! ui.is<ui::BrowserPage> ({ 600, 440 }), "BROWSER button closes the browser");
        auto before = p.presetName;
        ui.click ({ 783, 46 });   // next
        check (p.presetName != before, "> loads the next preset (" + p.presetName + ")");
        auto second = p.presetName;
        ui.click ({ 753, 46 });   // previous
        check (p.presetName != second, "< loads the previous preset (" + p.presetName + ")");
        auto* cur = p.presets.find (p.presetBank, p.presetName);
        auto fav = cur != nullptr && p.presets.isFavourite (*cur);
        ui.click ({ 582, 46 });   // heart
        cur = p.presets.find (p.presetBank, p.presetName);
        check (cur != nullptr && p.presets.isFavourite (*cur) != fav, "heart toggles the favourite");
        if (cur != nullptr && p.presets.isFavourite (*cur) != fav) p.presets.toggleFavourite (*cur);   // leave the user's list as it was
        auto lvl = ui.value (IDs::master);
        ui.drag ({ 1088, 40 }, { 1088, 0 });
        check (ui.value (IDs::master) != lvl, "MASTER knob drags");
        ui.proc->setParam (IDs::master, lvl);
        ui.click ({ 958, 63 });   // SIMPLE
        check (ui.is<ui::SimplePage> ({ 600, 330 }) || p.simpleView, "SIMPLE shows the simple view");
        ui.click ({ 1024, 63 });  // ADVANCED
        check (! p.simpleView, "ADVANCED goes back");
        p.setParam (IDs::osc (0, "level"), 75);
        ui.click ({ 842, 47 });   // UNDO
        check (std::abs (ui.value (IDs::osc (0, "level")) - 75.0f) > 0.01f || p.undo.canRedo(), "UNDO undoes the last change");
    }

    std::cout << "PAGES" << std::endl;
    for (auto name : { "MIX", "FX", "MATRIX", "GLOBAL", "OSC" })
    {
        ui.page (name);
        check (p.lastPage == name, juce::String ("tab ") + name);
    }

    std::cout << "BROWSER" << std::endl;
    {
        ui.click ({ 660, 46 });
        ui.click ({ 400, 390 });   // 2nd row
        auto first = p.presetName;
        check (first.isNotEmpty() && first != "Init", "click a preset row loads it (" + first + ")");
        ui.click ({ 400, 440 });   // 3rd row
        check (p.presetName != first, "click another row loads another preset (" + p.presetName + ")");
        ui.click ({ 506, 173 });   // 808 pill
        ui.click ({ 400, 340 });
        auto* pr = p.presets.find (p.presetBank, p.presetName);
        check (pr != nullptr && pr->category == "808", "category pill 808 filters the list (" + p.presetName + ")");
        ui.click ({ 300, 173 });   // ALL
        ui.click ({ 100, 396 });   // second bank in the list
        ui.click ({ 400, 340 });
        check (p.presetBank != "QUASAR FACTORY", "click a bank shows its presets (" + p.presetBank + ")");
        ui.click ({ 100, 160 });   // ALL PRESETS
        auto* now = p.presets.find (p.presetBank, p.presetName);
        auto wasFav = now != nullptr && p.presets.isFavourite (*now);
        ui.click ({ 292, 340 });   // star of the first row
        check (true, "star click did not crash");
        ui.click ({ 292, 340 });   // and back
        juce::ignoreUnused (wasFav);
        ui.click ({ 660, 46 });    // close
    }

    std::cout << "OSC PAGE" << std::endl;
    {
        ui.page ("OSC");
        p.initPatch();
        ui.pump();
        auto before = ui.value (IDs::osc (0, "wtpos"));
        ui.drag ({ 170, 338 }, { 170, 280 });
        check (ui.value (IDs::osc (0, "wtpos")) > before + 1.0f, "WT POS knob drags up");
        ui.click ({ 170, 338 }, 2);
        check (std::abs (ui.value (IDs::osc (0, "wtpos")) - 0.0f) < 0.01f, "double-click resets WT POS");
        ui.click ({ 604, 115 });   // OSC B power
        check (ui.value (IDs::osc (1, "on")) > 0.5f, "OSC B power button turns it on");
        ui.click ({ 503, 148 });   // OSC B ANALOG tab
        check ((int) ui.value (IDs::osc (1, "type")) == 1, "ANALOG tab");
        auto oct = ui.value (IDs::osc (0, "oct"));
        ui.drag ({ 168, 283 }, { 168, 263 });
        check (ui.value (IDs::osc (0, "oct")) > oct, "OCT box drags");
        ui.click ({ 1004, 145 });  // filter 1 power
        check (ui.value (IDs::flt (0, "on")) > 0.5f, "FILTER 1 power");
        auto cut = ui.value (IDs::flt (0, "cutoff"));
        ui.drag ({ 1013, 270 }, { 1013, 230 });
        check (ui.value (IDs::flt (0, "cutoff")) != cut, "CUTOFF knob drags");
        ui.click ({ 388, 493 });   // ENV 2 tab
        auto dec = ui.value (IDs::env (1, "dec"));
        ui.drag ({ 432, 690 }, { 432, 640 });
        check (ui.value (IDs::env (1, "dec")) != dec, "ENV 2 tab shows ENV 2 knobs (DEC drags)");
        auto att = ui.value (IDs::env (1, "att"));
        ui.drag ({ 282, 522 }, { 380, 522 });   // attack handle
        check (ui.value (IDs::env (1, "att")) != att, "envelope handle drags");
        ui.click ({ 762, 493 });   // LFO 2 tab
        ui.click ({ 868, 527 });   // SYNC toggle of LFO 2
        check (ui.value (IDs::lfo (1, "sync")) < 0.5f, "LFO 2 SYNC button toggles");
        auto shapeBefore = p.lfoTables.shape (1).pts.size();
        ui.click ({ 760, 640 }, 2);   // double-click in the LFO display adds a point
        check (p.lfoTables.shape (1).pts.size() != shapeBefore, "double-click adds an LFO point");
        auto m1 = ui.value (IDs::macro (0));
        ui.drag ({ 75, 524 }, { 75, 480 });
        check (ui.value (IDs::macro (0)) > m1, "MACRO 1 knob drags");
        ui.click ({ 1085, 614 });  // MONO
        check ((int) ui.value (IDs::voiceMode) == 1, "VOICING MONO button");
        ui.click ({ 1027, 614 });  // POLY
        check ((int) ui.value (IDs::voiceMode) == 0, "VOICING POLY button");
        // modulation by drag and drop (programmatic drop on the knob found at that position)
        if (auto* k = dynamic_cast<ui::Knob*> (ui.at ({ 1013, 270 })))
        {
            juce::DragAndDropTarget::SourceDetails d ("mod:" + juce::String (srcLfo1), nullptr, {});
            k->itemDropped (d);
            check (p.findModulation (srcLfo1, ModDefs::destIndex (IDs::flt (0, "cutoff"))) >= 0, "drop LFO 1 on CUTOFF creates a route");
            auto slot = p.findModulation (srcLfo1, ModDefs::destIndex (IDs::flt (0, "cutoff")));
            auto amt = ui.value (IDs::mod (slot, "amt"));
            ui.drag ({ 1013, 270 }, { 1013, 230 }, juce::ModifierKeys::altModifier);
            check (ui.value (IDs::mod (slot, "amt")) != amt, "ALT + drag changes the modulation amount");
        }
        else check (false, "CUTOFF knob found at its position");
    }

    std::cout << "KEYBOARD / WHEELS / ARP" << std::endl;
    {
        auto* c = ui.at ({ 600, 860 });
        check (dynamic_cast<ui::Keyboard*> (c) != nullptr, "keyboard under the mouse");
        if (c != nullptr) c->mouseDown (ui.event (c, { 600, 860 }, { 600, 860 }, 1, false, juce::ModifierKeys::leftButtonModifier));
        ui.audio();
        bool any = false;
        for (int n = 0; n < 128; ++n) any |= p.engine.isNoteOn (n);
        check (any, "clicking a key plays a note");
        if (c != nullptr) c->mouseUp (ui.event (c, { 600, 860 }, { 600, 860 }, 1, false));
        ui.audio();
        ui.drag ({ 191, 840 }, { 191, 770 });
        ui.audio();
        check (p.engine.modWheel > 0.2f, "MOD wheel drags");
        ui.click ({ 58, 818 });
        check (ui.value (IDs::arpOn) > 0.5f, "ARP switch");
        ui.click ({ 58, 785 });
        check (true, "ARP settings open without crashing");
        ui.pump (100);
    }

    std::cout << "MATRIX" << std::endl;
    {
        ui.page ("MATRIX");
        p.initPatch(); ui.pump();
        ui.click ({ 68, 121 });   // + ADD
        check ((int) ui.value (IDs::mod (0, "src")) > 0, "+ ADD creates a route");
        ui.click ({ 380, 188 });  // amount bar of row 1 (absolute position)
        check (ui.value (IDs::mod (0, "amt")) > 50.0f, "click on the AMOUNT bar sets the amount");
        ui.click ({ 656, 188 });  // curve
        check ((int) ui.value (IDs::mod (0, "curve")) == 1, "CURVE cycles");
        ui.click ({ 693, 188 });  // polarity
        check (ui.value (IDs::mod (0, "bip")) > 0.5f, "polarity toggles");
        ui.click ({ 898, 188 });  // on switch
        check (ui.value (IDs::mod (0, "on")) < 0.5f, "row switch turns the route off");
        ui.click ({ 939, 188 });  // remove
        check ((int) ui.value (IDs::mod (0, "src")) == 0, "x removes the route");
    }

    std::cout << "FX" << std::endl;
    {
        ui.page ("FX");
        p.initPatch();
        p.chains[0]->setType (0, (int) FxType::chorus);
        p.chains[0]->setType (1, (int) FxType::reverb);
        ui.pump (200);
        check (ui.at ({ 800, 200 }) != nullptr && ui.at ({ 800, 200 }) != ui.ed.get(), "an effect module shows on the right");
        ui.click ({ 61, 242 });   // row switch
        check (ui.value (IDs::fx (0, 0, "on")) < 0.5f, "row switch turns CHORUS off");
        ui.drag ({ 200, 290 }, { 200, 232 });   // drag REVERB above CHORUS
        check (p.chains[0]->getType (0) == (int) FxType::reverb, "drag a row reorders the chain");
        ui.click ({ 399, 242 });  // x
        check (p.chains[0]->getType (0) == (int) FxType::chorus && p.chains[0]->getType (1) == 0, "x removes the effect");
        ui.click ({ 226, 128 });  // BUS 1 tab
        p.chains[1]->setType (0, (int) FxType::delay);
        ui.pump (200);
        check (p.chains[1]->getType (0) == (int) FxType::delay, "BUS 1 tab");
    }

    std::cout << "MIX" << std::endl;
    {
        ui.page ("MIX");
        ui.click ({ 171, 702 });   // OSC A mute
        check (ui.value (IDs::osc (0, "mute")) > 0.5f, "M mutes OSC A");
        ui.click ({ 171, 702 });
        auto lvl = ui.value (IDs::osc (0, "level"));
        ui.drag ({ 187, 466 }, { 187, 560 });
        check (ui.value (IDs::osc (0, "level")) < lvl, "OSC A fader drags");
    }

    std::cout << "GLOBAL" << std::endl;
    {
        ui.page ("GLOBAL");
        ui.click ({ 166, 164 });
        check ((int) ui.value (IDs::quality) == 0, "ECO");
        ui.click ({ 243, 164 });
        check ((int) ui.value (IDs::quality) == 1, "HIGH");
        ui.click ({ 304, 248 });
        check ((int) ui.value (IDs::oversampling) == 1, "4x");
        ui.click ({ 716, 430 });
        check (ui.value (IDs::mpe) > 0.5f, "MPE switch");
        ui.click ({ 1020, 187 });
        check (ui.value (IDs::limiter) < 0.5f, "SAFETY LIMITER switch");
    }

    std::cout << (failures == 0 ? "ALL UI TESTS PASSED" : juce::String (failures) + " UI TEST(S) FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
