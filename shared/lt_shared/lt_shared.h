/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 lt_shared
  vendor:             LT Audio
  version:            0.1.0
  name:               LT Shared
  description:        Licensing, theme, fonts and space visuals shared by all LT products.
  license:            Proprietary
  minimumCppStandard: 17

  dependencies:       juce_gui_basics, juce_cryptography

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once
#define LT_SHARED_H_INCLUDED

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_cryptography/juce_cryptography.h>

#include "licensing/LicenseCore.h"
#include "licensing/LicenseManager.h"
#include "ui/Palette.h"
#include "ui/Fonts.h"
#include "ui/Glass.h"
#include "ui/NebulaBackground.h"
#include "ui/GlassLookAndFeel.h"
#include "ui/Logo.h"
#include "ui/ActivationComponent.h"
