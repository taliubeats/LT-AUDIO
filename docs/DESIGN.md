# LT AUDIO — Documento de diseño

Fuente única de verdad de todo lo decidido para la familia de plugins **LT**.
Las imágenes de referencia están en `design/`. Son referencia de **estilo y distribución**;
los textos, tipografías y detalles se implementan limpios en código (vector).

## Reglas generales

- **Formatos:** VST3 + AU (Mac, Universal arm64/x86_64) y VST3 (Windows). Host principal: Ableton Live 12, Mac M1.
- **Framework:** C++ / JUCE. Compilación automática con GitHub Actions (Mac y Windows).
- **Uso:** personal y para amigos (no comercial). Sin firma de Apple Developer: se documenta cómo desbloquear en macOS/Windows.
- **Estética de familia:** espacio profundo, nebulosas moradas, estrellas, partículas, cristal casi invisible, tipografía blanca fina.
- **Temas:** botón **THEME** con modo oscuro (negro + morado `#7B2CFF` + blanco) y modo claro
  (celeste `#4FC3F7` de fondo, amarillo `#FFD60A` para textos grandes/valores, blanco para acentos,
  negro `#111111` para textos pequeños). En modo claro usar amarillo más intenso (`#FFC300`) + sombra sutil donde falte contraste.
  Excepción: **LT VOID** no tiene THEME; el color depende del tipo de reverb.
- **Animaciones:** solo visuales (nunca alteran el audio). Nebulosas siempre en movimiento, partículas que reaccionan al audio y a los knobs.
- **Logos:** `design/brand/logos.webp`. "LT" en morado bold + nombre en blanco. Íconos: `design/brand/icons.webp`.
- **Licencias:** ver sección Licencias. Común a todos los plugins.
- **Ventana EQ "Frequency Focus"** (PULSAR y KNOCK): espectro 20 Hz–20 kHz en tiempo real, LOW CUT / HIGH CUT arrastrables,
  botones FULL / LOW / MID / HIGH, INVERT, SOLO. El efecto se aplica solo a la banda elegida; el resto pasa limpio.
  La banda seleccionada brilla (nebulosa + partículas), lo de fuera queda tenue.

## Orden de trabajo

1. Base compartida (`shared/`): licencias, temas, visuales (nebulosa, partículas), presets, Frequency Focus.
2. LT LICENSE STUDIO (app Mac).
3. LT TYPE BEATS · 4. LT PULSAR · 5. LT KNOCK · 6. LT ORBIT · 7. LT VOID · 8. LT NEBULA.

**Para más adelante:** Saturación (idea: knob DRIVE como estrella/sol; nombres LTSOLAR/LTNOVA/LTFLARE),
Armonías vocales, alternativa a Thermal, Vintage/Mellotron (emulación de cinta).

---

## 1. LT TYPE BEATS — analizador
Diseño: `design/type-beats/main-dark.webp`, `main-light.webp`.

- Barra **Paste YouTube link + GET** (arriba): descarga con yt-dlp (incluido), guarda WAV en carpeta configurable
  (por defecto `Música/LT Type Beats`), carga y analiza automáticamente. Botón en Settings para actualizar yt-dlp.
- Zona de onda (polvo de estrellas): arrastrar/soltar o BROWSE. El WAV se puede arrastrar a una pista de audio.
- **BPM** (número que late como púlsar) + **TAP** manual.
- **KEY** con % de confianza; tonalidad como planeta y **alternativas como lunas** (clic = pasa a principal).
- **Piano de cristal** (2 octavas) con notas detectadas iluminadas. (Corregir teclas negras: teclado real.)
- **Acordes como constelación** en línea de tiempo; clic en un acorde → popup con alternativas; elegir una la reemplaza.
- **EXPORT MIDI** (guardar archivo) + **DRAG MIDI** (arrastrar directo a una pista MIDI de Ableton). Exporta con las correcciones.
- Sin separación de stems.
- Corregir de las imágenes: sin duplicados de nombre de archivo ni de TAP.

## 2. LT PULSAR — sidechain / ducking (antes LTDUCK)
Diseño: `design/pulsar/*`. **Solo cambio de nombre** respecto al diseño LTDUCK (no estética espacial).

- Ducking por volumen sincronizado al tempo del host (1/1, 1/2, 1/4, 1/8, 1/16) estilo "volume shaper".
- Knob grande **MIX** con luz detrás y partículas que crecen con el valor y laten al ritmo.
- Visualizador con la **onda de audio real** pasando (original tenue detrás, procesada delante) + botón **WAVE** on/off.
- **Curva editable**: arrastrar puntos y desplazar (SHIFT) izquierda/derecha. 8 formas predefinidas.
- **MIDI TRIGGER**: el ducking se dispara con notas MIDI.
- **Presets** de fábrica: Classic Pump 1/4, Soft Duck 1/4, Hard Cut 1/4, Short Duck, Long Release, Double Pump 1/8,
  Offbeat 1/8, Gate 1/16, Half Time 1/2, Subtle Glue. **SAVE** para presets de usuario.
- Botón **EQ** → ventana Frequency Focus.

## 3. LT KNOCK — procesador de drums y melodías
Diseño: `design/knock/*`. Logo con la "O" como órbita.

- **TONE:** PUNCH (transient), SATURATION, DISTORTION (SOFT/HARD/FUZZ/TUBE), BRIGHT, LOW, GLUE (compresor), WIDTH.
- **FX:** CRUSH (bitcrusher), LO-FI (vinilo/cassette), STUTTER (glitch sincronizado), FILTER (barrido), RING (ring mod), REVERSE (reverb invertida).
- **SPACE:** REVERB, DELAY (+ selector de tiempo sincronizado).
- **KNOCK** (planeta central) = **mix general** dry/wet (0% original, 100% todo procesado).
  Animación: esfera vacía (0%) → nebulosa (25%) → planeta (50%) → planeta con anillo (75%) → encendido y latiendo (100%).
- **CHAOS**: aleatoriza knobs en rangos musicales, con deshacer y candado por sección (TONE/FX/SPACE). Ráfaga de partículas.
- **INPUT** y **OUTPUT** + medidores IN/OUT. Modo **DRUMS | MELODY**. Presets + SAVE. Botón **EQ**.
- Un solo panel de cristal casi invisible, sin cajas.

## 4. LT NEBULA — granular (alternativa simple a Portal)
Diseño: `design/nebula/*`.

- **SIMPLE:** galaxia central animada (cada grano = estrella). Selectores fáciles:
  PITCH STEP (OFF/+5/+7/+12/-12) y EVERY (1/4, 1/2, 1 BAR, 4 BAR); TAPE STOP (OFF/1/2/4 BAR);
  FX on/off (PARTICLES, GRAIN CLOUD, REVERSE, FREEZE, SHIMMER). Macros: MOTION, TEXTURE, PITCH, SPACE, DIRT, MIX.
- **ADVANCED (un solo panel):** GRAIN ENGINE (SIZE, DENSITY, POSITION, SPRAY, PITCH, STEREO, SHAPE, REVERSE, FREEZE),
  GRAIN DELAY (TIME, FEEDBACK, HP, LP, DRY/WET), PITCH STEP (8 pasos, GLIDE, CHANCE, RATE 1/16–4 bars, ORDER, SCALE LOCK, AMOUNT),
  TAPE STOP (EVERY, POSITION, LENGTH, CURVE, TAPE START, CHANCE, AUTO/MANUAL/MIDI), MODULATION (MOD 1 step, MOD 2 curva;
  RATE, SWING, SYNC, RANDOM, CLEAR; 4 LFO, envelope follower, random, asignables arrastrando; definen los macros),
  FX CHAIN (8 slots reordenables). Añadir barra superior (preset/SAVE/THEME) y knobs LENGTH/CURVE que faltan en la imagen.
- **Efectos (24+):** Reverb, Shimmer, Delay, Ping-Pong, Reverse Delay, Chorus, Flanger, Phaser, Tremolo, Auto-Pan, Widener,
  Pitch Shifter, Frequency Shifter, Harmonizer, Formant Filter, Saturación, Distorsión, Tape, Filtro LP/HP/BP, Comb, EQ,
  Bitcrusher, Tape Wobble, Stutter/Glitch, Spectral Freeze, Ring Mod, Vinyl/Lo-Fi, **Tape Stop**, **Particles**, **Grain Cloud**.
- **Presets y bancos:** bancos con imagen de portada, crear/guardar/importar/exportar, favoritos, búsqueda, etiquetas.
  Banco de fábrica **LT COSMOS** (~50 presets: Stardust Keys, Solaris Lead, Nebula Swell, Event Horizon, Retro Future,
  Gravity Bass, Cosmo Arp, Quantum Pad, Pulsar Keys, Deep Void, Starry Night, ...).
- Animación: la galaxia gira siempre; reacciona a audio, MOTION (velocidad), TEXTURE (densidad), SPACE (expansión),
  DIRT (chispeo), PITCH STEP (destello), TAPE STOP (se frena), FREEZE (congelada).

## 5. LT ORBIT — delay
Diseño: `design/orbit/*`. Logo con la "O" como planeta con anillo.

- Modos: DIGITAL, TAPE, ANALOG, PING-PONG, REVERSE, SHIMMER, LO-FI, DUB.
- TIME (sync 1/4… con NORMAL/DOTTED/TRIPLET, o MS), FEEDBACK, MIX, FREEZE ∞.
- LOW CUT, HIGH CUT, WIDTH, DUCK, WOBBLE, DRIVE. Medidores IN/OUT (una sola etiqueta). Presets + SAVE.
- Visual: planeta = sonido original; cada eco = satélite orbitando en anillos cada vez más lejanos/tenues.
  FEEDBACK = más anillos, TIME = velocidad, PING-PONG = rebote lateral, REVERSE = hacia dentro, FREEZE = órbitas infinitas.

## 6. LT VOID — reverb de alta gama
Diseño: `design/void/main-cloud.webp`, `all-types.webp`. Referencias de calidad: reverbs vintage/algorítmicas de gama alta.

- **REVERB TYPE** (motor; cada uno cambia la paleta de color con transición suave):
  CLOUD (por defecto, negro/morado), PLATE (celeste/amarillo), HALL (azul marino/dorado), ROOM (ámbar), CHAMBER (esmeralda),
  VINTAGE (naranja/rojo), SPRING (cobre rojizo metálico), SHIMMER (rosa/perla), REVERSE (carmesí), GATED (cian neón),
  LO-FI (sepia polvoriento — distinto de SPRING), VOID (negro/blanco monocromo).
- **PRESETS** por tipo + SAVE de usuario.
- Controles: DECAY (agujero negro), MIX, PRE-DELAY (sync), SIZE, DAMPING (HIGH FREQ, LOW FREQ), SHAPE (ATTACK, DIFFUSION),
  MOD (RATE, DEPTH), EQ (LOW CUT, HIGH CUT), WIDTH, DUCK, FREEZE ∞. Medidores INPUT/OUTPUT.
  Línea de texto inferior que describe el tipo elegido.
- Animación: nebulosa girando en espiral hacia el agujero negro; destellos con cada golpe que se apagan como la cola;
  DECAY/SIZE/MOD/DAMPING modifican la animación; FREEZE la suspende girando.

## Licencias
Diseño: `design/licensing/activation.webp`, `license-studio.webp`.

- **Tipo B:** licencia atada a la computadora (Machine ID). Firma digital Ed25519: clave privada solo en la Mac del autor,
  clave pública embebida en los plugins. Sin servidor ni internet.
- **Prueba:** 10 días. **2 computadoras** por persona (LICENSE STUDIO lleva la cuenta y avisa).
- **Pantalla de activación** (común, cambia logo/nombre por plugin): Paso 1 Machine ID + COPY; Paso 2 pegar licencia o
  arrastrar archivo `.lic` + ACTIVATE; CONTINUE TRIAL con días restantes y barra.
- **LT LICENSE STUDIO** (app Mac): lista de plugins; NEW LICENSE con Nombre, **Email (obligatorio)**, Machine ID,
  "Computers used: x of 2", GENERATE LICENSE, resultado con COPY y SAVE .lic FILE, "Signed & verified";
  RECENT LICENSES con búsqueda; CUSTOMERS; SETTINGS (clave de firma + copia de seguridad).
