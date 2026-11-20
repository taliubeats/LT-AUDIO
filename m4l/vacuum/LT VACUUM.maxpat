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
   640.0,
   480.0
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
  "devicewidth": 300.0,
  "description": "LT VACUUM: finds and deletes tracks whose signal never reaches Main",
  "boxes": [
   {
    "box": {
     "id": "title",
     "maxclass": "comment",
     "patching_rect": [
      20,
      20,
      120,
      20
     ],
     "presentation": 1,
     "presentation_rect": [
      10,
      8,
      140,
      20
     ],
     "text": "LT VACUUM",
     "fontname": "Arial Bold",
     "fontsize": 13.0,
     "textcolor": [
      0.92,
      0.9,
      0.96,
      1.0
     ]
    }
   },
   {
    "box": {
     "id": "sub",
     "maxclass": "comment",
     "patching_rect": [
      20,
      40,
      200,
      18
     ],
     "presentation": 1,
     "presentation_rect": [
      10,
      26,
      200,
      16
     ],
     "text": "removes tracks that never reach Main",
     "fontname": "Arial",
     "fontsize": 9.0,
     "textcolor": [
      0.62,
      0.58,
      0.72,
      1.0
     ]
    }
   },
   {
    "box": {
     "id": "scan",
     "maxclass": "live.text",
     "patching_rect": [
      20,
      70,
      80,
      24
     ],
     "presentation": 1,
     "presentation_rect": [
      10,
      50,
      86,
      26
     ],
     "text": "SCAN",
     "texton": "SCAN",
     "mode": 0,
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      ""
     ],
     "parameter_enable": 0,
     "fontname": "Arial Bold",
     "fontsize": 11.0,
     "bgcolor": [
      0.09,
      0.08,
      0.14,
      1.0
     ],
     "bgoncolor": [
      0.48,
      0.17,
      1.0,
      1.0
     ],
     "activebgcolor": [
      0.09,
      0.08,
      0.14,
      1.0
     ],
     "activebgoncolor": [
      0.48,
      0.17,
      1.0,
      1.0
     ],
     "textcolor": [
      0.92,
      0.9,
      0.96,
      1.0
     ],
     "textoncolor": [
      1,
      1,
      1,
      1
     ],
     "activetextcolor": [
      0.92,
      0.9,
      0.96,
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
     ],
     "rounded": 6.0
    }
   },
   {
    "box": {
     "id": "del",
     "maxclass": "live.text",
     "patching_rect": [
      120,
      70,
      80,
      24
     ],
     "presentation": 1,
     "presentation_rect": [
      102,
      50,
      86,
      26
     ],
     "text": "DELETE",
     "texton": "DELETE",
     "mode": 0,
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
      "",
      ""
     ],
     "parameter_enable": 0,
     "fontname": "Arial Bold",
     "fontsize": 11.0,
     "bgcolor": [
      0.09,
      0.08,
      0.14,
      1.0
     ],
     "bgoncolor": [
      0.97,
      0.44,
      0.44,
      1.0
     ],
     "activebgcolor": [
      0.09,
      0.08,
      0.14,
      1.0
     ],
     "activebgoncolor": [
      0.97,
      0.44,
      0.44,
      1.0
     ],
     "textcolor": [
      0.92,
      0.9,
      0.96,
      1.0
     ],
     "textoncolor": [
      1,
      1,
      1,
      1
     ],
     "activetextcolor": [
      0.92,
      0.9,
      0.96,
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
     ],
     "rounded": 6.0
    }
   },
   {
    "box": {
     "id": "emptyToggle",
     "maxclass": "live.toggle",
     "patching_rect": [
      220,
      70,
      20,
      20
     ],
     "presentation": 1,
     "presentation_rect": [
      196,
      54,
      18,
      18
     ],
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ],
     "parameter_enable": 0,
     "bgcolor": [
      0.09,
      0.08,
      0.14,
      1.0
     ],
     "activebgoncolor": [
      0.48,
      0.17,
      1.0,
      1.0
     ],
     "bordercolor": [
      0.62,
      0.58,
      0.72,
      1.0
     ]
    }
   },
   {
    "box": {
     "id": "emptyLabel",
     "maxclass": "comment",
     "patching_rect": [
      244,
      70,
      60,
      18
     ],
     "presentation": 1,
     "presentation_rect": [
      216,
      54,
      80,
      18
     ],
     "text": "+ EMPTY",
     "fontname": "Arial Bold",
     "fontsize": 9.5,
     "textcolor": [
      0.62,
      0.58,
      0.72,
      1.0
     ]
    }
   },
   {
    "box": {
     "id": "list",
     "maxclass": "textedit",
     "patching_rect": [
      20,
      110,
      280,
      80
     ],
     "presentation": 1,
     "presentation_rect": [
      10,
      82,
      280,
      80
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
      0.92,
      0.9,
      0.96,
      1.0
     ],
     "bgcolor": [
      0.09,
      0.08,
      0.14,
      1.0
     ],
     "bordercolor": [
      0.09,
      0.08,
      0.14,
      1.0
     ],
     "rounded": 6.0,
     "wordwrap": 1,
     "text": "SCAN finds tracks that never reach Main."
    }
   },
   {
    "box": {
     "id": "bg",
     "maxclass": "panel",
     "patching_rect": [
      0,
      0,
      300,
      169
     ],
     "presentation": 1,
     "presentation_rect": [
      0,
      0,
      300,
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
     "id": "thisdev",
     "maxclass": "newobj",
     "patching_rect": [
      320,
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
      320,
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
     "id": "tScan",
     "maxclass": "newobj",
     "patching_rect": [
      20,
      210,
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
     "id": "tDel",
     "maxclass": "newobj",
     "patching_rect": [
      120,
      210,
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
     "id": "msgScan",
     "maxclass": "message",
     "patching_rect": [
      20,
      240,
      40,
      22
     ],
     "text": "scan",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "msgDel",
     "maxclass": "message",
     "patching_rect": [
      120,
      240,
      44,
      22
     ],
     "text": "clean",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "prepEmpty",
     "maxclass": "newobj",
     "patching_rect": [
      220,
      210,
      90,
      22
     ],
     "text": "prepend empty",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "js",
     "maxclass": "newobj",
     "patching_rect": [
      20,
      290,
      200,
      22
     ],
     "text": "js lt_vacuum.js",
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
     "id": "prepActive",
     "maxclass": "newobj",
     "patching_rect": [
      120,
      330,
      100,
      22
     ],
     "text": "prepend active",
     "numinlets": 1,
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
      440,
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
      440,
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
      "scan",
      0
     ],
     "destination": [
      "tScan",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "tScan",
      0
     ],
     "destination": [
      "msgScan",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "msgScan",
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
      "del",
      0
     ],
     "destination": [
      "tDel",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "tDel",
      0
     ],
     "destination": [
      "msgDel",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "msgDel",
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
      "emptyToggle",
      0
     ],
     "destination": [
      "prepEmpty",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "prepEmpty",
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
      "list",
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
      "prepActive",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "prepActive",
      0
     ],
     "destination": [
      "del",
      0
     ]
    }
   }
  ],
  "dependency_cache": [
   {
    "name": "lt_vacuum.js",
    "type": "TEXT",
    "implicit": 1
   }
  ],
  "autosave": 0
 }
}
