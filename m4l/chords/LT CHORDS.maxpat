{
 "patcher": {
  "fileversion": 1,
  "appversion": {
   "major": 8,
   "minor": 5,
   "revision": 5,
   "architecture": "x64",
   "modernui": 1
  },
  "classnamespace": "box",
  "rect": [
   100.0,
   100.0,
   900.0,
   560.0
  ],
  "openinpresentation": 1,
  "default_fontsize": 10.0,
  "default_fontface": 0,
  "default_fontname": "Arial",
  "gridonopen": 1,
  "gridsize": [
   8.0,
   8.0
  ],
  "devicewidth": 380.0,
  "description": "LT CHORDS: one note in, a chord out, plus the chord that usually comes next in your genre",
  "boxes": [
   {
    "box": {
     "id": "bg",
     "maxclass": "panel",
     "patching_rect": [
      0,
      0,
      380,
      169
     ],
     "presentation": 1,
     "presentation_rect": [
      0,
      0,
      380,
      169
     ],
     "numinlets": 1,
     "numoutlets": 0,
     "bgcolor": [
      0.05,
      0.04,
      0.09,
      1.0
     ],
     "background": 1,
     "border": 0,
     "rounded": 0
    }
   },
   {
    "box": {
     "id": "title",
     "maxclass": "comment",
     "patching_rect": [
      20,
      20,
      100,
      18
     ],
     "presentation": 1,
     "presentation_rect": [
      10,
      6,
      120,
      16
     ],
     "text": "LT CHORDS",
     "fontname": "Arial Bold",
     "fontsize": 10.5,
     "textcolor": [
      0.62,
      0.58,
      0.72,
      1.0
     ],
     "numinlets": 1,
     "numoutlets": 0
    }
   },
   {
    "box": {
     "id": "chordLabel",
     "maxclass": "comment",
     "patching_rect": [
      20,
      44,
      170,
      36
     ],
     "presentation": 1,
     "presentation_rect": [
      10,
      22,
      176,
      38
     ],
     "text": "LT CHORDS",
     "fontname": "Arial Bold",
     "fontsize": 26.0,
     "textcolor": [
      0.93,
      0.91,
      0.97,
      1.0
     ],
     "numinlets": 1,
     "numoutlets": 0
    }
   },
   {
    "box": {
     "id": "quality",
     "maxclass": "live.tab",
     "patching_rect": [
      220,
      20,
      180,
      18
     ],
     "presentation": 1,
     "presentation_rect": [
      190,
      8,
      182,
      18
     ],
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "",
      "",
      "float"
     ],
     "num_lines_patching": 1,
     "num_lines_presentation": 1,
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Calidad",
       "parameter_shortname": "Calidad",
       "parameter_type": 2,
       "parameter_enum": [
        "MAYOR",
        "MENOR",
        "EN TONO"
       ],
       "parameter_mmax": 2,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        0
       ]
      }
     },
     "varname": "Calidad",
     "fontname": "Arial Bold",
     "fontsize": 9.5,
     "activebgoncolor": [
      0.48,
      0.17,
      1.0,
      1.0
     ],
     "bgoncolor": [
      0.48,
      0.17,
      1.0,
      1.0
     ],
     "activetextoncolor": [
      1,
      1,
      1,
      1
     ],
     "textoncolor": [
      1,
      1,
      1,
      1
     ],
     "bgcolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ],
     "activebgcolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ],
     "textcolor": [
      0.93,
      0.91,
      0.97,
      1.0
     ],
     "activetextcolor": [
      0.93,
      0.91,
      0.97,
      1.0
     ],
     "bordercolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ]
    }
   },
   {
    "box": {
     "id": "key",
     "maxclass": "live.menu",
     "patching_rect": [
      220,
      44,
      56,
      18
     ],
     "presentation": 1,
     "presentation_rect": [
      190,
      32,
      52,
      18
     ],
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "",
      "",
      "float"
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Tonalidad",
       "parameter_shortname": "Tonalidad",
       "parameter_type": 2,
       "parameter_enum": [
        "C",
        "C#",
        "D",
        "Eb",
        "E",
        "F",
        "F#",
        "G",
        "Ab",
        "A",
        "Bb",
        "B"
       ],
       "parameter_mmax": 11,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        0
       ]
      }
     },
     "varname": "Tonalidad",
     "fontsize": 9.5,
     "activebgcolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ],
     "bgcolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ],
     "textcolor": [
      0.93,
      0.91,
      0.97,
      1.0
     ],
     "activetextcolor": [
      0.93,
      0.91,
      0.97,
      1.0
     ]
    }
   },
   {
    "box": {
     "id": "scale",
     "maxclass": "live.tab",
     "patching_rect": [
      280,
      44,
      70,
      18
     ],
     "presentation": 1,
     "presentation_rect": [
      246,
      32,
      72,
      18
     ],
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "",
      "",
      "float"
     ],
     "num_lines_patching": 1,
     "num_lines_presentation": 1,
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Escala",
       "parameter_shortname": "Escala",
       "parameter_type": 2,
       "parameter_enum": [
        "MAY",
        "MEN"
       ],
       "parameter_mmax": 1,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        0
       ]
      }
     },
     "varname": "Escala",
     "fontname": "Arial Bold",
     "fontsize": 9.5,
     "activebgoncolor": [
      0.48,
      0.17,
      1.0,
      1.0
     ],
     "bgoncolor": [
      0.48,
      0.17,
      1.0,
      1.0
     ],
     "activetextoncolor": [
      1,
      1,
      1,
      1
     ],
     "textoncolor": [
      1,
      1,
      1,
      1
     ],
     "bgcolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ],
     "activebgcolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ],
     "textcolor": [
      0.93,
      0.91,
      0.97,
      1.0
     ],
     "activetextcolor": [
      0.93,
      0.91,
      0.97,
      1.0
     ],
     "bordercolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ]
    }
   },
   {
    "box": {
     "id": "dom",
     "maxclass": "live.text",
     "patching_rect": [
      356,
      44,
      44,
      18
     ],
     "presentation": 1,
     "presentation_rect": [
      322,
      32,
      50,
      18
     ],
     "text": "DOM7",
     "texton": "DOM7",
     "mode": 1,
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      ""
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Dom7",
       "parameter_shortname": "Dom7",
       "parameter_type": 2,
       "parameter_enum": [
        "off",
        "on"
       ],
       "parameter_mmax": 1,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        0
       ]
      }
     },
     "varname": "Dom7",
     "fontname": "Arial Bold",
     "fontsize": 10.0,
     "rounded": 5.0,
     "bgcolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ],
     "bgoncolor": [
      0.2,
      0.83,
      0.6,
      1.0
     ],
     "activebgcolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ],
     "activebgoncolor": [
      0.2,
      0.83,
      0.6,
      1.0
     ],
     "textcolor": [
      0.93,
      0.91,
      0.97,
      1.0
     ],
     "textoncolor": [
      1,
      1,
      1,
      1
     ],
     "activetextcolor": [
      0.93,
      0.91,
      0.97,
      1.0
     ],
     "activetextoncolor": [
      1,
      1,
      1,
      1
     ],
     "bordercolor": [
      0.2,
      0.83,
      0.6,
      1.0
     ],
     "focusbordercolor": [
      0.2,
      0.83,
      0.6,
      1.0
     ]
    }
   },
   {
    "box": {
     "id": "seventh",
     "maxclass": "live.text",
     "patching_rect": [
      20,
      90,
      50,
      22
     ],
     "presentation": 1,
     "presentation_rect": [
      10,
      62,
      52,
      22
     ],
     "text": "7a",
     "texton": "7a",
     "mode": 1,
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      ""
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Septima",
       "parameter_shortname": "Septima",
       "parameter_type": 2,
       "parameter_enum": [
        "off",
        "on"
       ],
       "parameter_mmax": 1,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        0
       ]
      }
     },
     "varname": "Septima",
     "fontname": "Arial Bold",
     "fontsize": 10.0,
     "rounded": 5.0,
     "bgcolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ],
     "bgoncolor": [
      0.48,
      0.17,
      1.0,
      1.0
     ],
     "activebgcolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ],
     "activebgoncolor": [
      0.48,
      0.17,
      1.0,
      1.0
     ],
     "textcolor": [
      0.93,
      0.91,
      0.97,
      1.0
     ],
     "textoncolor": [
      1,
      1,
      1,
      1
     ],
     "activetextcolor": [
      0.93,
      0.91,
      0.97,
      1.0
     ],
     "activetextoncolor": [
      1,
      1,
      1,
      1
     ],
     "bordercolor": [
      0.48,
      0.17,
      1.0,
      1.0
     ],
     "focusbordercolor": [
      0.48,
      0.17,
      1.0,
      1.0
     ]
    }
   },
   {
    "box": {
     "id": "ninth",
     "maxclass": "live.text",
     "patching_rect": [
      74,
      90,
      50,
      22
     ],
     "presentation": 1,
     "presentation_rect": [
      66,
      62,
      52,
      22
     ],
     "text": "9a",
     "texton": "9a",
     "mode": 1,
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      ""
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Novena",
       "parameter_shortname": "Novena",
       "parameter_type": 2,
       "parameter_enum": [
        "off",
        "on"
       ],
       "parameter_mmax": 1,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        0
       ]
      }
     },
     "varname": "Novena",
     "fontname": "Arial Bold",
     "fontsize": 10.0,
     "rounded": 5.0,
     "bgcolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ],
     "bgoncolor": [
      0.48,
      0.17,
      1.0,
      1.0
     ],
     "activebgcolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ],
     "activebgoncolor": [
      0.48,
      0.17,
      1.0,
      1.0
     ],
     "textcolor": [
      0.93,
      0.91,
      0.97,
      1.0
     ],
     "textoncolor": [
      1,
      1,
      1,
      1
     ],
     "activetextcolor": [
      0.93,
      0.91,
      0.97,
      1.0
     ],
     "activetextoncolor": [
      1,
      1,
      1,
      1
     ],
     "bordercolor": [
      0.48,
      0.17,
      1.0,
      1.0
     ],
     "focusbordercolor": [
      0.48,
      0.17,
      1.0,
      1.0
     ]
    }
   },
   {
    "box": {
     "id": "octave",
     "maxclass": "live.text",
     "patching_rect": [
      128,
      90,
      50,
      22
     ],
     "presentation": 1,
     "presentation_rect": [
      122,
      62,
      52,
      22
     ],
     "text": "8a",
     "texton": "8a",
     "mode": 1,
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      ""
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Octava",
       "parameter_shortname": "Octava",
       "parameter_type": 2,
       "parameter_enum": [
        "off",
        "on"
       ],
       "parameter_mmax": 1,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        0
       ]
      }
     },
     "varname": "Octava",
     "fontname": "Arial Bold",
     "fontsize": 10.0,
     "rounded": 5.0,
     "bgcolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ],
     "bgoncolor": [
      0.48,
      0.17,
      1.0,
      1.0
     ],
     "activebgcolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ],
     "activebgoncolor": [
      0.48,
      0.17,
      1.0,
      1.0
     ],
     "textcolor": [
      0.93,
      0.91,
      0.97,
      1.0
     ],
     "textoncolor": [
      1,
      1,
      1,
      1
     ],
     "activetextcolor": [
      0.93,
      0.91,
      0.97,
      1.0
     ],
     "activetextoncolor": [
      1,
      1,
      1,
      1
     ],
     "bordercolor": [
      0.48,
      0.17,
      1.0,
      1.0
     ],
     "focusbordercolor": [
      0.48,
      0.17,
      1.0,
      1.0
     ]
    }
   },
   {
    "box": {
     "id": "genre",
     "maxclass": "live.menu",
     "patching_rect": [
      220,
      90,
      180,
      20
     ],
     "presentation": 1,
     "presentation_rect": [
      190,
      63,
      182,
      20
     ],
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "",
      "",
      "float"
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Genero",
       "parameter_shortname": "Genero",
       "parameter_type": 2,
       "parameter_enum": [
        "REGGAETON",
        "TRAP",
        "R&B",
        "POP",
        "LO-FI",
        "BACHATA",
        "AFRO",
        "HOUSE"
       ],
       "parameter_mmax": 7,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        0
       ]
      }
     },
     "varname": "Genero",
     "fontsize": 9.5,
     "activebgcolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ],
     "bgcolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ],
     "textcolor": [
      0.93,
      0.91,
      0.97,
      1.0
     ],
     "activetextcolor": [
      0.93,
      0.91,
      0.97,
      1.0
     ]
    }
   },
   {
    "box": {
     "id": "info",
     "maxclass": "textedit",
     "patching_rect": [
      20,
      130,
      380,
      74
     ],
     "presentation": 1,
     "presentation_rect": [
      10,
      90,
      362,
      72
     ],
     "numinlets": 1,
     "numoutlets": 4,
     "outlettype": [
      "",
      "int",
      "",
      ""
     ],
     "readonly": 1,
     "fontname": "Arial",
     "fontsize": 9.5,
     "textcolor": [
      0.93,
      0.91,
      0.97,
      1.0
     ],
     "bgcolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ],
     "bordercolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ],
     "rounded": 6.0,
     "wordwrap": 1,
     "text": "Toca una nota: sale un acorde."
    }
   },
   {
    "box": {
     "id": "midiin",
     "maxclass": "newobj",
     "patching_rect": [
      460,
      20,
      50,
      22
     ],
     "text": "midiin",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      "int"
     ]
    }
   },
   {
    "box": {
     "id": "parse",
     "maxclass": "newobj",
     "patching_rect": [
      460,
      50,
      120,
      22
     ],
     "text": "midiparse",
     "numinlets": 1,
     "numoutlets": 8,
     "outlettype": [
      "",
      "",
      "",
      "int",
      "int",
      "int",
      "int",
      ""
     ]
    }
   },
   {
    "box": {
     "id": "js",
     "maxclass": "newobj",
     "patching_rect": [
      460,
      200,
      180,
      22
     ],
     "text": "js lt_chords.js",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "",
      "",
      ""
     ]
    }
   },
   {
    "box": {
     "id": "format",
     "maxclass": "newobj",
     "patching_rect": [
      460,
      250,
      120,
      22
     ],
     "text": "midiformat",
     "numinlets": 7,
     "numoutlets": 2,
     "outlettype": [
      "int",
      ""
     ]
    }
   },
   {
    "box": {
     "id": "midiout",
     "maxclass": "newobj",
     "patching_rect": [
      460,
      280,
      60,
      22
     ],
     "text": "midiout",
     "numinlets": 1,
     "numoutlets": 0,
     "outlettype": []
    }
   },
   {
    "box": {
     "id": "pre_quality",
     "maxclass": "newobj",
     "patching_rect": [
      700,
      20,
      100,
      22
     ],
     "text": "prepend quality",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "pre_key",
     "maxclass": "newobj",
     "patching_rect": [
      700,
      48,
      100,
      22
     ],
     "text": "prepend key",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "pre_scale",
     "maxclass": "newobj",
     "patching_rect": [
      700,
      76,
      100,
      22
     ],
     "text": "prepend scale",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "pre_dom",
     "maxclass": "newobj",
     "patching_rect": [
      700,
      104,
      100,
      22
     ],
     "text": "prepend dom",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "pre_seventh",
     "maxclass": "newobj",
     "patching_rect": [
      700,
      132,
      100,
      22
     ],
     "text": "prepend seventh",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "pre_ninth",
     "maxclass": "newobj",
     "patching_rect": [
      700,
      160,
      100,
      22
     ],
     "text": "prepend ninth",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "pre_octave",
     "maxclass": "newobj",
     "patching_rect": [
      700,
      188,
      100,
      22
     ],
     "text": "prepend octave",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "pre_genre",
     "maxclass": "newobj",
     "patching_rect": [
      700,
      216,
      100,
      22
     ],
     "text": "prepend genre",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "thisdev",
     "maxclass": "newobj",
     "patching_rect": [
      860,
      20,
      90,
      22
     ],
     "text": "live.thisdevice",
     "numinlets": 1,
     "numoutlets": 3,
     "outlettype": [
      "bang",
      "int",
      "int"
     ]
    }
   },
   {
    "box": {
     "id": "fan",
     "maxclass": "newobj",
     "patching_rect": [
      860,
      50,
      220,
      22
     ],
     "text": "t b b b b b b b b b",
     "numinlets": 1,
     "numoutlets": 9,
     "outlettype": [
      "bang",
      "bang",
      "bang",
      "bang",
      "bang",
      "bang",
      "bang",
      "bang",
      "bang"
     ]
    }
   },
   {
    "box": {
     "id": "msgInit",
     "maxclass": "message",
     "patching_rect": [
      1090,
      80,
      40,
      22
     ],
     "text": "init",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "msgPanic",
     "maxclass": "message",
     "patching_rect": [
      860,
      110,
      50,
      22
     ],
     "text": "panic",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "selOff",
     "maxclass": "newobj",
     "patching_rect": [
      860,
      80,
      40,
      22
     ],
     "text": "sel 0",
     "numinlets": 2,
     "numoutlets": 2,
     "outlettype": [
      "bang",
      ""
     ]
    }
   }
  ],
  "lines": [
   {
    "patchline": {
     "source": [
      "midiin",
      0
     ],
     "destination": [
      "parse",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "parse",
      0
     ],
     "destination": [
      "js",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "parse",
      1
     ],
     "destination": [
      "format",
      1
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "parse",
      2
     ],
     "destination": [
      "format",
      2
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "parse",
      3
     ],
     "destination": [
      "format",
      3
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "parse",
      4
     ],
     "destination": [
      "format",
      4
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "parse",
      5
     ],
     "destination": [
      "format",
      5
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "js",
      0
     ],
     "destination": [
      "format",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "format",
      0
     ],
     "destination": [
      "midiout",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "js",
      1
     ],
     "destination": [
      "chordLabel",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "js",
      2
     ],
     "destination": [
      "info",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "quality",
      0
     ],
     "destination": [
      "pre_quality",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "pre_quality",
      0
     ],
     "destination": [
      "js",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "key",
      0
     ],
     "destination": [
      "pre_key",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "pre_key",
      0
     ],
     "destination": [
      "js",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "scale",
      0
     ],
     "destination": [
      "pre_scale",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "pre_scale",
      0
     ],
     "destination": [
      "js",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "dom",
      0
     ],
     "destination": [
      "pre_dom",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "pre_dom",
      0
     ],
     "destination": [
      "js",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "seventh",
      0
     ],
     "destination": [
      "pre_seventh",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "pre_seventh",
      0
     ],
     "destination": [
      "js",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "ninth",
      0
     ],
     "destination": [
      "pre_ninth",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "pre_ninth",
      0
     ],
     "destination": [
      "js",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "octave",
      0
     ],
     "destination": [
      "pre_octave",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "pre_octave",
      0
     ],
     "destination": [
      "js",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "genre",
      0
     ],
     "destination": [
      "pre_genre",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "pre_genre",
      0
     ],
     "destination": [
      "js",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "thisdev",
      0
     ],
     "destination": [
      "fan",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "fan",
      8
     ],
     "destination": [
      "msgInit",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "msgInit",
      0
     ],
     "destination": [
      "js",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "fan",
      0
     ],
     "destination": [
      "quality",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "fan",
      1
     ],
     "destination": [
      "key",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "fan",
      2
     ],
     "destination": [
      "scale",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "fan",
      3
     ],
     "destination": [
      "dom",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "fan",
      4
     ],
     "destination": [
      "seventh",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "fan",
      5
     ],
     "destination": [
      "ninth",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "fan",
      6
     ],
     "destination": [
      "octave",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "fan",
      7
     ],
     "destination": [
      "genre",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "thisdev",
      1
     ],
     "destination": [
      "selOff",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "selOff",
      0
     ],
     "destination": [
      "msgPanic",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "msgPanic",
      0
     ],
     "destination": [
      "js",
      0
     ]
    }
   }
  ],
  "dependency_cache": [
   {
    "name": "lt_chords.js",
    "type": "TEXT",
    "implicit": 1
   }
  ],
  "autosave": 0
 }
}
