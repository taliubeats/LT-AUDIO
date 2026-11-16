/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 lt_dsp
  vendor:             LT Audio
  version:            0.1.0
  name:               LT DSP
  description:        Audio pieces shared by the LT plugins: Frequency Focus (band split + spectrum + window).
  license:            Proprietary
  minimumCppStandard: 17

  dependencies:       lt_shared, juce_dsp, juce_audio_processors

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once
#define LT_DSP_H_INCLUDED

#include <lt_shared/lt_shared.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/FocusSplitter.h"
#include "dsp/SpectrumAnalyser.h"
#include "ui/FrequencyFocusView.h"
