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
  "devicewidth": 250.0,
  "description": "LT CLIP GAIN: exact dB steps for the selected audio clip, mappable to keys",
  "boxes": [
   {
    "box": {
     "id": "bg",
     "maxclass": "panel",
     "patching_rect": [
      0,
      0,
      250,
      169
     ],
     "presentation": 1,
     "presentation_rect": [
      0,
      0,
      250,
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
      120,
      18
     ],
     "presentation": 1,
     "presentation_rect": [
      10,
      6,
      140,
      16
     ],
     "text": "LT CLIP GAIN",
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
     "id": "clipName",
     "maxclass": "comment",
     "patching_rect": [
      20,
      40,
      220,
      18
     ],
     "presentation": 1,
     "presentation_rect": [
      10,
      24,
      230,
      18
     ],
     "text": "No clip selected",
     "fontname": "Arial",
     "fontsize": 10.0,
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
     "id": "gain",
     "maxclass": "comment",
     "patching_rect": [
      20,
      60,
      220,
      36
     ],
     "presentation": 1,
     "presentation_rect": [
      10,
      42,
      230,
      38
     ],
     "text": "-",
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
     "id": "m3",
     "maxclass": "live.text",
     "patching_rect": [
      20,
      110,
      54,
      24
     ],
     "presentation": 1,
     "presentation_rect": [
      10,
      88,
      54,
      28
     ],
     "text": "-3 dB",
     "texton": "-3 dB",
     "mode": 0,
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      ""
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Gain -3 dB",
       "parameter_shortname": "Gain -3 dB",
       "parameter_type": 2,
       "parameter_enum": [
        "off",
        "on"
       ],
       "parameter_mmax": 1
      }
     },
     "varname": "Gain -3 dB",
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
      0.97,
      0.44,
      0.44,
      1.0
     ],
     "activebgcolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ],
     "activebgoncolor": [
      0.97,
      0.44,
      0.44,
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
      0.97,
      0.44,
      0.44,
      1.0
     ],
     "focusbordercolor": [
      0.97,
      0.44,
      0.44,
      1.0
     ]
    }
   },
   {
    "box": {
     "id": "t_m3",
     "maxclass": "newobj",
     "patching_rect": [
      20,
      150,
      30,
      22
     ],
     "text": "t b",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      "bang"
     ]
    }
   },
   {
    "box": {
     "id": "msg_m3",
     "maxclass": "message",
     "patching_rect": [
      20,
      180,
      50,
      22
     ],
     "text": "step -3",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "m1",
     "maxclass": "live.text",
     "patching_rect": [
      80,
      110,
      54,
      24
     ],
     "presentation": 1,
     "presentation_rect": [
      68,
      88,
      54,
      28
     ],
     "text": "-1 dB",
     "texton": "-1 dB",
     "mode": 0,
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      ""
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Gain -1 dB",
       "parameter_shortname": "Gain -1 dB",
       "parameter_type": 2,
       "parameter_enum": [
        "off",
        "on"
       ],
       "parameter_mmax": 1
      }
     },
     "varname": "Gain -1 dB",
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
      0.97,
      0.44,
      0.44,
      1.0
     ],
     "activebgcolor": [
      0.1,
      0.09,
      0.15,
      1.0
     ],
     "activebgoncolor": [
      0.97,
      0.44,
      0.44,
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
      0.97,
      0.44,
      0.44,
      1.0
     ],
     "focusbordercolor": [
      0.97,
      0.44,
      0.44,
      1.0
     ]
    }
   },
   {
    "box": {
     "id": "t_m1",
     "maxclass": "newobj",
     "patching_rect": [
      80,
      150,
      30,
      22
     ],
     "text": "t b",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      "bang"
     ]
    }
   },
   {
    "box": {
     "id": "msg_m1",
     "maxclass": "message",
     "patching_rect": [
      80,
      180,
      50,
      22
     ],
     "text": "step -1",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "p1",
     "maxclass": "live.text",
     "patching_rect": [
      140,
      110,
      54,
      24
     ],
     "presentation": 1,
     "presentation_rect": [
      126,
      88,
      54,
      28
     ],
     "text": "+1 dB",
     "texton": "+1 dB",
     "mode": 0,
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      ""
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Gain +1 dB",
       "parameter_shortname": "Gain +1 dB",
       "parameter_type": 2,
       "parameter_enum": [
        "off",
        "on"
       ],
       "parameter_mmax": 1
      }
     },
     "varname": "Gain +1 dB",
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
     "id": "t_p1",
     "maxclass": "newobj",
     "patching_rect": [
      140,
      150,
      30,
      22
     ],
     "text": "t b",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      "bang"
     ]
    }
   },
   {
    "box": {
     "id": "msg_p1",
     "maxclass": "message",
     "patching_rect": [
      140,
      180,
      50,
      22
     ],
     "text": "step 1",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "p3",
     "maxclass": "live.text",
     "patching_rect": [
      200,
      110,
      54,
      24
     ],
     "presentation": 1,
     "presentation_rect": [
      184,
      88,
      54,
      28
     ],
     "text": "+3 dB",
     "texton": "+3 dB",
     "mode": 0,
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      ""
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Gain +3 dB",
       "parameter_shortname": "Gain +3 dB",
       "parameter_type": 2,
       "parameter_enum": [
        "off",
        "on"
       ],
       "parameter_mmax": 1
      }
     },
     "varname": "Gain +3 dB",
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
     "id": "t_p3",
     "maxclass": "newobj",
     "patching_rect": [
      200,
      150,
      30,
      22
     ],
     "text": "t b",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      "bang"
     ]
    }
   },
   {
    "box": {
     "id": "msg_p3",
     "maxclass": "message",
     "patching_rect": [
      200,
      180,
      50,
      22
     ],
     "text": "step 3",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "unity",
     "maxclass": "live.text",
     "patching_rect": [
      20,
      220,
      100,
      22
     ],
     "presentation": 1,
     "presentation_rect": [
      10,
      122,
      112,
      26
     ],
     "text": "0 dB",
     "texton": "0 dB",
     "mode": 0,
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      ""
     ],
     "parameter_enable": 1,
     "saved_attribute_attributes": {
      "valueof": {
       "parameter_longname": "Gain 0 dB",
       "parameter_shortname": "Gain 0 dB",
       "parameter_type": 2,
       "parameter_enum": [
        "off",
        "on"
       ],
       "parameter_mmax": 1
      }
     },
     "varname": "Gain 0 dB",
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
     "id": "t_unity",
     "maxclass": "newobj",
     "patching_rect": [
      130,
      220,
      30,
      22
     ],
     "text": "t b",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      "bang"
     ]
    }
   },
   {
    "box": {
     "id": "msg_unity",
     "maxclass": "message",
     "patching_rect": [
      170,
      220,
      44,
      22
     ],
     "text": "unity",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "hint",
     "maxclass": "comment",
     "patching_rect": [
      20,
      250,
      220,
      18
     ],
     "presentation": 1,
     "presentation_rect": [
      126,
      124,
      120,
      26
     ],
     "text": "map the buttons to keys: Cmd/Ctrl+K",
     "fontname": "Arial",
     "fontsize": 8.5,
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
     "id": "js",
     "maxclass": "newobj",
     "patching_rect": [
      20,
      290,
      160,
      22
     ],
     "text": "js lt_clipgain.js",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      ""
     ]
    }
   },
   {
    "box": {
     "id": "thisdev",
     "maxclass": "newobj",
     "patching_rect": [
      300,
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
     "id": "msgInit",
     "maxclass": "message",
     "patching_rect": [
      300,
      50,
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
     "id": "plugin",
     "maxclass": "newobj",
     "patching_rect": [
      600,
      20,
      60,
      22
     ],
     "text": "plugin~",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "signal",
      "signal"
     ]
    }
   },
   {
    "box": {
     "id": "plugout",
     "maxclass": "newobj",
     "patching_rect": [
      600,
      70,
      60,
      22
     ],
     "text": "plugout~",
     "numinlets": 2,
     "numoutlets": 0,
     "outlettype": []
    }
   }
  ],
  "lines": [
   {
    "patchline": {
     "source": [
      "m3",
      0
     ],
     "destination": [
      "t_m3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "t_m3",
      0
     ],
     "destination": [
      "msg_m3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "msg_m3",
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
      "m1",
      0
     ],
     "destination": [
      "t_m1",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "t_m1",
      0
     ],
     "destination": [
      "msg_m1",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "msg_m1",
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
      "p1",
      0
     ],
     "destination": [
      "t_p1",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "t_p1",
      0
     ],
     "destination": [
      "msg_p1",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "msg_p1",
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
      "p3",
      0
     ],
     "destination": [
      "t_p3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "t_p3",
      0
     ],
     "destination": [
      "msg_p3",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "msg_p3",
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
      "unity",
      0
     ],
     "destination": [
      "t_unity",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "t_unity",
      0
     ],
     "destination": [
      "msg_unity",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "msg_unity",
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
      "js",
      0
     ],
     "destination": [
      "clipName",
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
      "gain",
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
      "plugin",
      0
     ],
     "destination": [
      "plugout",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "plugin",
      1
     ],
     "destination": [
      "plugout",
      1
     ]
    }
   }
  ],
  "dependency_cache": [
   {
    "name": "lt_clipgain.js",
    "type": "TEXT",
    "implicit": 1
   }
  ],
  "autosave": 0
 }
}
