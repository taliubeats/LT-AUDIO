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
  "devicewidth": 260.0,
  "description": "LT BOUNCE: record the selected track over the loop into a new audio track, in place",
  "boxes": [
   {
    "box": {
     "id": "bg",
     "maxclass": "panel",
     "patching_rect": [
      0,
      0,
      260,
      169
     ],
     "presentation": 1,
     "presentation_rect": [
      0,
      0,
      260,
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
     "text": "LT BOUNCE",
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
     "id": "sub",
     "maxclass": "comment",
     "patching_rect": [
      20,
      40,
      220,
      16
     ],
     "presentation": 1,
     "presentation_rect": [
      10,
      22,
      240,
      16
     ],
     "text": "bounce the selected track in place",
     "fontname": "Arial",
     "fontsize": 9.0,
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
     "id": "bounce",
     "maxclass": "live.text",
     "patching_rect": [
      20,
      70,
      110,
      28
     ],
     "presentation": 1,
     "presentation_rect": [
      10,
      42,
      118,
      30
     ],
     "text": "BOUNCE",
     "texton": "BOUNCE",
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
       "parameter_longname": "Bounce",
       "parameter_shortname": "Bounce",
       "parameter_type": 2,
       "parameter_enum": [
        "off",
        "on"
       ],
       "parameter_mmax": 1
      }
     },
     "varname": "Bounce",
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
     "id": "cancel",
     "maxclass": "live.text",
     "patching_rect": [
      140,
      70,
      80,
      28
     ],
     "presentation": 1,
     "presentation_rect": [
      134,
      42,
      116,
      30
     ],
     "text": "CANCEL",
     "texton": "CANCEL",
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
       "parameter_longname": "Cancel",
       "parameter_shortname": "Cancel",
       "parameter_type": 2,
       "parameter_enum": [
        "off",
        "on"
       ],
       "parameter_mmax": 1
      }
     },
     "varname": "Cancel",
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
     "id": "afterLabel",
     "maxclass": "comment",
     "patching_rect": [
      20,
      104,
      120,
      16
     ],
     "presentation": 1,
     "presentation_rect": [
      10,
      78,
      120,
      16
     ],
     "text": "THEN THE ORIGINAL:",
     "fontname": "Arial Bold",
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
     "id": "after",
     "maxclass": "live.tab",
     "patching_rect": [
      140,
      104,
      110,
      18
     ],
     "presentation": 1,
     "presentation_rect": [
      10,
      94,
      240,
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
       "parameter_longname": "Original",
       "parameter_shortname": "Original",
       "parameter_type": 2,
       "parameter_enum": [
        "MUTE",
        "KEEP",
        "DELETE"
       ],
       "parameter_mmax": 2,
       "parameter_initial_enable": 1,
       "parameter_initial": [
        0
       ]
      }
     },
     "varname": "Original",
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
     "id": "status",
     "maxclass": "textedit",
     "patching_rect": [
      20,
      130,
      230,
      44
     ],
     "presentation": 1,
     "presentation_rect": [
      10,
      118,
      240,
      44
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
     "text": "Select a track, set the loop, press BOUNCE."
    }
   },
   {
    "box": {
     "id": "js",
     "maxclass": "newobj",
     "patching_rect": [
      20,
      220,
      160,
      22
     ],
     "text": "js lt_bounce.js",
     "numinlets": 1,
     "numoutlets": 1,
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "t_bounce",
     "maxclass": "newobj",
     "patching_rect": [
      300,
      70,
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
     "id": "msg_bounce",
     "maxclass": "message",
     "patching_rect": [
      340,
      70,
      54,
      22
     ],
     "text": "bounce",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "t_cancel",
     "maxclass": "newobj",
     "patching_rect": [
      300,
      100,
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
     "id": "msg_cancel",
     "maxclass": "message",
     "patching_rect": [
      340,
      100,
      54,
      22
     ],
     "text": "cancel",
     "numinlets": 2,
     "numoutlets": 1,
     "outlettype": [
      ""
     ]
    }
   },
   {
    "box": {
     "id": "pre_after",
     "maxclass": "newobj",
     "patching_rect": [
      300,
      140,
      100,
      22
     ],
     "text": "prepend setAfter",
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
      420,
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
      420,
      50,
      50,
      22
     ],
     "text": "t b b",
     "numinlets": 1,
     "numoutlets": 2,
     "outlettype": [
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
      480,
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
      "js",
      0
     ],
     "destination": [
      "status",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "bounce",
      0
     ],
     "destination": [
      "t_bounce",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "t_bounce",
      0
     ],
     "destination": [
      "msg_bounce",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "msg_bounce",
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
      "cancel",
      0
     ],
     "destination": [
      "t_cancel",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "t_cancel",
      0
     ],
     "destination": [
      "msg_cancel",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "msg_cancel",
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
      "after",
      0
     ],
     "destination": [
      "pre_after",
      0
     ]
    }
   },
   {
    "patchline": {
     "source": [
      "pre_after",
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
      1
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
      "after",
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
    "name": "lt_bounce.js",
    "type": "TEXT",
    "implicit": 1
   }
  ],
  "autosave": 0
 }
}
