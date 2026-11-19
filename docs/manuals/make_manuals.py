#!/usr/bin/env python3
"""Generates the LT AUDIO user manuals (PDF, Spanish). Run: python3 docs/manuals/make_manuals.py"""
import os, tempfile
from PIL import Image
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_CENTER
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Table, TableStyle,
                                Image as RLImage, PageBreak, KeepTogether, NextPageTemplate)

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.dirname(os.path.abspath(__file__))
FONTS = os.path.join(ROOT, "shared", "fonts")
for name, f in [("M-Light", "Montserrat-Light.ttf"), ("M", "Montserrat-Regular.ttf"),
                ("M-Semi", "Montserrat-SemiBold.ttf"), ("M-Bold", "Montserrat-Bold.ttf")]:
    pdfmetrics.registerFont(TTFont(name, os.path.join(FONTS, f)))

BG = colors.HexColor("#0b0812"); PANEL = colors.HexColor("#17122a"); EDGE = colors.HexColor("#3a2d5c")
PURPLE = colors.HexColor("#7b2cff"); LILAC = colors.HexColor("#b9a2ff"); TEXT = colors.HexColor("#ece8f7")
DIM = colors.HexColor("#a79fc0")

S = {
    "h1": ParagraphStyle("h1", fontName="M-Light", fontSize=30, leading=36, textColor=TEXT),
    "h2": ParagraphStyle("h2", fontName="M-Semi", fontSize=15, leading=20, textColor=LILAC, spaceBefore=12, spaceAfter=6),
    "h3": ParagraphStyle("h3", fontName="M-Semi", fontSize=11.5, leading=15, textColor=TEXT, spaceBefore=8, spaceAfter=3),
    "p": ParagraphStyle("p", fontName="M", fontSize=9.6, leading=14, textColor=TEXT, spaceAfter=5),
    "dim": ParagraphStyle("dim", fontName="M", fontSize=8.5, leading=12, textColor=DIM, spaceAfter=4),
    "cap": ParagraphStyle("cap", fontName="M", fontSize=8, leading=11, textColor=DIM, alignment=TA_CENTER, spaceAfter=8),
    "cell": ParagraphStyle("cell", fontName="M", fontSize=8.6, leading=11.5, textColor=TEXT),
    "cellb": ParagraphStyle("cellb", fontName="M-Semi", fontSize=8.8, leading=11.5, textColor=LILAC),
    "tip": ParagraphStyle("tip", fontName="M", fontSize=9.2, leading=13.5, textColor=TEXT),
    "cover": ParagraphStyle("cover", fontName="M-Light", fontSize=46, leading=52, textColor=TEXT, alignment=TA_CENTER),
    "coversub": ParagraphStyle("coversub", fontName="M", fontSize=12, leading=18, textColor=DIM, alignment=TA_CENTER),
}
TMP = tempfile.mkdtemp()


def page_bg(c, doc):
    c.saveState()
    w, h = A4
    c.setFillColor(BG); c.rect(0, 0, w, h, stroke=0, fill=1)
    c.setFillColor(colors.HexColor("#1d1236")); c.circle(w * 0.85, h * 0.92, 160, stroke=0, fill=1)
    c.setFillColor(colors.HexColor("#140d26")); c.circle(w * 0.1, h * 0.1, 120, stroke=0, fill=1)
    c.setFont("M", 7.5); c.setFillColor(DIM)
    c.drawString(18 * mm, 10 * mm, "LT AUDIO  ·  " + doc.lt_title)
    c.drawRightString(w - 18 * mm, 10 * mm, str(doc.page))
    c.restoreState()


def img(path, width):
    full = os.path.join(ROOT, path)
    if not os.path.exists(full):
        return None
    png = os.path.join(TMP, os.path.basename(path) + ".png")
    im = Image.open(full).convert("RGB"); im.save(png)
    w, h = im.size
    return RLImage(png, width=width, height=width * h / w)


def table(rows, widths):
    data = [[Paragraph(h, S["cellb"]) for h in ["Control", "Rango / opciones", "Qué hace"]]]
    for r in rows:
        data.append([Paragraph(r[0], S["cellb"]), Paragraph(r[1], S["cell"]), Paragraph(r[2], S["cell"])])
    t = Table(data, colWidths=widths, repeatRows=1)
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#2a1d4f")),
        ("BACKGROUND", (0, 1), (-1, -1), PANEL),
        ("ROWBACKGROUNDS", (0, 1), (-1, -1), [PANEL, colors.HexColor("#1b1531")]),
        ("LINEBELOW", (0, 0), (-1, -1), 0.4, EDGE),
        ("BOX", (0, 0), (-1, -1), 0.6, EDGE),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("TOPPADDING", (0, 0), (-1, -1), 5), ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
        ("LEFTPADDING", (0, 0), (-1, -1), 6), ("RIGHTPADDING", (0, 0), (-1, -1), 6),
    ]))
    return t


def tipbox(text):
    t = Table([[Paragraph("<font name='M-Semi' color='#b9a2ff'>TIP  </font>" + text, S["tip"])]], colWidths=[174 * mm])
    t.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, -1), colors.HexColor("#22173f")),
                           ("BOX", (0, 0), (-1, -1), 0.8, PURPLE),
                           ("TOPPADDING", (0, 0), (-1, -1), 7), ("BOTTOMPADDING", (0, 0), (-1, -1), 7),
                           ("LEFTPADDING", (0, 0), (-1, -1), 9)]))
    return t


COMMON_INSTALL = [
    ("h2", "Instalación"),
    ("p", "<b>Mac:</b> abre el instalador <font name='M-Semi'>.pkg</font> del plugin (o el de todos los plugins) → "
          "<b>Continuar → Instalar</b> → contraseña de tu Mac. Si macOS dice que es de un desarrollador no identificado: "
          "<b>clic derecho → Abrir → Abrir</b>. Se instala en VST3 y Audio Units."),
    ("p", "<b>Windows:</b> abre el instalador <font name='M-Semi'>.exe</font>. Si aparece \"Windows protegió su PC\": "
          "<b>Más información → Ejecutar de todas formas</b>. Se instala en <font name='M-Semi'>C:\\Program Files\\Common Files\\VST3</font>."),
    ("p", "En Ableton Live: <b>Settings → Plug-Ins → Rescan</b>. El plugin aparece en <b>Plug-Ins → LT Audio</b>."),
    ("h2", "Licencia"),
    ("p", "Tienes <b>10 días de prueba</b> completos. Para activar: en la pantalla de activación pulsa <b>COPY</b> para copiar tu "
          "<b>Machine ID</b> y envíaselo a LT. Recibirás una licencia (texto o archivo <font name='M-Semi'>.lic</font>): "
          "pégala o arrastra el archivo y pulsa <b>ACTIVATE</b>. Cada licencia vale para <b>2 computadoras</b>. "
          "Una licencia <b>LT ALL PLUGINS</b> activa todos los plugins a la vez."),
    ("p", "<b>THEME</b> (arriba) cambia entre el modo oscuro (negro y morado) y el modo claro (celeste y amarillo)."),
]


PRODUCTS = []

# ------------------------------------------------------------------ TYPE BEATS
PRODUCTS.append(dict(
    file="LT-TYPE-BEATS-Manual.pdf", title="LT TYPE BEATS", subtitle="Analizador de BPM, tonalidad y acordes",
    image="design/type-beats/main-dark.webp",
    intro="LT TYPE BEATS analiza una canción y te dice su <b>tempo (BPM)</b>, su <b>tonalidad</b> y la <b>progresión de acordes</b>. "
          "Puedes cargar un archivo o pegar un <b>link de YouTube</b>, corregir lo que detectó y llevar los acordes a Ableton como "
          "<b>MIDI</b> arrastrándolos a una pista. No modifica el audio de tu pista: es una herramienta de análisis.",
    sections=[
        ("Cómo se usa", [
            "<b>1.</b> Pon LT TYPE BEATS en cualquier pista de Ableton (por ejemplo una pista vacía).",
            "<b>2.</b> Arrastra una canción (WAV, MP3, AIFF, FLAC, M4A) a la zona de la onda, haz clic en ella para buscar un archivo, "
            "o pega un link de YouTube arriba y pulsa <b>GET</b>.",
            "<b>3.</b> En unos segundos aparecen el BPM, la tonalidad y los acordes.",
            "<b>4.</b> Corrige lo que quieras (tonalidad, acordes, BPM) y arrastra el MIDI a una pista MIDI.",
        ]),
    ],
    params=[
        ("Barra de YouTube + GET", "Link de youtube.com o youtu.be", "Descarga el audio, lo guarda como WAV en tu carpeta "
         "<i>Música/LT Type Beats</i> y lo analiza automáticamente. La primera vez instala el descargador (unos segundos)."),
        ("Onda (polvo de estrellas)", "Clic / arrastrar archivo", "Muestra la canción. <b>Clic</b> = escuchar desde ese punto "
         "(clic otra vez = parar). Suena por la salida del plugin. Suelta otra canción encima para reemplazarla."),
        ("Nombre del archivo", "Arrastrar", "Arrastra la etiqueta con el nombre a una <b>pista de audio</b> de Ableton para usar el WAV."),
        ("BPM", "Detectado", "Tempo de la canción. Los anillos laten a ese tempo."),
        ("TAP", "Pulsar al ritmo", "Si el BPM detectado no te convence, pulsa TAP al ritmo (3 o más veces). Muestra <i>TAPPED</i>."),
        ("1/2 y x2", "Botones", "Divide o duplica el BPM (útil cuando detecta la mitad o el doble, típico en trap)."),
        ("KEY", "Tonalidad + %", "Tonalidad principal y su porcentaje de confianza."),
        ("Lunas (alternativas)", "3 tonalidades", "Otras tonalidades posibles con su %. <b>Clic en una luna</b> = pasa a ser la principal."),
        ("Piano", "2 octavas", "Ilumina las notas del acorde seleccionado (o del que está sonando). La octava baja muestra el bajo."),
        ("CHORDS (constelación)", "Estrellas", "Cada estrella es un acorde con su tiempo debajo. <b>Clic en una estrella</b> = alternativas; "
         "elige una para reemplazar el acorde (el anterior queda como alternativa, así puedes deshacer). <b>&lt; &gt;</b> = ver más acordes."),
        ("EXPORT MIDI", "Botón", "Guarda los acordes como archivo .mid (bajo + acorde, con el tempo detectado)."),
        ("DRAG MIDI", "Arrastrar", "Arrastra este ícono directo a una <b>pista MIDI</b> de Ableton y se crea el clip con los acordes."),
        ("SETTINGS", "Menú", "Cambiar/abrir la carpeta de descargas, <b>actualizar el descargador de YouTube</b>, licencia, cargar canción."),
    ],
    tips=["Si los acordes salen raros, prueba con una versión <b>sin batería</b> (un stem de melodía): el análisis es más limpio.",
          "Si YouTube deja de descargar, usa <b>SETTINGS → Update YouTube downloader</b>: YouTube cambia seguido.",
          "Las correcciones que hagas se guardan con tu proyecto de Ableton y se exportan en el MIDI."],
))

# ------------------------------------------------------------------ PULSAR
PRODUCTS.append(dict(
    file="LT-PULSAR-Manual.pdf", title="LT PULSAR", subtitle="Sidechain / ducking",
    image="design/pulsar/main-dark.jpg",
    intro="LT PULSAR baja el volumen de una pista al ritmo de la canción, creando el clásico efecto de <b>sidechain</b> o \"bombeo\" "
          "sin necesidad de conectar un kick. También puede reaccionar a <b>notas MIDI</b> o a una señal de <b>sidechain real</b> "
          "(por ejemplo tu kick). Con <b>Frequency Focus</b> puedes aplicar el efecto solo a una parte del espectro, por ejemplo solo a los graves.",
    sections=[
        ("Cómo se usa", [
            "<b>1.</b> Pon LT PULSAR en la pista que quieres que \"bombee\" (bajo, pads, acordes…).",
            "<b>2.</b> Elige un preset o una de las 8 formas de curva, y la velocidad (1/4 es lo más común).",
            "<b>3.</b> Sube el <b>MIX</b> hasta que el bombeo se sienta bien.",
        ]),
    ],
    params=[
        ("MIX (knob grande)", "0 – 100 %", "Cuánto baja el volumen. 0 % = sin efecto; 100 % = la curva completa. La luz y las partículas crecen con el valor."),
        ("RATE", "1/1, 1/2, 1/4, 1/8, 1/16", "Cada cuánto se repite la curva, sincronizado al tempo de Ableton."),
        ("Formas de curva", "8 botones", "Formas predefinidas de ducking (suave, rápida, corte duro…). Punto de partida para editar."),
        ("Editor de curva", "Puntos y líneas", "<b>Arrastrar puntos</b> = cambiar la forma. <b>Doble clic</b> = añadir / quitar punto. "
         "<b>Arrastrar una línea</b> = curvarla. <b>SHIFT + arrastrar</b> = mover toda la curva en el tiempo (adelantar/atrasar). "
         "<b>Clic derecho</b> = más opciones: invertir arriba/abajo, invertir izquierda/derecha, enderezar línea, quitar punto, "
         "reiniciar el desplazamiento, volver a línea recta."),
        ("Visor de onda", "Automático", "Muestra el audio pasando y cómo la curva lo aplasta: original tenue detrás, procesado delante."),
        ("MIDI TRIGGER", "On / Off", "El ducking se dispara cada vez que llega una nota MIDI (envía una pista MIDI a LT PULSAR). Útil si el kick no cae en cada tiempo."),
        ("AUDIO TRIGGER", "Input / Sidechain", "El ducking se dispara con los golpes del audio: de la propia pista (INPUT) o de la señal de SIDECHAIN."),
        ("SIDECHAIN", "Selector de Ableton", "En el dispositivo de Ableton abre <b>Sidechain</b> y elige la pista del kick. El visor muestra esa señal, "
         "para ver dónde baja y dónde vuelve el volumen."),
        ("EQ (Frequency Focus)", "Ventana aparte", "Elige qué frecuencias se ven afectadas. <b>LOW CUT / HIGH CUT</b> arrastrables, botones "
         "<b>FULL · LOW · MID · HIGH</b>, <b>INVERT</b> (afecta todo menos la banda) y <b>SOLO</b> (escuchar solo la banda)."),
        ("Presets / SAVE", "Menú", "Presets de fábrica (Classic Pump 1/4, Smooth S, Soft Duck, Hard Cut, Short Duck, Long Release, Double Pump 1/8…) "
         "y <b>MY PRESETS</b> para guardar los tuyos."),
    ],
    tips=["Para bajos en trap: <b>Short Duck</b> + Frequency Focus en <b>LOW</b>: el sub deja espacio al kick sin perder los agudos.",
          "Para pads y acordes: <b>Soft Duck 1/4</b> o <b>Long Release</b> con MIX entre 50 y 70 %."],
))

# ------------------------------------------------------------------ KNOCK
PRODUCTS.append(dict(
    file="LT-KNOCK-Manual.pdf", title="LT KNOCK", subtitle="Procesador de drums y melodías",
    image="design/knock/main-dark.webp",
    intro="LT KNOCK le da golpe, color y carácter a tus drums (y también a melodías): pegada, saturación, distorsión, brillo, "
          "graves, compresión, efectos raros (crush, lo-fi, stutter, filtro, ring, reverse) y espacio (reverb y delay). "
          "El planeta central <b>KNOCK</b> controla cuánto de todo eso se mezcla con el sonido original.",
    sections=[
        ("Cómo se usa", [
            "<b>1.</b> Ponlo en tu bus de drums, en un kick, en un loop o en una melodía. Elige <b>DRUMS</b> o <b>MELODY</b> arriba.",
            "<b>2.</b> Ajusta la sección TONE para el color, añade FX y SPACE si quieres.",
            "<b>3.</b> Controla la intensidad general con el planeta <b>KNOCK</b>.",
        ]),
    ],
    params=[
        ("KNOCK (planeta)", "0 – 100 % (75 %)", "Mezcla general. 0 % = suena igual que el original; 100 % = todo procesado. El planeta se forma y se enciende al subirlo."),
        ("DRUMS | MELODY", "Modo", "Ajusta el comportamiento de los efectos según lo que proceses."),
        ("PUNCH", "0 – 100 %", "Realza el golpe inicial (transiente) para que los drums peguen más."),
        ("SATURATION", "0 – 100 %", "Calidez analógica, tipo cinta o consola."),
        ("DISTORTION", "0 – 100 %", "Distorsión fuerte. Tipo: <b>SOFT, HARD, FUZZ, TUBE</b>."),
        ("BRIGHT", "0 – 100 %", "Brillo y aire en los agudos."),
        ("LOW", "0 – 100 %", "Cuerpo y graves."),
        ("GLUE", "0 – 100 %", "Compresor que \"pega\" y aplasta el sonido."),
        ("WIDTH", "0 – 200 % (100 %)", "Amplitud estéreo. 0 % = mono, 100 % = original, 200 % = muy abierto."),
        ("CRUSH", "0 – 100 %", "Bitcrusher: sonido digital roto, tipo videojuego retro."),
        ("LO-FI", "0 – 100 %", "Vinilo/cassette viejo: ruido y ondulación."),
        ("STUTTER", "0 – 100 %", "Repite pedacitos del sonido al ritmo (glitch)."),
        ("FILTER", "LP 100 % ← OFF → HP 100 %", "A la izquierda filtro paso bajo (oscurece), a la derecha paso alto (adelgaza)."),
        ("RING", "0 – 100 %", "Modulación en anillo: metálico y robótico."),
        ("REVERSE", "0 – 100 %", "Reverb invertida que aparece antes del golpe."),
        ("REVERB", "0 – 100 %", "Cantidad de ambiente."),
        ("DELAY + tiempo", "0 – 100 % · 1/16, 1/8, 1/8D, 1/4, 1/4D, 1/2", "Ecos sincronizados al tempo (D = con puntillo)."),
        ("INPUT / OUTPUT", "-24 a +24 dB", "Volumen de entrada (empujar más la saturación) y volumen final."),
        ("CHAOS", "Botón + deshacer", "Mueve los knobs al azar dentro de rangos musicales. El <b>candado</b> de cada sección (TONE, FX, SPACE) la protege. La flecha de deshacer vuelve al sonido anterior."),
        ("EQ (Frequency Focus)", "Ventana aparte", "Aplica el procesado solo a una banda de frecuencias (FULL, LOW, MID, HIGH, INVERT, SOLO)."),
    ],
    tips=["Kick que no se escucha en el celular: <b>SATURATION</b> 30–40 % + <b>BRIGHT</b> 20 %.",
          "Usa <b>CHAOS</b> con TONE bloqueado para buscar efectos nuevos sin perder tu sonido base."],
))

# ------------------------------------------------------------------ ORBIT
PRODUCTS.append(dict(
    file="LT-ORBIT-Manual.pdf", title="LT ORBIT", subtitle="Delay",
    image="design/orbit/main-dark.webp",
    intro="LT ORBIT es un delay con 8 caracteres distintos. Cada eco se ve como un satélite orbitando alrededor del planeta "
          "(tu sonido original): más feedback, más órbitas.",
    sections=[],
    params=[
        ("Modos", "DIGITAL, TAPE, ANALOG, PING-PONG, REVERSE, SHIMMER, LO-FI, DUB", "El carácter del eco: limpio (DIGITAL), cálido y ondulado (TAPE), "
         "oscuro (ANALOG), rebota izquierda-derecha (PING-PONG), al revés (REVERSE), sube de octava (SHIMMER), sucio (LO-FI) o se oscurece y crece (DUB)."),
        ("TIME", "1/64 – 1/1 (1/4) o 1 – 2000 ms", "Tiempo entre ecos. <b>SYNC</b> = sincronizado al tempo; <b>MS</b> = milisegundos libres."),
        ("NORMAL / DOTTED / TRIPLET", "Feel", "Tiempo normal, con puntillo (×1,5) o en tresillo (×2/3)."),
        ("FEEDBACK", "0 – 100 % (45 %)", "Cuántas repeticiones. Valores altos = ecos muy largos."),
        ("MIX", "0 – 100 % (30 %)", "Cuánto eco se escucha frente al sonido original."),
        ("FREEZE ∞", "On / Off", "Congela los ecos y los repite infinitamente."),
        ("LOW CUT", "20 – 2000 Hz (120 Hz)", "Quita graves de los ecos para que no embarren la mezcla."),
        ("HIGH CUT", "1 – 20 kHz (9 kHz)", "Quita agudos de los ecos: más oscuros y de fondo."),
        ("WIDTH", "0 – 150 % (100 %)", "Amplitud estéreo de los ecos."),
        ("DUCK", "0 – 100 %", "Los ecos bajan mientras suena el sonido y suben en los silencios: no tapan la voz o la melodía."),
        ("WOBBLE", "0 – 100 % (20 %)", "Ondulación de cinta (wow &amp; flutter)."),
        ("DRIVE", "0 – 100 % (20 %)", "Saturación dentro de los ecos."),
        ("Presets / SAVE", "Menú", "Presets de fábrica agrupados por modo y tus propios presets."),
    ],
    tips=["Voces: modo <b>TAPE</b>, 1/4 DOTTED, FEEDBACK 30 %, <b>DUCK</b> 50 % y LOW CUT 300 Hz.",
          "Transiciones: sube FEEDBACK y activa <b>FREEZE</b> justo antes del drop."],
))

# ------------------------------------------------------------------ VOID
PRODUCTS.append(dict(
    file="LT-VOID-Manual.pdf", title="LT VOID", subtitle="Reverb de alta gama — 12 tipos",
    image="design/void/main-cloud.webp",
    intro="LT VOID es una reverb con 12 motores distintos. Cada <b>REVERB TYPE</b> cambia el sonido y el color de todo el plugin. "
          "El knob grande es un agujero negro: el <b>DECAY</b>. Al abrir el plugin carga <b>CLOUD</b>.",
    sections=[("Tipos de reverb", [
        "<b>CLOUD</b> — espacio atmosférico enorme e infinito, florece lento, modulación profunda.",
        "<b>PLATE</b> — placa metálica brillante y densa, cola suave: ideal para voces y snares.",
        "<b>HALL</b> — sala de conciertos grande, reflexiones naturales, amplia y profunda.",
        "<b>ROOM</b> — habitación real pequeña: ubica los sonidos en un espacio.",
        "<b>CHAMBER</b> — cámara de eco clásica, cálida y densa, carácter de estudio vintage.",
        "<b>VINTAGE</b> — reverb digital de rack de los 80: cola granulada de 12 bits con chorus.",
        "<b>SPRING</b> — tanque de resortes: metálico, \"boing\", dub y surf.",
        "<b>SHIMMER</b> — sube una octava: pads celestiales que ascienden.",
        "<b>REVERSE</b> — reverb al revés: la cola crece antes de cada sonido.",
        "<b>GATED</b> — reverb cortada de los 80: gran explosión y corte en seco.",
        "<b>LO-FI</b> — espacio polvoriento: aplastado, filtrado y con siseo de cinta.",
        "<b>VOID</b> — el vacío: espacio oscuro monocromo e interminable.",
    ])],
    params=[
        ("DECAY (agujero negro)", "0,2 – 30 s (8 s)", "Cuánto dura la cola de la reverb."),
        ("MIX", "0 – 100 % (35 %)", "Cantidad de reverb frente al sonido original."),
        ("PRE-DELAY", "0 – 500 ms (20 ms) o nota", "Tiempo antes de que empiece la reverb. <b>SYNC</b> = en notas (1/64 a 1/4, con tresillos y puntillo)."),
        ("SIZE", "0 – 100 % (90 %)", "Tamaño del espacio."),
        ("DAMPING HIGH FREQ", "1 – 20 kHz (8 kHz)", "A partir de qué frecuencia se apagan los agudos en la cola (más bajo = más oscuro)."),
        ("DAMPING LOW FREQ", "20 – 1000 Hz (120 Hz)", "Control de los graves dentro de la cola."),
        ("ATTACK", "0 – 100 % (20 %)", "Qué tan suave o inmediato arranca la reverb."),
        ("DIFFUSION", "0 – 100 % (70 %)", "Densidad: bajo = ecos separados, alto = cola lisa."),
        ("MOD RATE / DEPTH", "0,05 – 5 Hz · 0 – 100 %", "Movimiento en la cola: el toque \"caro\" y vivo."),
        ("LOW CUT / HIGH CUT", "20 – 1000 Hz · 1 – 20 kHz", "EQ de la reverb: limpia graves y agudos."),
        ("WIDTH", "0 – 150 % (100 %)", "Amplitud estéreo de la reverb."),
        ("DUCK", "0 – 100 %", "La reverb baja mientras suena el sonido y aparece en los silencios."),
        ("FREEZE ∞", "On / Off", "La cola queda suspendida infinitamente."),
        ("PRESETS / SAVE", "Menú", "Presets por tipo de reverb y los tuyos."),
    ],
    tips=["Voces limpias: <b>PLATE</b>, DECAY 1,5–2,5 s, PRE-DELAY 1/16 en SYNC, DUCK 40 %.",
          "Ambientes enormes para intros: <b>CLOUD</b> o <b>VOID</b>, DECAY 15 s+, y FREEZE para sostener."],
))

# ------------------------------------------------------------------ NEBULA
PRODUCTS.append(dict(
    file="LT-NEBULA-Manual.pdf", title="LT NEBULA", subtitle="Efecto granular para melodías y sonidos",
    image="design/nebula/simple-dark.webp", image2="design/nebula/advanced-dark.webp",
    intro="LT NEBULA rompe tu sonido en miles de granos (cada estrella de la galaxia es un grano) y los reorganiza para crear "
          "texturas, pads, glitches y transformaciones. Tiene dos niveles: <b>SIMPLE</b>, con controles fáciles para resultados "
          "rápidos, y <b>ADVANCED</b>, con todo el motor abierto en una sola pantalla.",
    sections=[],
    params_title="Modo SIMPLE",
    params=[
        ("MOTION", "0 – 100 %", "Cuánto se mueve y flota el sonido."),
        ("TEXTURE", "0 – 100 %", "De liso a granulado y roto."),
        ("PITCH", "-1200 a +1200 cents", "Sube o baja el tono (±1 octava)."),
        ("SPACE", "0 – 100 %", "Reverb y delay: de seco a infinito."),
        ("DIRT", "0 – 100 %", "Suciedad: saturación, lo-fi, crush."),
        ("MIX", "0 – 100 % (50 %)", "Cuánto del efecto se escucha."),
        ("PITCH STEP", "OFF, +5, +7, +12, -12, CUSTOM", "El tono salta a esos semitonos al ritmo elegido."),
        ("EVERY", "1/16 – 4 BAR", "Cada cuánto cambia el PITCH STEP."),
        ("TAPE STOP", "OFF, 1 BAR, 2 BAR, 4 BAR", "Frenado de cinta automático cada cierto número de compases."),
        ("FX", "PARTICLES, GRAIN CLOUD, REVERSE, FREEZE, SHIMMER", "Activa con un clic: destellos, nube de granos, reversa, congelar, octava brillante."),
    ],
    extra_tables=[
        ("Modo ADVANCED — GRAIN ENGINE y GRAIN DELAY", [
            ("SIZE", "10 – 500 ms", "Tamaño de cada grano."),
            ("DENSITY", "1 – 80 granos/s", "Cuántos granos suenan por segundo."),
            ("POSITION", "0 – 100 %", "De qué parte del audio reciente se toman los granos."),
            ("SPRAY", "0 – 100 %", "Aleatoriedad de la posición."),
            ("GRAIN PITCH", "-24 a +24 st", "Afinación de cada grano."),
            ("STEREO", "0 – 100 %", "Reparto de los granos en el estéreo."),
            ("SHAPE", "0 – 100 %", "Forma del grano: suave o con ataque marcado."),
            ("REVERSE", "0 – 100 %", "Probabilidad de que un grano suene al revés."),
            ("FREEZE", "On / Off", "Congela el audio y sigue granulando lo capturado."),
            ("GRAIN DELAY", "TIME 10–2000 ms · FEEDBACK · HP · LP · DRY/WET", "Delay propio de los granos con filtros."),
        ]),
        ("Modo ADVANCED — PITCH STEP y TAPE STOP", [
            ("8 pasos", "-12 a +12 semitonos", "El valor de cada paso del secuenciador de tono."),
            ("RATE", "1/16 – 4 BAR", "Velocidad de los pasos."),
            ("GLIDE", "0 – 100 %", "Salto de golpe o deslizamiento entre notas."),
            ("CHANCE", "0 – 100 %", "Probabilidad de que cambie en cada paso."),
            ("STEP AMOUNT", "0 – 100 %", "Mezcla entre el sonido original y el que tiene pitch."),
            ("SCALE LOCK", "OFF, MAJOR, MINOR, PENTA + tónica", "Ajusta los saltos a una escala para que siempre suene musical."),
            ("TAPE STOP", "EVERY, POSITION, LENGTH (0,125–4 tiempos), CURVE, START, CHANCE", "Cuándo y cómo se frena la cinta, y el arranque (TAPE START)."),
            ("Disparo", "AUTO, MANUAL, MIDI", "Por compases, con el botón STOP, o con una nota MIDI."),
        ]),
        ("Modo ADVANCED — FX CHAIN y MODULATION", [
            ("FX CHAIN", "8 ranuras · 31 efectos", "Reverb, Shimmer, Delay, Ping-Pong, Reverse Delay, Chorus, Flanger, Phaser, Tremolo, Auto-Pan, Widener, "
             "Pitch Shifter, Freq Shifter, Harmonizer, Formant, Saturation, Distortion, Tape, Filter LP/HP/BP, Comb, EQ, Crush, Tape Wobble, "
             "Stutter, Spectral Freeze, Ring Mod, Vinyl, Particles, Grain Cloud. <b>Clic</b> = elegir/mover/quitar; <b>arrastrar izquierda-derecha</b> "
             "= cantidad; <b>arriba-abajo</b> = reordenar; <b>+</b> = añadir."),
            ("MOD 1", "8 pasos dibujables · RATE · SWING", "Secuencia de valores; RANDOM y CLEAR."),
            ("MOD 2", "SINE, TRIANGLE, SAW UP/DOWN, SQUARE, SMOOTH RND", "Onda continua; clic cambia la forma."),
            ("ENV / RND", "Automáticos", "ENV sigue el volumen de la entrada; RND es aleatorio (sample &amp; hold)."),
            ("Asignar", "Arrastrar", "Arrastra el chip de un modulador sobre cualquier knob. El anillo de color muestra la cantidad. "
             "<b>Clic derecho</b> en el knob = cambiar cantidad o quitar. Hasta 16 rutas."),
            ("INPUT / OUTPUT", "-24 a +24 dB", "Volumen de entrada y de salida."),
        ]),
        ("Presets y bancos", [
            ("Navegador", "Clic en el nombre del preset", "Búsqueda, filtros (AMBIENT, RHYTHMIC, GLITCH, DARK, LO-FI, VOCAL), favoritos (estrella)."),
            ("Bancos", "Con portada", "Crea bancos, ponles una imagen, impórtalos y expórtalos para compartir."),
            ("LT COSMOS", "50 presets de fábrica", "Stardust Keys, Nebula Swell, Event Horizon, Gravity Bass, Cosmo Arp, Quantum Pad, Deep Void…"),
        ]),
    ],
    tips=["Empieza siempre en <b>SIMPLE</b> con un preset de LT COSMOS y mueve los macros; pasa a ADVANCED solo para afinar.",
          "Para un \"pitch\" tipo type beat: PITCH STEP <b>+12</b>, EVERY <b>1/2</b>, con SHIMMER activado."],
))

# ------------------------------------------------------------------ TUNE
PRODUCTS.append(dict(
    file="LT-TUNE-Manual.pdf", title="LT TUNE", subtitle="Afinador vocal en tiempo real",
    image="design/tune/main-dark-final.webp", image2="design/tune/match-dark.webp",
    image2_caption="Ventana MATCH REFERENCE (imagen de referencia del diseño)",
    intro="LT TUNE corrige la afinación de la voz en tiempo real: lleva cada nota a la nota más cercana de la escala elegida. "
          "Va de una corrección invisible (NATURAL) al efecto robótico (ROBOT). Mantiene el timbre de la voz al cambiar el tono "
          "(<b>preservación de formantes</b>, sin efecto \"ardilla\") y tiene un modo de <b>baja latencia</b> para grabar con el efecto puesto. "
          "Con <b>MATCH REFERENCE</b> cargas una canción y LT TUNE deduce cómo está afinada su voz para copiarlo.",
    sections=[("Cómo empezar", [
        "<b>1.</b> Pon LT TUNE en la pista de voz (mono o estéreo).",
        "<b>2.</b> Elige tu <b>VOICE</b> (el rango de tu voz) y la <b>KEY &amp; SCALE</b> de la canción (puedes sacarla con LT TYPE BEATS).",
        "<b>3.</b> Canta: la nota grande muestra la nota a la que te afina y el medidor cuánto te desvías (en cents). "
        "Brilla cuando estás afinado y tiembla cuando te sales.",
        "<b>4.</b> Ajusta <b>RETUNE SPEED</b>: hacia ROBOT (0–10 ms) efecto duro; hacia NATURAL (60–200 ms) corrección suave.",
    ]), ("MATCH REFERENCE", [
        "Pulsa <b>MATCH REF</b> y suelta una canción (WAV, AIFF, FLAC, MP3) o haz clic para buscarla. Con un <b>stem de voz (acapella)</b> "
        "el resultado es mucho más exacto; con la canción completa se usa la voz del centro de la mezcla.",
        "LT TUNE detecta KEY &amp; SCALE, RETUNE SPEED (HARD/SOFT), HUMANIZE, FLEX TUNE, VIBRATO (Kept/Flattened) y FORMANT. "
        "Cada uno tiene su interruptor: apaga los que no quieras copiar.",
        "<b>MATCH AMOUNT</b> decide cuánto se copia (100 % = igual que la referencia, 50 % = a medio camino entre tu ajuste y el detectado). "
        "<b>APPLY TO LT TUNE</b> aplica y todo queda editable (y se puede deshacer con el botón DESHACER).",
        "Es una estimación a partir de la curva de afinación de la voz: úsala como punto de partida y ajusta a oído.",
    ])],
    params=[
        ("VOICE", "Soprano, Alto, Tenor, Baritone, Bass", "Rango en el que se busca la nota. Elige el de tu voz: un rango más agudo detecta mejor y tiene menos latencia."),
        ("KEY &amp; SCALE", "C…B · Chromatic, Major, Minor, Harmonic Minor, Pentatonic Major/Minor, Blues, Custom",
         "Las notas permitidas. Chromatic = las 12 notas. Al cambiarla, EDIT NOTES vuelve a la escala."),
        ("EDIT NOTES", "12 orbes", "Encendidas = notas a las que se puede afinar. Clic = activar/desactivar (el nombre de la escala muestra *). "
         "La nota que cantas destella. La flecha circular = volver a la escala."),
        ("RETUNE SPEED", "0 – 400 ms (12 ms)", "Velocidad de la corrección. Arrastra hacia arriba/derecha para ROBOT. Destella en cada nota corregida. Doble clic = valor por defecto."),
        ("HUMANIZE", "0 – 100 (20)", "Las notas largas se corrigen más despacio para que suenen naturales; las cortas siguen rápidas."),
        ("FLEX TUNE", "0 – 100 (15)", "Solo corrige cuando estás cerca de la nota; los adornos y deslizamientos grandes quedan naturales. 0 = corrige siempre."),
        ("VIBRATO", "-100 a +100 (0)", "Negativo aplana tu vibrato natural; positivo añade un vibrato de 5,5 Hz."),
        ("FORMANT", "-12 a +12 st (0) + PRESERVE", "Cambia el timbre (más grave/oscuro o más agudo/infantil) sin cambiar la nota. "
         "<b>PRESERVE</b> encendido mantiene el timbre al corregir o transponer; apagado, el timbre sube y baja con el tono."),
        ("TRANSPOSE", "-12 a +12 st (0)", "Sube o baja toda la voz en semitonos (después de afinar)."),
        ("MIX", "0 – 100 % (100 %)", "Mezcla entre la voz original y la afinada."),
        ("DETUNE", "430 – 450 Hz (440)", "Frecuencia de referencia del La (A4). Úsala si el beat está afinado a 432 Hz u otra referencia."),
        ("MODE", "LOW LATENCY | HQ", "LOW LATENCY para grabar con el efecto puesto. HQ analiza con una ventana mayor: más estable en notas graves, algo más de latencia."),
        ("IN / OUT", "Medidores", "Nivel de entrada y salida."),
        ("Botones redondos", "Abajo a la derecha", "Deshacer/rehacer, ajustes (latencia, licencia, carpeta de presets, reset) y bypass."),
        ("Presets / SAVE / THEME", "Arriba", "16 presets de fábrica (Pop Vocal, Hard Tune, Trap Robot, R&amp;B Smooth, Deep Voice, Chipmunk…), "
         "flechas ‹ › para recorrerlos, SAVE para los tuyos, THEME para modo claro/oscuro. Los presets no cambian VOICE ni MODE."),
    ],
    extra_tables=[("Latencia (a 48 kHz)", [
        ("Soprano", "LOW LATENCY ≈ 14 ms", "HQ ≈ 20 ms"),
        ("Alto", "LOW LATENCY ≈ 18 ms", "HQ ≈ 26 ms"),
        ("Tenor", "LOW LATENCY ≈ 23 ms", "HQ ≈ 33 ms"),
        ("Baritone", "LOW LATENCY ≈ 28 ms", "HQ ≈ 40 ms"),
        ("Bass", "LOW LATENCY ≈ 36 ms", "HQ ≈ 53 ms"),
    ])],
    tips=["Efecto trap/robot: <b>Trap Robot</b> (RETUNE 0, HUMANIZE 0, VIBRATO -100) y la escala exacta del beat. Si suena \"a notas equivocadas\", revisa la KEY.",
          "Corrección invisible para baladas: RETUNE 80–150 ms, HUMANIZE 60, FLEX TUNE 40–60.",
          "Ableton compensa la latencia al reproducir. Para grabar con el efecto, usa LOW LATENCY y una VOICE lo más aguda que te funcione.",
          "Al cantar una nota que no está en la escala, LT TUNE la lleva a la nota permitida más cercana. Apaga notas en EDIT NOTES para forzar una melodía."],
))

# ------------------------------------------------------------------ NOVA
PRODUCTS.append(dict(
    file="LT-NOVA-Manual.pdf", title="LT NOVA", subtitle="Saturación analógica",
    image="design/nova/main-dark.webp",
    intro="LT NOVA es una saturación de estilo analógico con 8 tipos de \"hardware\": cinta, válvula, consola, transformador, transistor, "
          "diodo, vinilo y excitador. Cada tipo satura como su equipo real: la cinta comprime los agudos, el transformador satura primero los graves, "
          "la válvula empuja los medios y \"respira\" con la dinámica. Trabaja con <b>sobremuestreo 4x</b> (sin aliasing) y <b>AUTO GAIN</b> "
          "para que compares a igual volumen. La estrella crece y brilla con el DRIVE.",
    sections=[("Cómo empezar", [
        "<b>1.</b> Pon LT NOVA en una pista o bus (voz, 808, batería, master...).",
        "<b>2.</b> Elige un tipo en la barra (TAPE, TUBE, CONSOLE...) o un preset de fábrica.",
        "<b>3.</b> Sube el <b>DRIVE</b> (la estrella) hasta que oigas el color. Con AUTO GAIN encendido el volumen se mantiene.",
        "<b>4.</b> Usa <b>MIX</b> para saturación en paralelo: el original se mezcla con el saturado sin desfases.",
        "<b>EQ FOCUS</b>: aplica la saturación solo a una banda de frecuencias (por ejemplo solo graves del 808 o solo agudos de la voz).",
    ])],
    params=[
        ("TYPE", "TAPE, TUBE, CONSOLE, TRANSFORMER, TRANSISTOR, DIODE, VINYL, EXCITER", "El carácter de la saturación."),
        ("DRIVE", "0 – 100 %", "Cantidad de saturación (la estrella). Doble clic = valor por defecto."),
        ("TONE", "-100 a +100", "Inclina el sonido saturado hacia oscuro (izquierda) o brillante (derecha)."),
        ("WARMTH", "0 – 100 %", "Más cuerpo y calidez en los graves-medios."),
        ("AIR", "0 – 100 %", "Brillo y aire arriba de 10 kHz."),
        ("CHARACTER", "EVEN · BALANCED · ODD", "Armónicos pares (cálido, suave) o impares (más duro y agresivo)."),
        ("INPUT / OUTPUT", "-18 a +18 dB", "Nivel de entrada (empuja más la saturación) y de salida."),
        ("MIX", "0 – 100 %", "Mezcla en paralelo con el original (alineado, sin cancelaciones)."),
        ("AUTO GAIN", "ON / OFF", "Iguala el volumen del saturado al del original."),
        ("Presets", "20 de fábrica", "Warm Glue, Master Sheen, Expensive Console, Tape Bus, Vocal Warmth, Rap Vocal Grit, Drum Bus Smash, 808 Heat, "
         "808 Distorted, Lo-Fi Record, Old Cassette, Hi-Hat Shine, Guitar Amp Crunch, Supernova..."),
    ],
    tips=["En el master: TAPE o CONSOLE con DRIVE 10–25 % y MIX 100 %. Debe notarse más \"pegado\", no más distorsionado.",
          "808 que se oiga en altavoces pequeños: TRANSISTOR o DIODE, DRIVE 40–60 %, EQ FOCUS en graves.",
          "Voz con presencia: TUBE, DRIVE 20–35 %, MIX 50–70 %."],
))

# ------------------------------------------------------------------ VOCAL SUITE
PRODUCTS.append(dict(
    file="LT-VOCAL-SUITE-Manual.pdf", title="LT VOCAL SUITE", subtitle="Cadena vocal completa con asistente",
    image="design/vocal-suite/main-dark.webp",
    intro="LT VOCAL SUITE es una cadena vocal completa en un solo plugin: afinación (motor de LT TUNE), limpieza (low cut, puerta de ruido, "
          "de-esser), ecualizador vocal, compresor, saturación (motor de LT NOVA), doblador, efecto teléfono, anchura estéreo, delay (LT ORBIT) "
          "y reverb (LT VOID), con un limitador de seguridad a la salida. Le escribes al <b>astronauta</b> lo que quieres "
          "(\"vocal chain estilo trap melódico\", \"más brillo\", \"menos reverb\") y él ajusta los controles por ti.",
    sections=[("Cómo empezar", [
        "<b>1.</b> Pon LT VOCAL SUITE en la pista de voz.",
        "<b>2.</b> En <b>TUNE</b> elige la <b>KEY</b> de la canción y tu tipo de voz (Soprano...Bass).",
        "<b>3.</b> Elige un preset de estilo (Melodic Trap Lead, Drill Vocal, R&amp;B Silk, Reggaeton Lead...) o escríbele al asistente y pulsa <b>Send</b>.",
        "<b>4.</b> Ajusta a oído en cada módulo. <b>UNDO / REDO</b> deshacen también lo que hizo el asistente. <b>BYPASS</b> compara con la voz original.",
    ]), ("El asistente", [
        "Sin configurar nada funciona el <b>asistente integrado</b> (sin internet): entiende pedidos comunes en español e inglés y conoce todos los estilos.",
        "Para que entienda <b>cualquier pedido</b> (incluido \"estilo de tal artista\"), conecta Claude: clic en el astronauta &gt; <b>Connect Claude</b> "
        "y pega una clave de API de console.anthropic.com. La clave se guarda solo en tu computadora (nunca en el proyecto ni en los presets). "
        "Cada pedido cuesta muy poco.",
        "El asistente recrea el <b>procesado</b> (afinación, EQ, compresión, efectos), no la voz de nadie.",
        "Micrófono: en Mac, con el cuadro de texto seleccionado, pulsa dos veces la tecla <b>Fn</b> (dictado) y habla.",
    ])],
    params=[
        ("TUNE", "Key, Voice, Retune Speed, Humanize, Flex Tune, Mix", "Afinación en tiempo real. Retune 0 = efecto robot; 40–80 ms = natural."),
        ("CLEANUP", "Low Cut, Gate, De-Ess, De-Ess Freq", "Quita golpes de graves, ruido entre frases (GATE OPEN se enciende con la voz) y eses fuertes."),
        ("TONE", "Body, Mud Cut, Presence, Air", "EQ vocal: cuerpo (180 Hz), barro (350 Hz), presencia (3,2 kHz) y aire (12 kHz). La curva muestra el resultado."),
        ("DYNAMICS", "Compression, Attack, Release · Saturation type, drive, mix", "Compresor que se adapta al nivel de tu grabación (siempre suena igual de "
         "fuerte gracias a la ganancia automática) + saturación de LT NOVA en paralelo."),
        ("SPACE", "Delay (nota, modo, feedback, mix) · Reverb (tipo, decay, pre-delay, mix)", "Delays sincronizados al tempo y 12 reverbs. "
         "Se apartan solos cuando cantas (ducking) para que la voz quede clara."),
        ("FX", "Doubler, Phone, Width, Output", "Doble estéreo, efecto teléfono/radio, anchura y volumen de salida."),
        ("INPUT / OUTPUT", "Medidores laterales", "Nivel de entrada y salida. La salida nunca pasa de -0,5 dBFS (limitador de seguridad)."),
        ("Presets", "18 estilos", "Melodic Trap Lead, Hard Tune Robot, Rage Vocal, Drill Vocal, Boom Bap Rap, Modern Rap Upfront, R&amp;B Silk, "
         "Pop Lead Polished, Ballad Natural, Reggaeton Lead, Afro Smooth, Rock Grit, Lo-Fi Bedroom, Phone Intro, Ad-Lib Wide, Ethereal Shimmer, Dry &amp; Clean."),
    ],
    extra_tables=[("Latencia", [
        ("Total", "≈ 26 ms (voz Tenor, 44,1 kHz)", "Ableton la compensa al reproducir. Para grabar con el efecto, elige una VOICE más aguda o graba sin el plugin."),
    ])],
    tips=["Empieza con el preset de tu género y pide cambios pequeños al asistente: \"más aire\", \"autotune más natural\", \"menos delay\".",
          "Para ad-libs: preset <b>Ad-Lib Wide</b> (doble y ancho) en una pista aparte.",
          "Si la afinación va a notas raras, revisa la KEY (puedes sacarla con LT TYPE BEATS)."],
))

# ------------------------------------------------------------------ QUASAR
PRODUCTS.append(dict(
    file="LT-QUASAR-Manual.pdf", title="LT QUASAR", subtitle="Sintetizador wavetable / analógico / sampler",
    image="design/quasar/main-dark.webp", image2="design/quasar/fx-dark.webp", image2_caption="Página FX (imagen de referencia del diseño)",
    intro="LT QUASAR es un sintetizador para producir y para diseñar sonido: 3 osciladores (cada uno <b>WAVETABLE</b>, <b>ANALOG</b> o <b>SAMPLE</b>), "
          "SUB, NOISE, 2 filtros analógicos, 4 envolventes, 4 LFOs dibujables, una matriz de modulación de 32 rutas, mezclador con buses "
          "y 8 efectos por bus. Trae <b>239 presets</b> en 8 bancos pensados para trap, reggaetón, R&amp;B y lo-fi. Todo se puede modular "
          "arrastrando una fuente (ENV, LFO, MACRO) sobre cualquier perilla, como en Serum.",
    sections=[("Cómo empezar", [
        "<b>1.</b> Crea una pista MIDI en Ableton y carga LT QUASAR (Instrumentos &gt; Plug-ins &gt; LT Audio).",
        "<b>2.</b> Pulsa <b>BROWSER</b> (o el nombre del preset) y elige un banco o una categoría (BASS, 808, PLUCK, KEYS, PAD...). "
        "Busca por nombre, filtra por estilo (TRAP, REGGAETON, R&amp;B, LO-FI...) y marca favoritos con la estrella.",
        "<b>3.</b> Ajusta el sonido con las 8 <b>MACROS</b> (BRIGHTNESS, TIMBRE, TIME, MOVEMENT, DRIVE, WIDTH, DELAY, SPACE). "
        "En la vista <b>SIMPLE</b> solo ves el preset y las macros, estilo Analog Lab.",
        "<b>4.</b> Guarda con el icono de disco: nombre, categoría, estilo y banco.",
    ]), ("Modular (como Serum)", [
        "Arrastra la pestaña <b>ENV 1-3</b>, <b>PITCH ENV</b>, <b>LFO 1-4</b> o el nombre de una <b>MACRO</b> y suéltala sobre cualquier perilla. "
        "Aparece un anillo de color con la cantidad y un punto blanco que muestra el valor real en cada momento.",
        "<b>ALT + arrastrar</b> sobre la perilla cambia la cantidad. Clic derecho: invertir, bipolar, quitar o modular con otra fuente.",
        "La página <b>MATRIX</b> muestra las 32 rutas: fuente, cantidad, destino, curva, polaridad, fuente AUX (multiplica) y salida.",
        "LFO: arrastra los puntos, doble clic añade o borra un punto, ALT + arrastrar curva un tramo, clic derecho = formas (seno, S&amp;H, random...).",
    ]), ("Mezclador, buses y efectos", [
        "Cada fuente va a FILTER 1, FILTER 2, los dos, o directo a MAIN / BUS 1 / BUS 2 (página <b>MIX</b>, menú DEST). "
        "FILTER 1 puede ir a FILTER 2 (en serie) o a un bus.",
        "En <b>FX</b> eliges el bus (MAIN, BUS 1, BUS 2), pulsas <b>+ FX</b> y añades hasta 8 efectos: Saturation (motor de LT NOVA), Compressor, "
        "Chorus, Flanger, Phaser, Delay (motor de LT ORBIT), Reverb, Equalizer, Filter, Distortion, Bitcrush, Tape, Stereo Width y Utility. "
        "Arrastra una fila para cambiar el orden. Los buses 1 y 2 terminan en MAIN.",
        "La reverb de QUASAR es nueva: se mantiene limpia aunque pongas MIX al 100 % (sin efecto de feedback).",
        "SAMPLE: arrastra un .wav al visor del oscilador. La nota base se detecta del nombre (\"Pluck_C.wav\" = C). "
        "WAVETABLE: arrastra un .wav (ciclos de 2048 muestras, formato Serum) para importar tu propia wavetable.",
    ]), ("Bancos para compartir", [
        "Un banco es una carpeta de presets. <b>NEW BANK</b> crea uno; al guardar eliges en qué banco va el preset.",
        "<b>EXPORT BANK</b> crea un archivo <b>.ltqbank</b> (presets + samples + portada) para mandarlo por WhatsApp o Drive. "
        "<b>IMPORT BANK</b> lo instala. <b>EXPORT</b> guarda un solo preset (.ltq).",
    ])],
    params=[
        ("OSC A / B / C", "WAVETABLE · ANALOG · SAMPLE", "36 wavetables de fábrica + USER. ANALOG = osciladores tipo analógico (seno → triángulo → sierra → cuadrada) con deriva. "
         "OCT, SEM, FIN, PHS (fase) y RND (fase aleatoria). WT POS, UNISON (hasta 16), DETUNE, WARP (modo en el menú: Sync, Bend, PWM, Mirror, Asym, FM, Ring, Fold, Quantize), BLEND, SPREAD, PAN, LEVEL."),
        ("SUB / NOISE", "OCT, LEVEL · COLOR, LEVEL", "Sub-oscilador (clic en el visor: seno, triángulo, sierra, cuadrada, pulso). Ruido: White, Pink, Brown, Vinyl, Tape Hiss, Air, Crackle, Digital."),
        ("FILTERS", "Tipo, CUTOFF, RES, DRIVE, KEY, MIX", "Ladder 24/12 (transistor), LP/HP/BP 24/12, Notch, Peak, Comb +/-, Formant (vocales) y Diode."),
        ("ENV 1 / 2 / 3 / PITCH ENV", "ATK, HOLD, DEC, SUS, REL", "ENV 1 es el volumen. Arrastra los puntos del dibujo. PITCH ENV tiene AMOUNT (semitonos): el golpe del 808."),
        ("LFO 1-4", "FREE / RETRIG / ENV, SYNC, RATE, RISE, DELAY, SMOOTH, PHASE", "Forma dibujable, sincronía al tempo de Ableton (8/1 a 1/64, tresillos y puntillos)."),
        ("VOICING", "POLY, MONO, LEGATO, GLIDE", "Polifonía hasta 32. Robo de voz suave (sin clics). Glide para 808 deslizados."),
        ("ARP", "Clic en ARP", "Modo, velocidad, patrón (Trap Roll, Triplet Flow, Dembow, Drill, Plugg...), octavas, gate, swing y latch."),
        ("GLOBAL", "Quality, Tuning, Voices, Analog, Output", "ECO/HIGH/ULTRA y oversampling 2x/4x, afinación, pitch bend, escala, unison de voces, voice steal, prioridad, MPE, DRIFT, WARMTH, limitador de seguridad."),
        ("MASTER", "Arriba a la derecha", "Volumen general. Los presets de fábrica están nivelados para sonar a un volumen parecido."),
    ],
    extra_tables=[("Bancos de fábrica", [
        ("Cómo se hicieron", "239 presets", "Cada preset se ajustó automáticamente a un one-shot de referencia distinto (espectro en el tiempo, envolvente y anchura estéreo) y se quitaron los que sonaban iguales."),
        ("QUASAR FACTORY", "Lo mejor", "Los ajustes más fieles de cada categoría + sonidos insignia (808 con glide, reese, arps)."),
        ("TRAP ESSENTIALS · PERREO · R&B NIGHTS", "Por género", "Plucks, leads, keys y bells de trap, reggaetón y R&amp;B."),
        ("LO-FI DUST · 808 GALAXY", "", "Versiones con cinta y vinilo / bajos, 808, reese y subs."),
        ("ARP MACHINE · SPACE PADS", "", "Cada arp es un sonido distinto con su patrón / pads, texturas y FX."),
    ])],
    tips=["808 con slide: pon VOICING en LEGATO con GLIDE 80-150 ms y toca notas que se solapen.",
          "Pluck de reggaetón: preset de PERREO, MACRO 7 (DELAY) a tu gusto y el delay en 1/8 o 1/8D.",
          "CPU: en GLOBAL baja a ECO si usas muchas instancias. Con 16 voces muy cargadas QUASAR usa ~35 % de un núcleo del M1.",
          "Para comparar con el sonido original de un preset, la estrella * junto al nombre indica que lo has cambiado."],
))

# ------------------------------------------------------------------ LICENSE STUDIO
PRODUCTS.append(dict(
    file="LT-License-Studio-Manual.pdf", title="LT LICENSE STUDIO", subtitle="Generador de licencias (solo para tu Mac)",
    image="design/licensing/license-studio.webp", no_install=True,
    intro="LT LICENSE STUDIO crea las licencias de los plugins LT. Cada licencia queda <b>firmada digitalmente</b> con tu clave "
          "secreta, que solo existe en tu Mac: nadie puede falsificarlas. Cada licencia queda atada a la computadora de quien la recibe.",
    sections=[("Cómo generar una licencia", [
        "<b>1.</b> Tu amigo instala el plugin. En la pantalla de activación pulsa <b>COPY</b> y te envía su <b>Machine ID</b>.",
        "<b>2.</b> En LT License Studio elige el plugin a la izquierda (o <b>LT ALL PLUGINS</b> para todos).",
        "<b>3.</b> Escribe el nombre, el <b>email (obligatorio)</b> y pega el Machine ID (botón PASTE).",
        "<b>4.</b> Pulsa <b>GENERATE LICENSE</b>. Debe aparecer <b>Signed &amp; verified</b>.",
        "<b>5.</b> <b>COPY</b> para enviarla como texto o <b>SAVE .lic FILE</b> para enviar el archivo. Tu amigo la pega o arrastra el archivo y pulsa ACTIVATE.",
    ])],
    params=[
        ("Lista de plugins", "Izquierda", "Elige para qué plugin es la licencia. LT ALL PLUGINS activa todos."),
        ("Computers used", "x de 2", "Cuántas computadoras tiene ya esa persona (según su email). Si intenta una tercera, la app avisa y pregunta."),
        ("RECENT LICENSES", "Derecha", "Todas tus licencias con búsqueda. Clic = volver a verla, copiarla o guardarla. REMOVE = borrarla de tu lista "
         "(la licencia que ya enviaste sigue funcionando)."),
        ("CUSTOMERS", "Abajo a la izquierda", "Tus amigos agrupados por email, con sus plugins y computadoras."),
        ("SETTINGS", "Abajo a la izquierda", "Tu clave pública (COPY PUBLIC KEY), su estado, <b>EXPORT KEY BACKUP</b>, IMPORT KEY BACKUP y OPEN DATA FOLDER."),
    ],
    tips=["<b>Haz una copia de seguridad</b> con EXPORT KEY BACKUP y guárdala en un lugar privado. Si pierdes la clave no podrás crear licencias nuevas.",
          "Nunca compartas el archivo de backup ni <i>signing-key.json</i>. La clave <b>pública</b> sí se puede compartir."],
))


def build(prod):
    path = os.path.join(OUT, prod["file"])
    doc = BaseDocTemplate(path, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=18 * mm, bottomMargin=18 * mm,
                          title=prod["title"] + " — Manual", author="LT Audio")
    doc.lt_title = prod["title"]
    frame = Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f")
    doc.addPageTemplates([PageTemplate(id="p", frames=[frame], onPage=page_bg)])

    st = []
    st.append(Spacer(1, 40 * mm))
    st.append(Paragraph("<font name='M-Bold' color='#7b2cff'>LT</font> " + prod["title"][3:], S["cover"]))
    st.append(Spacer(1, 4 * mm))
    st.append(Paragraph(prod["subtitle"], S["coversub"]))
    st.append(Paragraph("MANUAL DE USUARIO", S["coversub"]))
    st.append(Spacer(1, 12 * mm))
    if (im := img(prod["image"], doc.width)) is not None:
        st.append(im)
        st.append(Paragraph("Interfaz (imagen de referencia del diseño)", S["cap"]))
    st.append(PageBreak())

    st.append(Paragraph("Qué es", S["h2"]))
    st.append(Paragraph(prod["intro"], S["p"]))
    for title, lines in prod.get("sections", []):
        st.append(Paragraph(title, S["h2"]))
        for l in lines:
            st.append(Paragraph(l, S["p"]))

    st.append(Paragraph(prod.get("params_title", "Controles y parámetros"), S["h2"]))
    widths = [38 * mm, 42 * mm, 94 * mm]
    st.append(table(prod["params"], widths))
    for title, rows in prod.get("extra_tables", []):
        st.append(Paragraph(title, S["h2"]))
        st.append(table(rows, widths))
    if prod.get("image2") and (im2 := img(prod["image2"], doc.width)) is not None:
        st.append(Spacer(1, 6 * mm)); st.append(im2)
        st.append(Paragraph(prod.get("image2_caption", "Modo ADVANCED (imagen de referencia del diseño)"), S["cap"]))

    st.append(Paragraph("Consejos", S["h2"]))
    for t in prod["tips"]:
        st.append(tipbox(t)); st.append(Spacer(1, 3 * mm))

    if not prod.get("no_install"):
        for kind, text in COMMON_INSTALL:
            st.append(Paragraph(text, S[kind]))

    doc.build(st)
    print("built", path)


if __name__ == "__main__":
    for p in PRODUCTS:
        build(p)
