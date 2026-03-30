#!/usr/bin/env python3
"""
SBC Studio - A lightweight code editor for .bas Tiny Basic files
with serial upload support for AVR-SBC devices.

Requirements: pip install pyserial
Optional:     pip install pretty_midi   (for MIDI to BAS converter)
              pip install Pillow        (for Bitmap Editor & Loader)
"""

import os
from os.path import exists
import sys
import re
import json
import struct
import tkinter as tk
from tkinter import (
    ttk, filedialog, messagebox, simpledialog, font as tkfont,
)
import threading
import datetime

# ---------------------------------------------------------------------------
# Optional dependency checks (fail gracefully)
# ---------------------------------------------------------------------------
try:
    import serial
    import serial.tools.list_ports
    SERIAL_AVAILABLE = True
except ImportError:
    SERIAL_AVAILABLE = False

try:
    from PIL import Image, ImageTk, ImageOps, ImageDraw
    PIL_AVAILABLE = True
except ImportError:
    PIL_AVAILABLE = False

try:
    import pretty_midi
    MIDI_AVAILABLE = True
except ImportError:
    MIDI_AVAILABLE = False

try:
    import numpy as np
    NUMPY_AVAILABLE = True
except ImportError:
    NUMPY_AVAILABLE = False

# ===========================================================================
# CONSTANTS
# ===========================================================================
APP_TITLE   = "SBC Studio"
APP_VERSION = "1.1.0"
CONFIG_FILE = os.path.join(os.path.expanduser("~"), ".tinybasic_editor.json")

# Resolve paths relative to this .py file
BASE_DIR = os.path.dirname(os.path.abspath(__file__))

FILETYPES = [
    ("Tiny Basic files", "*.bas"),
    ("Text files", "*.txt"),
    ("All files", "*.*"),
]

TB_KEYWORDS = [
    "PRINT", "INPUT", "LET", "IF", "THEN", "ELSE", "GOTO", "GOSUB",
    "RETURN", "FOR", "TO", "STEP", "NEXT", "END", "REM", "STOP",
    "RUN", "LIST", "NEW", "LOAD", "SAVE", "CLR", "PEEK", "POKE",
    "ABS", "RND", "USR", "SIZE", "AND", "OR", "NOT",
    "SERLOAD", "SEROPEN", "DIM", "DATA", "READ", "RESTORE",
]

_KW_PATTERN  = re.compile(r'\b(?:' + '|'.join(TB_KEYWORDS) + r')\b',
                           re.IGNORECASE)
_NUM_PATTERN = re.compile(r'\b\d+\.?\d*\b')
_STR_PATTERN = re.compile(r'"[^"]*"?')
_OP_PATTERN  = re.compile(r'[+\-*/=<>!&|^~%]+')
_REM_PATTERN = re.compile(r'\bREM\b', re.IGNORECASE)

THEME = {
    "bg":               "#FFFFFF",
    "fg":               "#000000",
    "select_bg":        "#308CC6",
    "select_fg":        "#FFFFFF",
    "caret":            "#000000",
    "line_highlight":   "#E8E8FF",
    "gutter_bg":        "#F0F0F0",
    "gutter_fg":        "#8B8B8B",
    "gutter_border":    "#D0D0D0",
    "keyword":          "#0000FF",
    "number":           "#FF8000",
    "string":           "#808080",
    "comment":          "#008000",
    "operator":         "#800080",
    "menu_bg":          "#F5F5F5",
    "menu_fg":          "#000000",
    "toolbar_bg":       "#E8E8E8",
    "statusbar_bg":     "#E0E0E0",
    "statusbar_fg":     "#333333",
    "tab_bg":           "#ECECEC",
    "tab_active_bg":    "#FFFFFF",
    "find_bg":          "#E8E8E8",
    "find_match":       "#FFFF00",
    "find_current":     "#FF8C00",
}

TOOL_BG   = "#F5F5F5"
TOOL_FONT = ("Segoe UI", 9)


def list_com_ports():
    if not SERIAL_AVAILABLE:
        return []
    return [p.device for p in serial.tools.list_ports.comports()]


def load_config():
    defaults = {
        "recent_files": [], "last_com_port": "None", "word_wrap": False,
        "font_size": 11, "font_family": "Consolas",
        "window_geometry": "1100x720", "show_line_numbers": True,
        "show_statusbar": True, "show_toolbar": True,
    }
    try:
        with open(CONFIG_FILE, "r") as f:
            saved = json.load(f)
        defaults.update(saved)
    except Exception:
        pass
    return defaults

def save_config(cfg):
    try:
        with open(CONFIG_FILE, "w") as f:
            json.dump(cfg, f, indent=2)
    except Exception:
        pass


# ===========================================================================
# LINE-NUMBER GUTTER
# ===========================================================================
class LineNumbers(tk.Canvas):
    def __init__(self, master, text_widget, **kw):
        super().__init__(master, width=50, highlightthickness=0,
                         bg=THEME["gutter_bg"], bd=0, **kw)
        self.text_widget = text_widget
        self._font = text_widget["font"]

    def redraw(self, *_a):
        self.delete("all")
        try:
            i = self.text_widget.index("@0,0")
        except tk.TclError:
            return
        while True:
            dline = self.text_widget.dlineinfo(i)
            if dline is None:
                break
            y = dline[1]
            ln = str(i).split(".")[0]
            self.create_text(self.winfo_width() - 8, y, anchor="ne",
                             text=ln, font=self._font,
                             fill=THEME["gutter_fg"])
            i = self.text_widget.index(f"{i}+1line")
        self.create_line(self.winfo_width()-1, 0,
                         self.winfo_width()-1, self.winfo_height(),
                         fill=THEME["gutter_border"])

    def update_font(self, f):
        self._font = f
        self.redraw()


# ===========================================================================
# TEXT WIDGET WITH CHANGE PROXY
# ===========================================================================
class TrackedText(tk.Text):
    def __init__(self, master=None, **kw):
        super().__init__(master, **kw)
        private = str(self) + "_orig"
        self.tk.call("rename", str(self), private)
        self._orig = private
        self.tk.createcommand(str(self), self._proxy)

    def _proxy(self, *args):
        try:
            result = self.tk.call(self._orig, *args)
        except tk.TclError:
            return ""
        if args:
            op = args[0]
            if op in ("insert", "delete", "replace"):
                self.event_generate("<<ContentChanged>>")
            elif op == "edit" and len(args) >= 2 and args[1] in ("undo", "redo"):
                self.event_generate("<<ContentChanged>>")
        return result


# ===========================================================================
# EDITOR TAB
# ===========================================================================
class EditorTab(ttk.Frame):
    def __init__(self, master, app, filepath=None, content=""):
        super().__init__(master)
        self.app = app
        self.filepath = filepath
        self.modified = False
        self._hl_pending = False
        self._build_ui()
        if content:
            self.text.insert("1.0", content)
        self.text.edit_reset()
        self.text.edit_modified(False)
        self._schedule_full_highlight()

    def _build_ui(self):
        self.container = tk.Frame(self, bg=THEME["bg"])
        self.container.pack(fill="both", expand=True)
        self.text = TrackedText(
            self.container, undo=True, maxundo=-1, autoseparators=True,
            wrap="none", font=self.app.editor_font,
            bg=THEME["bg"], fg=THEME["fg"],
            insertbackground=THEME["caret"],
            selectbackground=THEME["select_bg"],
            selectforeground=THEME["select_fg"],
            relief="flat", borderwidth=0, padx=6, pady=4, tabs=("4c",))
        self.linenumbers = LineNumbers(self.container, self.text)
        self.vscroll = ttk.Scrollbar(self.container, orient="vertical",
                                      command=self.text.yview)
        self.hscroll = ttk.Scrollbar(self.container, orient="horizontal",
                                      command=self.text.xview)
        self.text.configure(yscrollcommand=self._on_vs,
                            xscrollcommand=self.hscroll.set)
        self.linenumbers.pack(side="left", fill="y")
        self.vscroll.pack(side="right", fill="y")
        self.hscroll.pack(side="bottom", fill="x")
        self.text.pack(side="left", fill="both", expand=True)
        self.text.bind("<<Modified>>", self._on_mod)
        self.text.bind("<<ContentChanged>>", self._on_cc)
        self.text.bind("<ButtonRelease-1>", self._on_cur)
        self.text.bind("<KeyRelease>", self._on_cur)
        self.text.bind("<MouseWheel>", lambda e: self.linenumbers.redraw())
        self.text.bind("<Configure>", lambda e: self.linenumbers.redraw())
        for t, kw in [
            ("keyword",  dict(foreground=THEME["keyword"], font=self.app.editor_font_bold)),
            ("number",   dict(foreground=THEME["number"])),
            ("string",   dict(foreground=THEME["string"])),
            ("comment",  dict(foreground=THEME["comment"], font=self.app.editor_font_italic)),
            ("operator", dict(foreground=THEME["operator"])),
            ("current_line", dict(background=THEME["line_highlight"])),
            ("find_match",   dict(background=THEME["find_match"], foreground="#000")),
            ("find_current", dict(background=THEME["find_current"], foreground="#FFF")),
        ]:
            self.text.tag_configure(t, **kw)
        self.text.tag_lower("current_line")
        self.text.tag_raise("find_current", "find_match")

    def _on_vs(self, *a):
        self.vscroll.set(*a); self.linenumbers.redraw()
    def _on_mod(self, _e=None):
        if self.text.edit_modified():
            self.modified = True; self.app._update_tab_title(self)
        self.text.edit_modified(False)
    def _on_cc(self, _e=None):
        self._schedule_full_highlight(); self.linenumbers.redraw()
        self._highlight_current_line(); self.app._update_statusbar()
    def _on_cur(self, _e=None):
        self._highlight_current_line(); self.app._update_statusbar()

    def _schedule_full_highlight(self):
        if not self._hl_pending:
            self._hl_pending = True; self.after(1, self._do_hl)
    def _do_hl(self):
        self._hl_pending = False; self._hl_all()
    def _hl_all(self):
        t = self.text
        for tag in ("keyword","number","string","comment","operator"):
            t.tag_remove(tag, "1.0", "end")
        content = t.get("1.0", "end-1c")
        for i, line in enumerate(content.split("\n")):
            ln = i + 1
            if not line.strip(): continue
            rm = _REM_PATTERN.search(line)
            if rm:
                sc = rm.start()
                t.tag_add("comment", f"{ln}.{sc}", f"{ln}.end")
                if line[:sc].strip(): self._hl_tok(ln, line[:sc], 0)
                continue
            self._hl_tok(ln, line, 0)
    def _hl_tok(self, ln, line, off):
        t = self.text
        for pat, tag in [(_KW_PATTERN,"keyword"),(_NUM_PATTERN,"number"),
                         (_STR_PATTERN,"string"),(_OP_PATTERN,"operator")]:
            for m in pat.finditer(line):
                t.tag_add(tag, f"{ln}.{m.start()+off}", f"{ln}.{m.end()+off}")
    def _highlight_current_line(self):
        self.text.tag_remove("current_line", "1.0", "end")
        try:
            ln = self.text.index("insert").split(".")[0]
        except tk.TclError: return
        self.text.tag_add("current_line", f"{ln}.0", f"{ln}.end+1c")

    def get_content(self): return self.text.get("1.0", "end-1c")
    def get_selection(self):
        try: return self.text.get("sel.first", "sel.last") or ""
        except tk.TclError: return ""
    def set_wrap(self, on):
        self.text.configure(wrap="word" if on else "none")
        if on: self.hscroll.pack_forget()
        else: self.hscroll.pack(side="bottom", fill="x", before=self.text)
    def update_font(self):
        self.text.configure(font=self.app.editor_font)
        self.text.tag_configure("keyword", font=self.app.editor_font_bold)
        self.text.tag_configure("comment", font=self.app.editor_font_italic)
        self.linenumbers.update_font(self.app.editor_font)
    def clear_find_marks(self):
        self.text.tag_remove("find_match", "1.0", "end")
        self.text.tag_remove("find_current", "1.0", "end")


# ===========================================================================
# FIND / REPLACE BAR
# ===========================================================================
class FindReplaceBar(tk.Frame):
    def __init__(self, master, app):
        super().__init__(master, bg=THEME["find_bg"], bd=0, relief="flat")
        self.app = app; self.visible = False
        self._matches = []; self._cur = -1; self._build()

    def _build(self):
        r0 = tk.Frame(self, bg=THEME["find_bg"]); r0.pack(fill="x", padx=6, pady=(3,1))
        tk.Label(r0, text="Find:", bg=THEME["find_bg"], font=("Segoe UI",8)).pack(side="left")
        self.fvar = tk.StringVar()
        self.fentry = tk.Entry(r0, textvariable=self.fvar, width=30,
                               font=("Segoe UI",8), relief="solid", bd=1)
        self.fentry.pack(side="left", padx=(4,4))
        self.fentry.bind("<Return>", lambda e: self.find_next())
        bk = dict(font=("Segoe UI",8), relief="flat", bg=THEME["find_bg"],
                  activebackground="#D0D0D0", bd=0, padx=6, pady=1, cursor="hand2")
        tk.Button(r0, text="Next", command=self.find_next, **bk).pack(side="left", padx=1)
        tk.Button(r0, text="Prev", command=self.find_prev, **bk).pack(side="left", padx=1)
        self.mc = tk.BooleanVar(value=False)
        tk.Checkbutton(r0, text="Match case", variable=self.mc,
                       bg=THEME["find_bg"], fg=THEME["fg"],
                       activebackground=THEME["find_bg"],
                       selectcolor=THEME["find_bg"],
                       font=("Segoe UI",8)).pack(side="left", padx=(8,4))
        self.clbl = tk.Label(r0, text="", bg=THEME["find_bg"], fg="#666",
                             font=("Segoe UI",8))
        self.clbl.pack(side="left", padx=4)
        tk.Button(r0, text="\u00D7", command=self.hide, font=("Segoe UI",9,"bold"),
                  relief="flat", bg=THEME["find_bg"], activebackground="#D0D0D0",
                  bd=0, padx=6, cursor="hand2").pack(side="right")
        r1 = tk.Frame(self, bg=THEME["find_bg"]); r1.pack(fill="x", padx=6, pady=(1,3))
        tk.Label(r1, text="Replace:", bg=THEME["find_bg"], font=("Segoe UI",8)).pack(side="left")
        self.rvar = tk.StringVar()
        self.rentry = tk.Entry(r1, textvariable=self.rvar, width=30,
                               font=("Segoe UI",8), relief="solid", bd=1)
        self.rentry.pack(side="left", padx=(4,4))
        tk.Button(r1, text="Replace", command=self.replace_one, **bk).pack(side="left", padx=1)
        tk.Button(r1, text="Replace All", command=self.replace_all, **bk).pack(side="left", padx=1)
        tk.Frame(self, height=1, bg=THEME["gutter_border"]).pack(fill="x", side="bottom")

    def show(self, replace=False):
        if not self.visible:
            self.pack(side="top", fill="x", before=self.app.notebook); self.visible = True
        self.fentry.focus_set()
        tab = self.app._current_tab()
        if tab:
            s = tab.get_selection()
            if s: self.fvar.set(s)
        self.fentry.select_range(0, "end")

    def hide(self):
        if self.visible:
            tab = self.app._current_tab()
            if tab: tab.clear_find_marks()
            self.pack_forget(); self.visible = False; self.clbl.config(text="")
            self._matches = []; self._cur = -1

    def _nocase(self): return not self.mc.get()

    def _build_matches(self, pat, tab):
        tab.clear_find_marks(); self._matches = []; self._cur = -1
        content = tab.get_content()
        fl = re.IGNORECASE if self._nocase() else 0
        for m in re.finditer(re.escape(pat), content, fl):
            s = f"1.0+{m.start()}c"; e = f"1.0+{m.end()}c"
            tab.text.tag_add("find_match", s, e)
            self._matches.append((s, e))
        return len(self._matches)

    def _hl_cur(self, tab):
        tab.text.tag_remove("find_current", "1.0", "end")
        if 0 <= self._cur < len(self._matches):
            s, e = self._matches[self._cur]
            tab.text.tag_add("find_current", s, e)

    def _update_lbl(self):
        n = len(self._matches)
        if n == 0: self.clbl.config(text="No matches"); return
        self.clbl.config(text=f"{self._cur+1} of {n}")

    def _goto(self, tab):
        if 0 <= self._cur < len(self._matches):
            s, e = self._matches[self._cur]
            tab.text.mark_set("insert", e); tab.text.see(s)
            self._hl_cur(tab); tab._highlight_current_line()
            self.app._update_statusbar()
        self._update_lbl()

    def find_next(self):
        tab = self.app._current_tab()
        if not tab: return
        pat = self.fvar.get()
        if not pat: return
        n = self._build_matches(pat, tab)
        if n == 0: self._update_lbl(); return
        ip = tab.text.index("insert")
        found = False
        for i, (s, e) in enumerate(self._matches):
            if tab.text.compare(s, ">=", ip):
                self._cur = i; found = True; break
        if not found: self._cur = 0
        self._goto(tab)

    def find_prev(self):
        tab=self.app._current_tab()
        if not tab: return
        pat=self.fvar.get()
        if not pat: return
        # Capture the start of the current match BEFORE _build_matches resets
        # self._matches. _goto places "insert" at the *end* of the match, so
        # using "insert" as the reference makes s < insert true for the current
        # match itself — causing Prev to always re-select the same result.
        prev_start = self._matches[self._cur][0] if 0 <= self._cur < len(self._matches) else None
        n=self._build_matches(pat,tab)
        if n==0: self._update_lbl(); return
        if prev_start is None:
            prev_start = tab.text.index("insert")
        found=False
        for i in range(len(self._matches)-1,-1,-1):
            s,e=self._matches[i]
            if tab.text.compare(s,"<",prev_start): self._cur=i; found=True; break
        if not found: self._cur=len(self._matches)-1
        self._goto(tab)

    def replace_one(self):
        tab = self.app._current_tab()
        if not tab: return
        if self._cur < 0 or self._cur >= len(self._matches):
            self.find_next(); return
        s, e = self._matches[self._cur]
        tab.text.delete(s, e); tab.text.insert(s, self.rvar.get())
        self.find_next()

    def replace_all(self):
        tab = self.app._current_tab()
        if not tab: return
        pat = self.fvar.get(); repl = self.rvar.get()
        if not pat: return
        content = tab.get_content()
        fl = re.IGNORECASE if self._nocase() else 0
        new, n = re.subn(re.escape(pat), repl, content, flags=fl)
        if n: tab.text.delete("1.0", "end"); tab.text.insert("1.0", new)
        self.clbl.config(text=f"Replaced {n} occurrence{'s' if n!=1 else ''}")
        self._matches = []; self._cur = -1


# ===========================================================================
# TOOL WINDOWS
# ===========================================================================

# ---------------------------------------------------------------------------
# Bitmap Editor
# ---------------------------------------------------------------------------
class BitmapEditorWindow(tk.Toplevel):
    """128x64 binary BMP editor for AVR-SBC."""

    def __init__(self, parent):
        super().__init__(parent)
        self.title("AVR-SBC Bitmap Editor")
        p = os.path.join(BASE_DIR, "bitmap-editor-icon.ico")
        if os.path.exists(p):
            self.iconbitmap(p)
        self.geometry("460x700")
        self.resizable(False, False)
        self.configure(bg=TOOL_BG)
        self.image = None
        self.modified_image = None

        self.invert_var = tk.BooleanVar()
        self.dither_var = tk.StringVar(value="None")
        self.threshold_var = tk.DoubleVar(value=128)
        self.scale_var = tk.StringVar(value="Stretch to fill canvas")
        self.center_h_var = tk.BooleanVar()
        self.center_v_var = tk.BooleanVar()
        self.rotate_var = tk.IntVar(value=0)
        self.flip_h_var = tk.BooleanVar()
        self.flip_v_var = tk.BooleanVar()
        self.bg_color_var = tk.StringVar(value="Black")

        self._build()
        self._show_placeholder()

    def _build(self):
        tk.Label(self, text="AVR-SBC Bitmap Editor", font=("Segoe UI", 12, "bold"),
                 bg=TOOL_BG).pack(pady=(10, 2))
        tk.Label(self, text="Generate 128\u00d764 binary BMP images for the AVR-SBC.\n"
                 "Use the Bitmap Loader to upload them to the board.",
                 font=TOOL_FONT, bg=TOOL_BG, justify="center").pack(pady=(0, 8))

        bf = tk.Frame(self, bg=TOOL_BG); bf.pack(pady=4)
        self.path_var = tk.StringVar()
        tk.Entry(bf, textvariable=self.path_var, width=36, state="readonly",
                 font=TOOL_FONT).pack(side="left", padx=4)
        tk.Button(bf, text="Browse Image", command=self._browse,
                  font=TOOL_FONT).pack(side="left")

        # Previews
        for lbl_text, attr in [("Input", "in_lbl"), ("Output", "out_lbl")]:
            tk.Label(self, text=lbl_text, font=TOOL_FONT, bg=TOOL_BG).pack()
            frm = tk.Frame(self, bd=1, relief="solid", width=256, height=128, bg="black")
            frm.pack(); frm.pack_propagate(False)
            lbl = tk.Label(frm, bg="black"); lbl.pack(expand=True)
            setattr(self, attr, lbl)

        # Options
        of = tk.Frame(self, bg=TOOL_BG); of.pack(pady=6)
        r = 0
        tk.Label(of, text="Invert:", font=TOOL_FONT, bg=TOOL_BG).grid(row=r, column=0, sticky="w")
        tk.Checkbutton(of, text="Invert Colors", variable=self.invert_var,
                       bg=TOOL_BG, font=TOOL_FONT, command=self._update).grid(row=r, column=1, sticky="w")
        r += 1
        tk.Label(of, text="Dithering:", font=TOOL_FONT, bg=TOOL_BG).grid(row=r, column=0, sticky="w")
        dm = tk.OptionMenu(of, self.dither_var, "None", "Floyd-Steinberg", command=lambda _: self._update())
        dm.config(font=TOOL_FONT, width=16); dm.grid(row=r, column=1, sticky="w")
        r += 1
        tk.Label(of, text="Threshold:", font=TOOL_FONT, bg=TOOL_BG).grid(row=r, column=0, sticky="w")
        tk.Scale(of, from_=0, to=255, orient="horizontal", variable=self.threshold_var,
                 bg=TOOL_BG, command=lambda _: self._update()).grid(row=r, column=1, sticky="w")
        r += 1
        tk.Label(of, text="Scaling:", font=TOOL_FONT, bg=TOOL_BG).grid(row=r, column=0, sticky="w")
        sm = tk.OptionMenu(of, self.scale_var, "None", "Scale to fit", "Stretch to fill canvas",
                           command=lambda _: self._update())
        sm.config(font=TOOL_FONT, width=16); sm.grid(row=r, column=1, sticky="w")
        r += 1
        tk.Label(of, text="Center:", font=TOOL_FONT, bg=TOOL_BG).grid(row=r, column=0, sticky="w")
        cf = tk.Frame(of, bg=TOOL_BG); cf.grid(row=r, column=1, sticky="w")
        tk.Checkbutton(cf, text="H", variable=self.center_h_var, bg=TOOL_BG,
                       font=TOOL_FONT, command=self._update).pack(side="left")
        tk.Checkbutton(cf, text="V", variable=self.center_v_var, bg=TOOL_BG,
                       font=TOOL_FONT, command=self._update).pack(side="left")
        r += 1
        tk.Label(of, text="Background:", font=TOOL_FONT, bg=TOOL_BG).grid(row=r, column=0, sticky="w")
        bgf = tk.Frame(of, bg=TOOL_BG); bgf.grid(row=r, column=1, sticky="w")
        tk.Radiobutton(bgf, text="Black", variable=self.bg_color_var, value="Black",
                       bg=TOOL_BG, font=TOOL_FONT, command=self._update).pack(side="left")
        tk.Radiobutton(bgf, text="White", variable=self.bg_color_var, value="White",
                       bg=TOOL_BG, font=TOOL_FONT, command=self._update).pack(side="left")
        r += 1
        tk.Label(of, text="Rotate:", font=TOOL_FONT, bg=TOOL_BG).grid(row=r, column=0, sticky="w")
        rf = tk.Frame(of, bg=TOOL_BG); rf.grid(row=r, column=1, sticky="w")
        for deg in (0, 90, 180, 270):
            tk.Radiobutton(rf, text=str(deg), variable=self.rotate_var, value=deg,
                           bg=TOOL_BG, font=TOOL_FONT, command=self._update).pack(side="left")
        r += 1
        tk.Label(of, text="Flip:", font=TOOL_FONT, bg=TOOL_BG).grid(row=r, column=0, sticky="w")
        ff = tk.Frame(of, bg=TOOL_BG); ff.grid(row=r, column=1, sticky="w")
        tk.Checkbutton(ff, text="H", variable=self.flip_h_var, bg=TOOL_BG,
                       font=TOOL_FONT, command=self._update).pack(side="left")
        tk.Checkbutton(ff, text="V", variable=self.flip_v_var, bg=TOOL_BG,
                       font=TOOL_FONT, command=self._update).pack(side="left")

        tk.Button(self, text="Save Bitmap", font=TOOL_FONT, command=self._save).pack(pady=8)

    def _show_placeholder(self):
        img = Image.new("RGB", (256, 128), "white")
        d = ImageDraw.Draw(img)
        d.text((80, 56), "No Image Loaded", fill="black")
        ph = ImageTk.PhotoImage(img)
        self.in_lbl.config(image=ph); self.in_lbl.image = ph
        self.out_lbl.config(image=ph); self.out_lbl.image = ph

    def _browse(self):
        fp = filedialog.askopenfilename(
            filetypes=[("Image files", "*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tiff"), ("All", "*.*")])
        if fp:
            self.image = Image.open(fp); self.path_var.set(fp)
            self._show_input(); self._update()

    def _show_input(self):
        if self.image:
            ph = ImageTk.PhotoImage(self.image.resize((256, 128), Image.LANCZOS))
            self.in_lbl.config(image=ph); self.in_lbl.image = ph

    def _update(self):
        if not self.image: return
        img = self.image.copy()
        if self.rotate_var.get(): img = img.rotate(self.rotate_var.get(), expand=True)
        if self.flip_h_var.get(): img = img.transpose(Image.FLIP_LEFT_RIGHT)
        if self.flip_v_var.get(): img = img.transpose(Image.FLIP_TOP_BOTTOM)
        w, h = img.size; tw, th = 128, 64
        sv = self.scale_var.get()
        if sv == "Scale to fit":
            r = min(tw/w, th/h); tw, th = int(w*r), int(h*r)
        elif sv == "Stretch to fill canvas":
            tw, th = 128, 64
        else:
            tw, th = w, h
        img = img.resize((tw, th), Image.LANCZOS)
        bgc = "black" if self.bg_color_var.get() == "Black" else "white"
        bg = Image.new("RGB", (128, 64), bgc)
        xo = (128 - tw) // 2 if self.center_h_var.get() else 0
        yo = (64 - th) // 2 if self.center_v_var.get() else 0
        bg.paste(img, (xo, yo)); img = bg
        dt = self.dither_var.get()
        if dt == "Floyd-Steinberg":
            img = img.convert("1", dither=Image.FLOYDSTEINBERG)
        else:
            img = img.convert("L")
            thr = self.threshold_var.get()
            img = img.point(lambda x: 0 if x < thr else 255)
        if self.invert_var.get(): img = ImageOps.invert(img.convert("L"))
        self.modified_image = img
        ph = ImageTk.PhotoImage(img.resize((256, 128), Image.NEAREST))
        self.out_lbl.config(image=ph); self.out_lbl.image = ph

    def _save(self):
        if not self.modified_image: messagebox.showerror("Error","Load an image first.",parent=self); return
        img=self.modified_image
        if img.mode!="L": img=img.convert("L")
        thr=self.threshold_var.get(); bimg=img.point(lambda x:0 if x<thr else 255,"1")
        
        # Create output directory
        base_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
        out_dir = os.path.join(base_dir, "sbc-studio-output", "bitmap-editor")
        os.makedirs(out_dir, exist_ok=True)
        
        init_file = "output.bmp"
        if self.path_var.get():
            init_file = os.path.splitext(os.path.basename(self.path_var.get()))[0] + ".bmp"

        fp=filedialog.asksaveasfilename(initialdir=out_dir, initialfile=init_file, defaultextension=".bmp",filetypes=[("BMP","*.bmp"),("All","*.*")],parent=self)
        if fp: bimg.save(fp); messagebox.showinfo("Saved",f"Bitmap saved to:\n{fp}",parent=self)


# ---------------------------------------------------------------------------
# Bitmap Loader
# ---------------------------------------------------------------------------
class BitmapLoaderWindow(tk.Toplevel):
    """Load 128x64 binary BMP images onto AVR-SBC via serial."""

    BMP_HEADER_FMT = '<2sIHHI'
    BMP_INFO_FMT   = '<IiiHHIIiiII'

    def __init__(self, parent, port_var):
        super().__init__(parent)
        self.title("AVR-SBC Bitmap Loader")
        p = os.path.join(BASE_DIR, "bitmap-loader-icon.ico")
        if os.path.exists(p):
            self.iconbitmap(p)
        self.geometry("560x500")
        self.resizable(False, False)
        self.configure(bg=TOOL_BG)
        self.port_var = port_var
        self._build()

    def _build(self):
        tk.Label(self, text="AVR-SBC Bitmap Loader", font=("Segoe UI", 12, "bold"),
                 bg=TOOL_BG).pack(pady=(10, 2))

        # --- Explanation ---
        explanation = (
            "This tool loads 128\u00d764 binary .bmp images onto an AVR-SBC single board computer "
            "via serial connection.\n\n"
            "How the process works:\n\n"
            "1. Reading the .bmp File: The program reads the header and info header of "
            "the .bmp file to extract essential information about the image, such as its "
            "dimensions and color depth.\n\n"
            "2. Converting to Binary: After verifying that the image is monochrome, the "
            "program calculates padding bytes and extracts the pixel data. Each pixel is "
            "represented as a bit, and these bits are then converted into bytes and written "
            "into a binary file with a .bin extension.\n\n"
            "3. Sending Data to AVR-SBC: The binary file is sent to the AVR-SBC in four "
            "batches. Each batch consists of 256 bytes of image data. The program prompts "
            'you to type \"SERLOAD\" on the AVR-SBC to initiate the data transfer.\n\n'
            "4. Finalizing the Image: Once the four image data batches are sent, in the "
            "fifth, final batch, the program sends drawing commands to the AVR-SBC to "
            "display the loaded image on the screen.\n\n"
            "5. Closing the Connection: Finally, the program closes the serial connection "
            "and exits.\n\n"
            "The entire payload is designed to fit the 5396 byte BASIC program buffer "
            "of the AVR-SBC board."
        )
        tf = tk.Frame(self, bg=TOOL_BG); tf.pack(fill="x", padx=12, pady=4)
        self.exp_text = tk.Text(tf, wrap="word", height=14, font=("Segoe UI", 8),
                                bg="#FFFFFF", fg="#333", relief="solid", bd=1, padx=6, pady=4)
        self.exp_text.pack(fill="x")
        self.exp_text.insert("1.0", explanation)
        self.exp_text.configure(state="disabled")

        # --- Controls ---
        cf = tk.Frame(self, bg=TOOL_BG); cf.pack(pady=8)
        tk.Label(cf, text="COM Port:", font=TOOL_FONT, bg=TOOL_BG).grid(row=0, column=0, padx=4)
        self.port_cb = ttk.Combobox(cf, textvariable=self.port_var,
                                     values=["None"] + list_com_ports(),
                                     state="readonly", width=12, font=TOOL_FONT)
        self.port_cb.grid(row=0, column=1, padx=4)
        self.port_cb.bind("<Button-1>", lambda e: self._refresh_ports())

        tk.Label(cf, text="BMP File:", font=TOOL_FONT, bg=TOOL_BG).grid(row=1, column=0, padx=4, pady=4)
        self.file_var = tk.StringVar()
        tk.Entry(cf, textvariable=self.file_var, width=30, font=TOOL_FONT,
                 state="readonly").grid(row=1, column=1, padx=4, pady=4)
        tk.Button(cf, text="Browse", font=TOOL_FONT,
                  command=self._browse).grid(row=1, column=2, padx=4, pady=4)

        tk.Button(self, text="Upload to AVR-SBC", font=("Segoe UI", 10, "bold"),
                  command=self._upload).pack(pady=8)

        # Log
        tk.Label(self, text="Log:", font=TOOL_FONT, bg=TOOL_BG).pack(anchor="w", padx=12)
        self.log = tk.Text(self, height=8, font=("Consolas", 8), bg="#FFFFFF",
                           fg="#333", relief="solid", bd=1, state="disabled")
        self.log.pack(fill="x", padx=12, pady=(0, 10))

    def _refresh_ports(self):
        self.port_cb["values"] = ["None"] + list_com_ports()

    def _browse(self):
        fp = filedialog.askopenfilename(filetypes=[("BMP files", "*.bmp"), ("All", "*.*")],
                                         parent=self)
        if fp: self.file_var.set(fp)

    def _log(self,msg): 
            if not self.winfo_exists(): return
            try:
                self.log.configure(state="normal")
                self.log.insert("end",msg+"\n")
                self.log.see("end")
                self.log.configure(state="disabled")
                self.update_idletasks()
            except tk.TclError:
                pass
            
    def _read_bmp(self, filename):
        with open(filename, "rb") as f:
            hdr = struct.unpack(self.BMP_HEADER_FMT, f.read(14))
            info = struct.unpack(self.BMP_INFO_FMT, f.read(40))
            if info[4] != 1:
                messagebox.showerror("Error", "Image is not monochrome (1-bit).", parent=self)
                return None
            w, h = info[1], info[2]
            padding = ((w + 7) // 8) % 4
            f.seek(hdr[4])
            raw = bytearray()
            for _ in range(h):
                row_bytes = f.read((w + 7) // 8)
                raw.extend(row_bytes)
                f.seek(padding, 1)
        return bytes(raw)

    def _upload(self):
        port=self.port_var.get()
        if port=="None": messagebox.showerror("Error","Select a COM port.",parent=self); return
        fp=self.file_var.get()
        if not fp or not os.path.exists(fp): messagebox.showerror("Error","Select a valid BMP file.",parent=self); return
        self._log("Reading BMP file..."); data=self._read_bmp(fp)
        if data is None: return
        if len(data)!=1024: messagebox.showerror("Error",f"Expected 1024 bytes, got {len(data)}.",parent=self); return
        self._log(f"Read {len(data)} bytes.")
        
        def _do():
            try:
                import time as _t
                ser = serial.Serial(port, 9600, timeout=5)

                # Helper to safely pause background thread for GUI user confirmation
                def ask(prompt):
                    evt = threading.Event()
                    res = [False]
                    def _show():
                        try:
                            res[0] = messagebox.askokcancel("Action Required", prompt, parent=self)
                        except tk.TclError: pass
                        evt.set()
                    if self.winfo_exists():
                        self.after(0, _show)
                        evt.wait()
                    return res[0]

                # 4 Data Batches
                for bi in range(4):
                    if not ask("Type SERLOAD on the AVR-SBC and press OK to continue..."):
                        ser.close(); self.after(0, lambda: self._log("Upload cancelled.")); return
                    
                    self.after(0, lambda b=bi: self._log(f"Sending batch {b+1}/4 (256 bytes)..."))
                    chunk = data[bi*256:(bi+1)*256]
                    for idx2, bv in enumerate(chunk):
                        addr = bi*256 + idx2
                        ln2 = addr * 10
                        cmd = f"{ln2} XPOKE {bv}, {addr}\r\n"
                        ser.write(cmd.encode())
                    ser.write(b'\x00'); _t.sleep(0.5)

                    if not ask("Type RUN on the AVR-SBC and press OK to continue..."):
                        ser.close(); self.after(0, lambda: self._log("Upload cancelled.")); return

                # Batch 5: Drawing Commands
                if not ask("Type SERLOAD on the AVR-SBC and press OK to send drawing commands..."):
                    ser.close(); self.after(0, lambda: self._log("Upload cancelled.")); return
                
                self.after(0, lambda: self._log("Sending drawing commands (batch 5/5)..."))
                for c in ["5 CLS\r\n","10 X=0\r\n","20 Y=0\r\n","30 FOR I=0 TO 1023\r\n","40 XPEEK N,I\r\n","50 D=128\r\n","60 FOR J=0 TO 7\r\n","70 B=N/D\r\n","80 B=B-(B/2)*2\r\n","90 DRAWPIX X+40,64-Y+80,B\r\n","100 X=X+1\r\n","110 IF X=128 THEN X=0:Y=Y+1\r\n","120 D=D/2\r\n","130 NEXT J\r\n","140 NEXT I\r\n"]:
                    ser.write(c.encode())
                ser.write(b'\x00'); ser.close()
                
                self.after(0, lambda: self._log("Upload complete! Type RUN on the AVR-SBC."))
                if self.winfo_exists():
                    self.after(0, lambda: messagebox.showinfo("Success", "Upload complete!\nType RUN on the AVR-SBC.", parent=self))
            
            except Exception as e:
                self.after(0, lambda err=e: self._log(f"ERROR: {err}"))
                if self.winfo_exists():
                    self.after(0, lambda err=e: messagebox.showerror("Error", str(err), parent=self))
        
        threading.Thread(target=_do, daemon=True).start()


# ---------------------------------------------------------------------------
# MIDI to BAS Converter
# ---------------------------------------------------------------------------
class MidiConverterWindow(tk.Toplevel):
    """Convert single-track MIDI files to .BAS for AVR-SBC buzzer."""

    MAX_BAS_SIZE = 5300

    def __init__(self, parent):
        super().__init__(parent)
        self.title("MIDI to .BAS Converter")
        p = os.path.join(BASE_DIR, "midi-to-bas-converter-icon.ico")
        if os.path.exists(p):
            self.iconbitmap(p)
        self.geometry("500x300")
        self.resizable(False, False)
        self.configure(bg=TOOL_BG)
        self._build()

    def _build(self):
        tk.Label(self, text="MIDI to .BAS Converter", font=("Segoe UI", 12, "bold"),
                 bg=TOOL_BG).pack(pady=(10, 2))
        tk.Label(self, text=(
            "Convert single-track MIDI files to .BAS files that can be loaded\n"
            "onto the AVR-SBC single board computer using SBC Studio.\n"
            "The songs can then be played using the onboard buzzer speaker\n"
            "on the AVR-SBC by typing RUN.\n\n"
            "The output is limited to ~5300 bytes to fit the AVR-SBC program buffer."
        ), font=TOOL_FONT, bg=TOOL_BG, justify="center").pack(pady=(0, 10))

        bf = tk.Frame(self, bg=TOOL_BG); bf.pack(pady=4)
        self.path_var = tk.StringVar()
        tk.Entry(bf, textvariable=self.path_var, width=40, state="readonly",
                 font=TOOL_FONT).pack(side="left", padx=4)
        tk.Button(bf, text="Browse MIDI", command=self._browse,
                  font=TOOL_FONT).pack(side="left")

        tk.Button(self, text="Convert", font=("Segoe UI", 10, "bold"),
                  command=self._convert).pack(pady=10)

        self.status_lbl = tk.Label(self, text="", font=TOOL_FONT, bg=TOOL_BG,
                                   fg="#333", wraplength=460, justify="center")
        self.status_lbl.pack(pady=4)

        self.open_btn = tk.Button(self, text="Open in Editor", font=TOOL_FONT,
                                  command=self._open_in_editor, state="disabled")
        self.open_btn.pack(pady=4)
        self._last_output = None

    def _browse(self):
        fp = filedialog.askopenfilename(
            filetypes=[("MIDI files", "*.mid;*.midi"), ("All", "*.*")], parent=self)
        if fp: self.path_var.set(fp)

    @staticmethod
    def _note_to_freq(note):
        return round(440 * (2 ** ((note - 69) / 12)))

    def _midi_to_bas(self, midi_file):
        midi_data = pretty_midi.PrettyMIDI(midi_file)
        lines = []; ln = 10; prev_end = 0; sz = 0
        for inst in midi_data.instruments:
            notes = sorted(inst.notes, key=lambda n: n.start)
            for i, note in enumerate(notes):
                freq = self._note_to_freq(note.pitch)
                dur = round((note.end - note.start) * 1000)
                nxt = notes[i+1].start if i < len(notes)-1 else 1e6
                dur = min(dur, round((nxt - note.start) * 1000))
                if note.start > prev_end:
                    sil = round((note.start - prev_end) * 1000)
                    line = f"{ln} DELAY {sil}"
                    if sz + len(line) + 2 > self.MAX_BAS_SIZE: return "\n".join(lines)
                    lines.append(line); sz += len(line) + 2; ln += 10
                line = f"{ln} TONEW {freq},{dur}"
                if sz + len(line) + 2 > self.MAX_BAS_SIZE: return "\n".join(lines)
                lines.append(line); sz += len(line) + 2; ln += 10
                prev_end = note.end
            break
        return "\n".join(lines)

    def _convert(self):
        fp=self.path_var.get()
        if not fp: self.status_lbl.config(text="Please select a MIDI file.",fg="red"); return
        try:
            bas=self._midi_to_bas(fp)
            if not bas: self.status_lbl.config(text="Conversion produced no output.",fg="red"); return
            
            # Create output directory
            base_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
            out_dir = os.path.join(base_dir, "sbc-studio-output", "midi-to-bas-converter")
            os.makedirs(out_dir, exist_ok=True)
            
            out_name = os.path.splitext(os.path.basename(fp))[0] + ".bas"
            out = os.path.join(out_dir, out_name)
            
            with open(out,"w") as fh: fh.write(bas)
            self._last_output=out; self.status_lbl.config(text=f"Conversion successful!\nSaved to: {out}",fg="green")
            self.open_btn.config(state="normal")
        except Exception as e: self.status_lbl.config(text=f"Error: {e}",fg="red")

    def _open_in_editor(self):
        if self._last_output and os.path.exists(self._last_output):
            # Find the main app and open the file
            for w in self.master.winfo_children():
                pass
            # Access via master (root) -> app
            try:
                self.master._app_ref.open_file(self._last_output)
            except Exception:
                pass
            self.destroy()


# ===========================================================================
# MAIN APPLICATION
# ===========================================================================
class TinyBasicEditor:
    def __init__(self, root):
        self.root = root
        self.root._app_ref = self  # so tool windows can call back
        self.cfg = load_config()
        self.tabs = []; self._closing_all = False
        self.selected_port = tk.StringVar(value=self.cfg.get("last_com_port", "None"))
        self.word_wrap = tk.BooleanVar(value=self.cfg.get("word_wrap", False))
        self.show_linenums = tk.BooleanVar(value=self.cfg.get("show_line_numbers", True))
        self.show_toolbar_var = tk.BooleanVar(value=self.cfg.get("show_toolbar", True))
        self.show_statusbar_var = tk.BooleanVar(value=self.cfg.get("show_statusbar", True))
        sz = self.cfg.get("font_size", 11); fam = self.cfg.get("font_family", "Consolas")
        self.editor_font       = tkfont.Font(family=fam, size=sz)
        self.editor_font_bold  = tkfont.Font(family=fam, size=sz, weight="bold")
        self.editor_font_italic= tkfont.Font(family=fam, size=sz, slant="italic")
        self._setup_window(); self._build_menu(); self._build_toolbar()
        self._build_findbar(); self._build_notebook(); self._build_statusbar()
        self._bind_shortcuts(); self.new_file()

    def _setup_window(self):
        self.root.title(APP_TITLE)
        p = os.path.join(BASE_DIR, "sbc-studio-icon.ico")
        if os.path.exists(p):
            self.root.iconbitmap(p)
        self.root.geometry(self.cfg.get("window_geometry", "1100x720"))
        self.root.minsize(600, 400)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)
        s = ttk.Style(); s.theme_use("clam")
        s.configure("TNotebook", background=THEME["tab_bg"])
        s.configure("TNotebook.Tab", padding=[10,4], font=("Segoe UI",9))
        s.map("TNotebook.Tab", background=[("selected", THEME["tab_active_bg"])],
              foreground=[("selected","#000")])

    # --- MENU ---
    def _build_menu(self):
        mb = tk.Menu(self.root, bg=THEME["menu_bg"], fg=THEME["menu_fg"],
                     activebackground=THEME["select_bg"],
                     activeforeground=THEME["select_fg"], relief="flat", bd=0)
        # File
        fm = tk.Menu(mb, tearoff=0)
        fm.add_command(label="New", accelerator="Ctrl+N", command=self.new_file)
        fm.add_command(label="Open...", accelerator="Ctrl+O", command=self.open_file)
        fm.add_separator()
        fm.add_command(label="Save", accelerator="Ctrl+S", command=self.save_file)
        fm.add_command(label="Save As...", accelerator="Ctrl+Shift+S", command=self.save_file_as)
        fm.add_command(label="Save All", command=self.save_all)
        fm.add_separator()
        fm.add_command(label="Close Tab", accelerator="Ctrl+W", command=self.close_tab)
        fm.add_separator()
        self.recent_menu = tk.Menu(fm, tearoff=0); self._rebuild_recent()
        fm.add_cascade(label="Recent Files", menu=self.recent_menu)
        fm.add_separator()
        fm.add_command(label="Exit", accelerator="Alt+F4", command=self._on_close)
        mb.add_cascade(label="File", menu=fm)
        # Edit
        em = tk.Menu(mb, tearoff=0)
        em.add_command(label="Undo", accelerator="Ctrl+Z", command=self._undo)
        em.add_command(label="Redo", accelerator="Ctrl+Y", command=self._redo)
        em.add_separator()
        em.add_command(label="Cut", accelerator="Ctrl+X", command=self._cut)
        em.add_command(label="Copy", accelerator="Ctrl+C", command=self._copy)
        em.add_command(label="Paste", accelerator="Ctrl+V", command=self._paste)
        em.add_command(label="Delete", command=self._delete)
        em.add_separator()
        em.add_command(label="Select All", accelerator="Ctrl+A", command=self._select_all)
        em.add_separator()
        em.add_command(label="Duplicate Line", accelerator="Ctrl+D", command=self._dup_line)
        em.add_command(label="Delete Line", accelerator="Ctrl+Shift+K", command=self._del_line)
        em.add_command(label="Move Line Up", accelerator="Alt+Up", command=self._mv_up)
        em.add_command(label="Move Line Down", accelerator="Alt+Down", command=self._mv_down)
        em.add_separator()
        em.add_command(label="UPPERCASE", accelerator="Ctrl+Shift+U", command=self._to_upper)
        em.add_command(label="lowercase", accelerator="Ctrl+U", command=self._to_lower)
        em.add_separator()
        em.add_command(label="Insert Timestamp", command=self._ins_ts)
        mb.add_cascade(label="Edit", menu=em)
        # Search
        sm = tk.Menu(mb, tearoff=0)
        sm.add_command(label="Find / Replace...", accelerator="Ctrl+H", command=self._show_find)
        sm.add_command(label="Find Next", accelerator="F3", command=self._find_next)
        sm.add_command(label="Find Previous", accelerator="Shift+F3", command=self._find_prev)
        sm.add_separator()
        sm.add_command(label="Go to Line...", accelerator="Ctrl+G", command=self._goto_line)
        mb.add_cascade(label="Search", menu=sm)
        # View
        vm = tk.Menu(mb, tearoff=0)
        vm.add_checkbutton(label="Word Wrap", variable=self.word_wrap, command=self._toggle_wrap)
        vm.add_checkbutton(label="Line Numbers", variable=self.show_linenums, command=self._toggle_ln)
        vm.add_checkbutton(label="Toolbar", variable=self.show_toolbar_var, command=self._toggle_tb)
        vm.add_checkbutton(label="Status Bar", variable=self.show_statusbar_var, command=self._toggle_sb)
        vm.add_separator()
        vm.add_command(label="Zoom In", accelerator="Ctrl++", command=self._zi)
        vm.add_command(label="Zoom Out", accelerator="Ctrl+-", command=self._zo)
        vm.add_command(label="Reset Zoom", accelerator="Ctrl+0", command=self._zr)
        mb.add_cascade(label="View", menu=vm)
        # Tools
        tm = tk.Menu(mb, tearoff=0)
        tm.add_command(label="Bitmap Editor", command=self._open_bitmap_editor)
        tm.add_command(label="Bitmap Loader", command=self._open_bitmap_loader)
        tm.add_separator()
        tm.add_command(label="MIDI to BAS Converter", command=self._open_midi_converter)
        mb.add_cascade(label="Tools", menu=tm)
        # Serial
        serm = tk.Menu(mb, tearoff=0)
        serm.add_command(label="Start Programming (Send File)", accelerator="F5", command=self.start_prog)
        serm.add_command(label="Stop Programming", command=self.stop_prog)
        serm.add_separator()
        serm.add_command(label="Refresh COM Ports", command=self._refresh_ports)
        mb.add_cascade(label="Serial", menu=serm)
        # Help
        hm = tk.Menu(mb, tearoff=0)
        hm.add_command(label="About", command=self._about)
        hm.add_command(label="Tiny Basic Reference", command=self._reference)
        mb.add_cascade(label="Help", menu=hm)
        self.root.config(menu=mb)

    # --- TOOLS ---
    def _open_bitmap_editor(self):
        if not PIL_AVAILABLE:
            messagebox.showerror("Missing Dependency",
                "The Bitmap Editor requires Pillow.\n\n"
                "Install it with:\n  pip install Pillow")
            return
        BitmapEditorWindow(self.root)

    def _open_bitmap_loader(self):
        if not SERIAL_AVAILABLE:
            messagebox.showerror("Missing Dependency",
                "The Bitmap Loader requires pyserial.\n\n"
                "Install it with:\n  pip install pyserial")
            return
        BitmapLoaderWindow(self.root, self.selected_port)

    def _open_midi_converter(self):
        if not MIDI_AVAILABLE:
            messagebox.showerror("Missing Dependency",
                "The MIDI to BAS Converter requires pretty_midi.\n\n"
                "Install it with:\n  pip install pretty_midi")
            return
        MidiConverterWindow(self.root)

    # --- TOOLBAR ---
    def _build_toolbar(self):
        self.toolbar = tk.Frame(self.root, bg=THEME["toolbar_bg"], bd=0, height=32)
        if self.show_toolbar_var.get(): self.toolbar.pack(side="top", fill="x")
        bk = dict(relief="flat", bg=THEME["toolbar_bg"], activebackground="#D0D0D0",
                  bd=0, padx=6, font=("Segoe UI",9))
        tk.Button(self.toolbar, text="New", command=self.new_file, **bk).pack(side="left", padx=1)
        tk.Button(self.toolbar, text="Open", command=self.open_file, **bk).pack(side="left", padx=1)
        tk.Button(self.toolbar, text="Save", command=self.save_file, **bk).pack(side="left", padx=1)
        ttk.Separator(self.toolbar, orient="vertical").pack(side="left", fill="y", padx=6, pady=4)
        tk.Button(self.toolbar, text="Undo", command=self._undo, **bk).pack(side="left", padx=1)
        tk.Button(self.toolbar, text="Redo", command=self._redo, **bk).pack(side="left", padx=1)
        ttk.Separator(self.toolbar, orient="vertical").pack(side="left", fill="y", padx=6, pady=4)
        tk.Button(self.toolbar, text="Find", command=self._show_find, **bk).pack(side="left", padx=1)
        ttk.Separator(self.toolbar, orient="vertical").pack(side="left", fill="y", padx=6, pady=4)
        tk.Label(self.toolbar, text="COM:", bg=THEME["toolbar_bg"], font=("Segoe UI",9)).pack(side="left", padx=(8,2))
        self.port_combo = ttk.Combobox(self.toolbar, textvariable=self.selected_port,
            values=["None"]+list_com_ports(), state="readonly", width=10, font=("Segoe UI",9))
        self.port_combo.pack(side="left", padx=2)
        self.port_combo.bind("<Button-1>", lambda e: self._refresh_combo())
        tk.Button(self.toolbar, text="Upload", command=self.start_prog, fg="#006600", **bk).pack(side="left", padx=4)
        tk.Button(self.toolbar, text="Stop", command=self.stop_prog, fg="#CC0000", **bk).pack(side="left", padx=1)

    def _build_findbar(self): self.findbar = FindReplaceBar(self.root, self)
    def _build_notebook(self):
        self.notebook = ttk.Notebook(self.root); self.notebook.pack(fill="both", expand=True)
        self.notebook.bind("<<NotebookTabChanged>>", self._on_tab_ch)
        self.notebook.bind("<Button-2>", self._mid_close)
        self.notebook.bind("<Button-3>", self._tab_ctx)

    def _build_statusbar(self):
        self.statusbar = tk.Frame(self.root, bg=THEME["statusbar_bg"], height=24)
        if self.show_statusbar_var.get(): self.statusbar.pack(side="bottom", fill="x")
        lk = dict(bg=THEME["statusbar_bg"], fg=THEME["statusbar_fg"], font=("Segoe UI",9), padx=10)
        self.st_pos = tk.Label(self.statusbar, text="Ln 1, Col 1", anchor="w", **lk); self.st_pos.pack(side="left")
        self.st_ch  = tk.Label(self.statusbar, text="0 characters", anchor="w", **lk); self.st_ch.pack(side="left")
        self.st_ln  = tk.Label(self.statusbar, text="1 line", anchor="w", **lk); self.st_ln.pack(side="left")
        self.st_enc = tk.Label(self.statusbar, text="UTF-8", anchor="e", **lk); self.st_enc.pack(side="right")
        self.st_fp  = tk.Label(self.statusbar, text="", anchor="e", **lk); self.st_fp.pack(side="right")

    def _bind_shortcuts(self):
        r = self.root
        for k, fn in [
            ("<Control-n>", self.new_file), ("<Control-o>", self.open_file),
            ("<Control-s>", self.save_file), ("<Control-Shift-S>", self.save_file_as),
            ("<Control-w>", self.close_tab), ("<Control-h>", self._show_find),
            ("<Control-f>", self._show_find), ("<F3>", self._find_next),
            ("<Shift-F3>", self._find_prev), ("<Control-g>", self._goto_line),
            ("<Control-d>", self._dup_line), ("<Control-Shift-K>", self._del_line),
            ("<Control-plus>", self._zi), ("<Control-equal>", self._zi),
            ("<Control-minus>", self._zo), ("<Control-0>", self._zr),
            ("<F5>", self.start_prog), ("<Control-Shift-U>", self._to_upper),
            ("<Control-u>", self._to_lower), ("<Alt-Up>", self._mv_up),
            ("<Alt-Down>", self._mv_down), ("<Escape>", lambda: self.findbar.hide()),
        ]:
            r.bind_all(k, lambda e, f=fn: f())
        r.bind_all("<Control-MouseWheel>", self._ctrl_zoom)

    # --- TABS ---
    def _add_tab(self, fp=None, content=""):
        tab = EditorTab(self.notebook, self, filepath=fp, content=content)
        t = os.path.basename(fp) if fp else "Untitled"
        self.notebook.add(tab, text=f"  {t}  "); self.notebook.select(tab)
        self.tabs.append(tab); tab.set_wrap(self.word_wrap.get())
        self._toggle_ln(); tab.text.focus_set(); return tab

    def _current_tab(self):
        try:
            c = self.notebook.select()
            return self.notebook.nametowidget(c) if c else None
        except: return None

    def _update_tab_title(self, tab):
        try: idx = self.notebook.index(tab)
        except tk.TclError: return
        n = os.path.basename(tab.filepath) if tab.filepath else "Untitled"
        p = "* " if tab.modified else "  "
        self.notebook.tab(idx, text=f"{p}{n}  ")

    def _on_tab_ch(self, _e=None):
        self._update_statusbar(); tab = self._current_tab()
        if tab: tab._highlight_current_line(); tab._schedule_full_highlight(); tab.linenumbers.redraw()

    def _mid_close(self, e):
        try: self.notebook.select(self.notebook.index(f"@{e.x},{e.y}")); self.close_tab()
        except: pass

    def _tab_ctx(self, e):
        try: idx = self.notebook.index(f"@{e.x},{e.y}")
        except: return
        m = tk.Menu(self.root, tearoff=0)
        m.add_command(label="Close", command=lambda: (self.notebook.select(idx), self.close_tab()))
        m.add_command(label="Close Others", command=lambda: self._close_others(idx))
        m.add_command(label="Close All", command=self._close_all)
        m.tk_popup(e.x_root, e.y_root)

    def _close_others(self, ki):
        keep = self.notebook.nametowidget(self.notebook.tabs()[ki])
        self._closing_all = True
        try:
            for t in list(self.tabs):
                if t is not keep: self.notebook.select(t);
                if t is not keep and not self._close_int(False): break
        finally: self._closing_all = False
        if not self.tabs: self.new_file()

    def _close_all(self):
        self._closing_all = True
        try:
            for t in list(self.tabs):
                self.notebook.select(t)
                if not self._close_int(False): break
        finally: self._closing_all = False
        if not self.tabs: self.new_file()

    # --- FILES ---
    def new_file(self): self._add_tab()
    def open_file(self, fp=None):
        if fp is None: fp = filedialog.askopenfilename(filetypes=FILETYPES, defaultextension=".bas")
        if not fp: return
        for t in self.tabs:
            if t.filepath and os.path.abspath(t.filepath)==os.path.abspath(fp):
                self.notebook.select(t); return
        try:
            with open(fp, "r", encoding="utf-8", errors="replace") as f: content = f.read()
        except Exception as e: messagebox.showerror("Error", str(e)); return
        tab = self._add_tab(fp=fp, content=content)
        tab.modified = False; self._update_tab_title(tab)
        self._add_recent(fp); self._update_statusbar()

    def save_file(self):
        tab = self._current_tab()
        if not tab: return
        if tab.filepath: self._write(tab, tab.filepath)
        else: self.save_file_as()
    def save_file_as(self):
        tab = self._current_tab()
        if not tab: return
        fp = filedialog.asksaveasfilename(filetypes=FILETYPES, defaultextension=".bas")
        if fp: self._write(tab, fp)
    def save_all(self):
        cur = self._current_tab()
        for t in self.tabs: self.notebook.select(t); self.save_file()
        if cur: self.notebook.select(cur)
    def _write(self, tab, fp):
        try:
            with open(fp, "w", encoding="utf-8", newline="\n") as f: f.write(tab.get_content())
            tab.filepath = fp; tab.modified = False
            self._update_tab_title(tab); self._add_recent(fp); self._update_statusbar()
        except Exception as e: messagebox.showerror("Error", str(e))
    def close_tab(self): return self._close_int(True)
    def _close_int(self, create=True):
        tab = self._current_tab()
        if not tab: return True
        if tab.modified:
            n = os.path.basename(tab.filepath) if tab.filepath else "Untitled"
            a = messagebox.askyesnocancel("Save?", f'Save changes to "{n}"?')
            if a is None: return False
            if a: self.save_file();
            if a and tab.modified: return False
        self.tabs.remove(tab); self.notebook.forget(tab); tab.destroy()
        if not self.tabs and create: self.new_file()
        return True

    def _add_recent(self, fp):
        r = self.cfg.get("recent_files",[]); a = os.path.abspath(fp)
        if a in r: r.remove(a)
        r.insert(0, a); self.cfg["recent_files"] = r[:15]; save_config(self.cfg); self._rebuild_recent()
    def _rebuild_recent(self):
        self.recent_menu.delete(0, "end")
        for fp in self.cfg.get("recent_files",[]):
            self.recent_menu.add_command(label=fp, command=lambda p=fp: self.open_file(p))
        if not self.cfg.get("recent_files"):
            self.recent_menu.add_command(label="(empty)", state="disabled")

    # --- EDIT ---
    def _undo(self):
        t = self._current_tab()
        if t:
            try: t.text.edit_undo()
            except tk.TclError: pass
    def _redo(self):
        t = self._current_tab()
        if t:
            try: t.text.edit_redo()
            except tk.TclError: pass
    def _cut(self):
        t = self._current_tab()
        if t: t.text.event_generate("<<Cut>>")
    def _copy(self):
        t = self._current_tab()
        if t: t.text.event_generate("<<Copy>>")
    def _paste(self):
        t = self._current_tab()
        if t: t.text.event_generate("<<Paste>>")
    def _delete(self):
        t = self._current_tab()
        if t and t.get_selection(): t.text.delete("sel.first","sel.last")
    def _select_all(self):
        t = self._current_tab()
        if t: t.text.tag_add("sel","1.0","end"); return "break"
    def _dup_line(self):
        t = self._current_tab()
        if not t: return
        ln = t.text.index("insert").split(".")[0]
        c = t.text.get(f"{ln}.0", f"{ln}.end"); t.text.insert(f"{ln}.end", "\n"+c)
    def _del_line(self):
        t = self._current_tab()
        if not t: return
        ln = t.text.index("insert").split(".")[0]; t.text.delete(f"{ln}.0", f"{ln}.end+1c")
    def _mv_up(self):
        t = self._current_tab()
        if not t: return "break"
        p = t.text.index("insert"); ln = int(p.split(".")[0])
        if ln <= 1: return "break"
        cur = t.text.get(f"{ln}.0",f"{ln}.end"); abv = t.text.get(f"{ln-1}.0",f"{ln-1}.end")
        t.text.delete(f"{ln-1}.0",f"{ln}.end"); t.text.insert(f"{ln-1}.0", cur+"\n"+abv)
        t.text.mark_set("insert", f"{ln-1}.{p.split('.')[1]}"); return "break"
    def _mv_down(self):
        t = self._current_tab()
        if not t: return "break"
        p = t.text.index("insert"); ln = int(p.split(".")[0])
        last = int(t.text.index("end").split(".")[0])-1
        if ln >= last: return "break"
        cur = t.text.get(f"{ln}.0",f"{ln}.end"); blw = t.text.get(f"{ln+1}.0",f"{ln+1}.end")
        t.text.delete(f"{ln}.0",f"{ln+1}.end"); t.text.insert(f"{ln}.0", blw+"\n"+cur)
        t.text.mark_set("insert", f"{ln+1}.{p.split('.')[1]}"); return "break"
    def _to_upper(self):
        t = self._current_tab()
        if not t: return "break"
        s = t.get_selection()
        if s: t.text.delete("sel.first","sel.last"); t.text.insert("insert", s.upper())
        return "break"
    def _to_lower(self):
        t = self._current_tab()
        if not t: return "break"
        s = t.get_selection()
        if s: t.text.delete("sel.first","sel.last"); t.text.insert("insert", s.lower())
        return "break"
    def _ins_ts(self):
        t = self._current_tab()
        if t: t.text.insert("insert", datetime.datetime.now().strftime("REM %Y-%m-%d %H:%M:%S"))

    # --- SEARCH ---
    def _show_find(self): self.findbar.show(replace=True)
    def _find_next(self):
        if not self.findbar.visible: self.findbar.show()
        self.findbar.find_next()
    def _find_prev(self):
        if not self.findbar.visible: self.findbar.show()
        self.findbar.find_prev()
    def _goto_line(self):
        t = self._current_tab()
        if not t: return
        ln = simpledialog.askinteger("Go to Line", "Line number:", minvalue=1, parent=self.root)
        if ln:
            t.text.mark_set("insert",f"{ln}.0"); t.text.see(f"{ln}.0")
            t._highlight_current_line(); t._schedule_full_highlight(); t.linenumbers.redraw()

    # --- VIEW ---
    def _toggle_wrap(self):
        for t in self.tabs: t.set_wrap(self.word_wrap.get())
    def _toggle_ln(self):
        show = self.show_linenums.get()
        for t in self.tabs:
            if show: t.linenumbers.pack(side="left", fill="y", before=t.text); t.linenumbers.redraw()
            else: t.linenumbers.pack_forget()
    def _toggle_tb(self):
        if self.show_toolbar_var.get():
            self.toolbar.pack(side="top", fill="x",
                              before=(self.findbar if self.findbar.visible else self.notebook))
        else: self.toolbar.pack_forget()
    def _toggle_sb(self):
        if self.show_statusbar_var.get(): self.statusbar.pack(side="bottom", fill="x")
        else: self.statusbar.pack_forget()
    def _zi(self): self._cfs(1)
    def _zo(self): self._cfs(-1)
    def _zr(self): self._sfs(11)
    def _cfs(self, d):
        ns = self.editor_font.cget("size")+d
        if 6 <= ns <= 72: self._sfs(ns)
    def _sfs(self, s):
        self.editor_font.configure(size=s); self.editor_font_bold.configure(size=s)
        self.editor_font_italic.configure(size=s)
        for t in self.tabs: t.update_font()
        self.cfg["font_size"] = s; save_config(self.cfg)
    def _ctrl_zoom(self, e):
        if e.delta > 0: self._zi()
        else: self._zo()

    # --- SERIAL ---
    def _refresh_ports(self):
        p = list_com_ports()
        messagebox.showinfo("COM Ports", "\n".join(p) if p else "No COM ports found.")
        self._refresh_combo()
    def _refresh_combo(self):
        p = ["None"]+list_com_ports(); self.port_combo["values"] = p
        if self.selected_port.get() not in p: self.selected_port.set("None")

    def start_prog(self):
        if not SERIAL_AVAILABLE:
            messagebox.showerror("Error", "pyserial not installed.\nInstall: pip install pyserial"); return
        port = self.selected_port.get(); avail = list_com_ports()
        if port == "None": messagebox.showerror("Error", "Select a COM port."); return
        if port not in avail:
            messagebox.showwarning("Warning", "Port unavailable."); self.selected_port.set("None"); return
        if not messagebox.askokcancel("Start Programming",
                'Type "SERLOAD" on the AVR-SBC.\nStart programming?'): return
        tab = self._current_tab()
        if not tab: return
        content = tab.get_content()
        def _s():
            try:
                s = serial.Serial(port, 9600, timeout=5)
                s.write(content.encode()); s.write(b'\x00'); s.close()
                self.root.after(0, lambda: messagebox.showinfo("Success", "Programming successful!\nType RUN."))
            except serial.SerialException as e:
                self.root.after(0, lambda: messagebox.showerror("Error", str(e)))
        threading.Thread(target=_s, daemon=True).start()

    def stop_prog(self):
        if not SERIAL_AVAILABLE:
            messagebox.showerror("Error", "pyserial not installed."); return
        port = self.selected_port.get(); avail = list_com_ports()
        if port == "None": messagebox.showerror("Error", "Select a COM port."); return
        if port not in avail:
            messagebox.showwarning("Warning", "Port unavailable."); self.selected_port.set("None"); return
        if not messagebox.askokcancel("Stop Programming", "Stop programming mode?"): return
        def _s():
            try:
                s = serial.Serial(port, 9600, timeout=5); s.write(b'\x00'); s.close()
                self.root.after(0, lambda: messagebox.showinfo("Success", "Stopped."))
            except serial.SerialException as e:
                self.root.after(0, lambda: messagebox.showerror("Error", str(e)))
        threading.Thread(target=_s, daemon=True).start()

    # --- STATUSBAR ---
    def _update_statusbar(self):
        tab = self._current_tab()
        if not tab: return
        try: pos = tab.text.index("insert")
        except tk.TclError: return
        ln, col = pos.split(".")
        self.st_pos.config(text=f"Ln {ln}, Col {int(col)+1}")
        c = tab.get_content(); ch = len(c); ls = c.count("\n")+1
        self.st_ch.config(text=f"{ch} characters")
        self.st_ln.config(text=f"{ls} line{'s' if ls!=1 else ''}")
        self.st_fp.config(text=tab.filepath if tab.filepath else "Untitled")

    # --- HELP ---
    def _about(self):
        messagebox.showinfo("About",
            f"{APP_TITLE} v{APP_VERSION}\n\n"
            "A lightweight editor for Tiny Basic (.bas) files\n"
            "with serial upload & tools for AVR-SBC.\n\n"
            "Built with Python & Tkinter.")
    def _reference(self):
        w = tk.Toplevel(self.root); w.title("Tiny Basic Reference"); w.geometry("520x480")
        t = tk.Text(w, font=("Consolas",10), wrap="word", bg="#FFFFF0", padx=10, pady=10)
        t.pack(fill="both", expand=True); t.insert("1.0", TINY_BASIC_REFERENCE)
        t.configure(state="disabled")

    # --- CLOSE ---
    def _on_close(self):
        self._closing_all = True
        try:
            for t in list(self.tabs):
                self.notebook.select(t)
                if not self._close_int(False): self._closing_all = False; return
        finally: self._closing_all = False
        self.cfg["window_geometry"] = self.root.geometry()
        self.cfg["word_wrap"] = self.word_wrap.get()
        self.cfg["show_line_numbers"] = self.show_linenums.get()
        self.cfg["show_toolbar"] = self.show_toolbar_var.get()
        self.cfg["show_statusbar"] = self.show_statusbar_var.get()
        self.cfg["last_com_port"] = self.selected_port.get()
        self.cfg["font_size"] = self.editor_font.cget("size")
        save_config(self.cfg); self.root.destroy()


TINY_BASIC_REFERENCE = """\
AVR-SBC TINYBASIC PLUS QUICK REFERENCE
=======================================

PROGRAM CONTROL
  LIST [line]                List program (from line)
  RUN                        Run the program
  NEW                        Clear the program
  END / STOP                 Halt the program
  BYE                        Exit to system

STATEMENTS
  PRINT expr [, expr ...] [;]  Print values (comma=tab, semicolon=no newline)
  ? expr [, expr ...]          Shorthand for PRINT
  ' text                       Comment (shorthand for REM)
  INPUT var                    Read a number from the user into var
  LET var = expr               Assign a value (LET is optional: var = expr)
  IF expr THEN statement       Conditional; THEN is optional
  GOTO expr                    Jump to line number
  GOSUB expr                   Call subroutine at line number
  RETURN                       Return from GOSUB
  FOR var = expr TO expr [STEP expr]
  NEXT var                     Loop construct
  REM text                     Comment
  DELAY ms                     Pause for ms milliseconds (breakable with ESC)
  RSEED expr                   Seed the random number generator
  POKE val, addr               Write byte val to program RAM address addr
  MEM                          Print free RAM (and EEPROM if available)
  CHAIN                        Load next program from EEPROM and run it

OPERATORS
  +  -  *  /                   Arithmetic
  =  <>  !=  <  >  <=  >=     Comparison (return 1 if true, 0 if false)

FUNCTIONS  (used inside expressions)
  ABS(x)     Absolute value of x
  RND(x)     Random integer 0 .. x-1
  PEEK(x)    Read byte from program RAM at offset x
  AREAD(x)   Analog read from ADC pin x (returns 0-1023)
  DREAD(x)   Digital read from pin x (returns 0 or 1)

GPIO
  DWRITE pin, HIGH/LOW/val     Digital write to GPIO pin
  AWRITE pin, val              Analog (PWM) write to GPIO pin

TONE  (if ENABLE_TONES compiled in)
  TONE freq, dur               Start tone at freq Hz for dur ms (non-blocking)
  TONEW freq, dur              Start tone and wait for it to finish
  NOTONE                       Stop tone immediately

GRAPHICS
  CLS                          Clear the screen
  DRAWPIX x, y, c              Draw pixel at (x,y); c: 0=clear 1=set 2=XOR
  DRAWLINE x0,y0,x1,y1,c      Draw line; c: 0=clear 1=set 2=XOR
  DRAWRECT x,y,w,h,c,f        Draw rectangle; c=outline, f=fill color
  DRAWCIRC cx,cy,r,c,f         Draw circle; c=outline, f=fill color
  DRAWCHAR x,y,ch              Draw one character at (x,y)
  GETPIX x, y                  Print pixel value at (x,y) to screen

SERIAL (UART1: PD2=RX, PD3=TX, 9600 baud)
  SEROPEN                      Open UART1
  SERCLOSE                     Close UART1
  SERPRINT expr [, expr ...]   Print values over UART1
  SERREAD                      Read a line from UART1 (1 second timeout)
  SERLOAD                      Load a program over UART1 (serial uploader)

INTERNAL EEPROM  (if ENABLE_EEPROM compiled in)
  ESAVE                        Save program to internal EEPROM
  ELOAD                        Load program from internal EEPROM
  ELIST                        List contents of internal EEPROM
  EFORMAT                      Erase internal EEPROM
  EPOKE val, addr              Write byte val to EEPROM address addr
  EPEEK var, addr              Read byte from EEPROM address into var

EXTERNAL EEPROM  (if ENABLE_XEEPROM compiled in)
  XSAVE                        Save program to external EEPROM
  XLOAD                        Load program from external EEPROM
  XLIST                        List contents of external EEPROM
  XFORMAT                      Erase external EEPROM
  XPOKE val, addr              Write byte val to external EEPROM address addr
  XPEEK var, addr              Read byte from external EEPROM into var

KEYBOARD
  INKEY var                    Non-blocking key read; stores ASCII in var (0=none)

NOTES
  - Line numbers required (e.g. 10, 20, 30 ...)
  - Variables: single uppercase letters A-Z (16-bit signed integers)
  - Strings in double or single quotes
  - Multiple statements per line separated by ':'
  - Screen resolution: 320x240 pixels (HRES x VRES)
  - Program RAM: 5500 bytes total
  - Break execution at any time with ESC or Ctrl-C
"""

def main():
    root = tk.Tk()
    app = TinyBasicEditor(root)
    root.mainloop()

if __name__ == "__main__":
    main()
