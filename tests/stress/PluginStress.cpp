// Host-like stress test for an LT plugin: random params, block sizes, sample rates, MIDI,
// state save/restore and editor open/close. Built with sanitizers to find crashes.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::Random rnd (argc > 1 ? juce::String (argv[1]).getIntValue() : 1234);
    int rounds = argc > 2 ? juce::String (argv[2]).getIntValue() : 40;

    std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());
    p->enableAllBuses();
    std::cout << "plugin: " << p->getName() << "  params: " << p->getParameters().size() << std::endl;

    const double rates[] = { 44100, 48000, 88200, 96000 };
    const int blocks[] = { 32, 64, 128, 256, 441, 512, 1024, 2048 };

    for (int round = 0; round < rounds; ++round)
    {
        auto sr = rates[rnd.nextInt (4)];
        auto maxBlock = blocks[rnd.nextInt (8)];
        p->setRateAndBufferSizeDetails (sr, maxBlock);
        p->prepareToPlay (sr, maxBlock);

        // randomise every parameter (sometimes extremes)
        for (auto* prm : p->getParameters())
        {
            auto r = rnd.nextFloat();
            prm->setValueNotifyingHost (r < 0.1f ? 0.0f : r > 0.9f ? 1.0f : rnd.nextFloat());
        }

        juce::AudioBuffer<float> buf (juce::jmax (p->getTotalNumInputChannels(), p->getTotalNumOutputChannels()), maxBlock);
        for (int b = 0; b < 200; ++b)
        {
            auto n = rnd.nextFloat() < 0.2f ? 1 + rnd.nextInt (maxBlock) : maxBlock;   // hosts sometimes send short blocks
            buf.setSize (buf.getNumChannels(), n, false, false, true);
            for (int c = 0; c < buf.getNumChannels(); ++c)
                for (int i = 0; i < n; ++i)
                    buf.setSample (c, i, (rnd.nextFloat() * 2 - 1) * (b % 50 < 25 ? 0.8f : 0.0f));

            juce::MidiBuffer midi;
            if (rnd.nextFloat() < 0.1f) midi.addEvent (juce::MidiMessage::noteOn (1, 36 + rnd.nextInt (48), (juce::uint8) 100), rnd.nextInt (n));
            if (rnd.nextFloat() < 0.1f) midi.addEvent (juce::MidiMessage::noteOff (1, 36 + rnd.nextInt (48)), rnd.nextInt (n));

            // occasionally tweak a parameter mid-stream
            if (rnd.nextFloat() < 0.3f && ! p->getParameters().isEmpty())
            {
                auto& params = p->getParameters();
                params[rnd.nextInt (params.size())]->setValueNotifyingHost (rnd.nextFloat());
            }

            p->processBlock (buf, midi);

            for (int c = 0; c < buf.getNumChannels(); ++c)
                for (int i = 0; i < n; ++i)
                    if (! std::isfinite (buf.getSample (c, i)))
                    {
                        std::cout << "NaN/Inf output at round " << round << " block " << b << std::endl;
                        return 2;
                    }
        }

        // save / restore state
        juce::MemoryBlock state;
        p->getStateInformation (state);
        p->setStateInformation (state.getData(), (int) state.getSize());
        if (rnd.nextFloat() < 0.3f)   // garbage state must not crash
        {
            juce::MemoryBlock junk (64 + (size_t) rnd.nextInt (512));
            for (size_t i = 0; i < junk.getSize(); ++i) ((char*) junk.getData())[i] = (char) rnd.nextInt (256);
            p->setStateInformation (junk.getData(), (int) junk.getSize());
        }

        // open and close the editor like a host does (on screen, let timers/async callbacks run, then delete)
        if (p->hasEditor())
        {
            for (int k = 0; k < 3; ++k)
            {
                std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditorIfNeeded());
                if (ed)
                {
                    ed->addToDesktop (0);
                    ed->setVisible (true);
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (40 + rnd.nextInt (200));
                    p->editorBeingDeleted (ed.get());
                    ed.reset();
                }
                juce::MessageManager::getInstance()->runDispatchLoopUntil (30);   // async callbacks after close
            }
        }

        p->releaseResources();
        std::cout << "round " << round << " ok (sr " << sr << ", block " << maxBlock << ")" << std::endl;
    }

    p.reset();
    std::cout << "STRESS PASSED" << std::endl;
    return 0;
}
