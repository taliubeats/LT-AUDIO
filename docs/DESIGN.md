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
- **Presets** de fábrica: Smooth S 1/4 (curva de referencia del usuario: abajo, subida en S, arriba), Classic Pump 1/4, Soft Duck 1/4, Hard Cut 1/4, Short Duck, Long Release, Double Pump 1/8,
  Offbeat 1/8, Gate 1/16, Half Time 1/2, Subtle Glue. **SAVE** para presets de usuario.
- Botón **EQ** → ventana Frequency Focus.

**Estado: hecho** (`plugins/pulsar`, Frequency Focus en `shared/lt_dsp` para reutilizar en KNOCK).
- MIX 0 % = sin efecto, 100 % = la curva completa. Con el transporte parado sigue latiendo al BPM del host ("free run").
- Curva: arrastrar puntos; doble clic añade/borra; arrastrar una línea la curva; SHIFT + arrastrar la mueve en el tiempo;
  clic derecho: voltear, invertir, quitar desplazamiento, línea recta.
- Visualizador estilo osciloscopio (referencia del usuario): onda real como línea, curva blanca gruesa, pestañas 1/16…1/1 arriba a la izquierda.
- 8 formas: Pump, S-Fall, Linear, Fade, Ramp, Arc, Drop, Cut. "Offbeat 1/8" = curva Pump a 1/4 desplazada medio tiempo.
- MIDI TRIGGER: cada nota MIDI reinicia la curva; tras un ciclo se queda en el valor final.
- Frequency Focus: LOW 30–150 Hz, MID 150–2500 Hz, HIGH 2.5–20 kHz; arrastrar los bordes = CUSTOM; arrastrar el centro mueve la banda;
  doble clic = FULL. Crossovers Linkwitz-Riley (sin latencia, suma plana).
- Presets de usuario: `LT Audio/Presets/LT PULSAR/*.ltpreset` (JSON). Si la prueba de 10 días termina sin licencia, el audio pasa sin procesar.

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

**Estado: hecho** (`plugins/knock`). Cadena: INPUT → Frequency Focus → TONE → FX → SPACE → KNOCK (dry/wet) → OUTPUT, sin latencia.
PUNCH = transient shaper; GLUE = compresor de bus; STUTTER repite la última porción sincronizada (1/16 en DRUMS, 1/8 en MELODY);
FILTER bipolar (izquierda LP, derecha HP, doble clic = OFF); REVERSE = reverb invertida; DELAY con tiempo 1/16…1/2.
MELODY suaviza PUNCH, alarga GLUE y agranda la reverb. CHAOS respeta los candados de TONE/FX/SPACE y se puede deshacer (↶).
La ventana EQ usa el estilo cristal de `design/knock/eq-*.webp` (FrequencyFocusView::setGlassStyle). 13 presets de fábrica.

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

**Estado: fase 1 hecha** (`plugins/nebula`): motor granular (búfer de 8 s, hasta 96 granos), PITCH STEP (simple y 8 pasos con
GLIDE/CHANCE/SCALE LOCK), TAPE STOP (AUTO/MANUAL/MIDI, LENGTH/CURVE/START/CHANCE/POSITION), GRAIN DELAY, FREEZE, PARTICLES,
GRAIN CLOUD, REVERSE, SHIMMER, macros MOTION/TEXTURE/PITCH/SPACE/DIRT/MIX, pantalla SIMPLE completa, ADVANCED con GRAIN ENGINE,
GRAIN DELAY, PITCH STEP, TAPE STOP y medidores. Banco LT COSMOS: 50 presets con etiquetas.
**Fase 2 hecha:** FX CHAIN de 8 ranuras con 31 efectos (clic = elegir/mover/quitar, arrastrar izquierda-derecha = cantidad,
arriba-abajo = reordenar, "+" añade). MODULATION: MOD 1 (8 pasos dibujables, RATE, SWING, RANDOM, CLEAR), MOD 2 (curva:
SINE/TRIANGLE/SAW/SQUARE/SMOOTH RND, clic cambia la forma), ENV (sigue la entrada) y RND (sample & hold). Se asignan arrastrando
el chip sobre cualquier knob (anillo de color = cantidad); clic derecho en el knob = cantidad (±10…100 %) o quitar. Hasta 16 rutas,
se guardan con el proyecto.
**Fase 3 hecha:** navegador de presets (clic en el nombre del preset): búsqueda, filtros ALL/AMBIENT/RHYTHMIC/GLITCH/DARK/LO-FI/VOCAL/
★ FAVORITES, bancos con portada (LT COSMOS de fábrica, MY BANK y los que crees con + NEW BANK), CHANGE IMAGE (png/jpg, se recorta
cuadrada), IMPORT (.ltbank o .ltpreset), EXPORT (.ltbank para compartir). Clic derecho en un banco o preset de usuario = borrar.
SAVE guarda en el banco actual (LT COSMOS es de solo lectura → MY BANK). Carpeta: `LT Audio/Presets/LT NEBULA/`.
**LT NEBULA completo.**

## 5. LT ORBIT — delay
Diseño: `design/orbit/*`. Logo con la "O" como planeta con anillo.

- Modos: DIGITAL, TAPE, ANALOG, PING-PONG, REVERSE, SHIMMER, LO-FI, DUB.
- TIME (sync 1/4… con NORMAL/DOTTED/TRIPLET, o MS), FEEDBACK, MIX, FREEZE ∞.
- LOW CUT, HIGH CUT, WIDTH, DUCK, WOBBLE, DRIVE. Medidores IN/OUT (una sola etiqueta). Presets + SAVE.
- Visual: planeta = sonido original; cada eco = satélite orbitando en anillos cada vez más lejanos/tenues.
  FEEDBACK = más anillos, TIME = velocidad, PING-PONG = rebote lateral, REVERSE = hacia dentro, FREEZE = órbitas infinitas.

**Estado: hecho** (`plugins/orbit`). Una línea estéreo (hasta 6 s) con interpolación cúbica; color en la realimentación según el modo:
TAPE (wow/flutter, saturación, ~7 kHz, deslizamiento de tono al cambiar TIME), ANALOG (BBD oscuro ~3.8 kHz, ruido), PING-PONG (L↔R),
REVERSE (ecos al revés por bloques), SHIMMER (+12), LO-FI (8 bit, banda 300 Hz–3.2 kHz), DUB (banda estrecha, saturación, feedback hasta 105 %).
TIME: SYNC 1/64…1/1 o MS 1–2000; NORMAL/DOTTED/TRIPLET. 17 presets de fábrica.

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

**Estado: hecho** (`plugins/void`). Motor: FDN de 8 líneas con retardos modulados (interpolación cúbica), difusión de entrada,
ATTACK = "bloom" (entrada repartida en el tiempo), early reflections (HALL/ROOM/CHAMBER/GATED), damping alto/bajo dentro del tanque.
Etapas por tipo: SHIMMER (+12 en la realimentación), SPRING (cadena allpass = chirrido), REVERSE (cola reproducida al revés por bloques),
GATED (puerta: duración = DECAY × 0.08, máx. 1.2 s), VINTAGE (12 bit, ÷2), LO-FI (8 bit, ÷4, banda 250 Hz–3.8 kHz, siseo).
PRE-DELAY SYNC: 1/64 … 1/4 (con tresillos y 1/8 con puntillo). 36 presets de fábrica (3 por tipo) + SAVE.
El agujero negro es el knob de DECAY (arrastrar arriba/abajo). Cambiar de tipo no toca los knobs; elegir un preset sí.

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

---

## Estado y pendientes

- **LT TYPE BEATS v0.1** entregado (instaladores Mac/Windows, licencia funcionando).
  - Pendiente: mejorar precisión de BPM, tonalidad y acordes con canciones reales.
    Pedir al usuario ejemplos (nombre de canción o archivo + BPM/tonalidad/acordes reales) y ajustar `plugins/type-beats/Source/Analysis.cpp`.
- **LT License Studio** entregado. Clave pública instalada en `shared/license_public_key.txt`.
- **Siguiente:** LT PULSAR.

### LT PULSAR — correcciones pedidas tras la primera prueba (**hechas**)

Hecho: se quitaron las pestañas de rate del visor; WAVE y AUDIO TRIGGER se movieron fuera del visor (esquina del panel del knob).
Bus SIDECHAIN (opcional) añadido: el visor muestra la onda del sidechain delante (etiqueta "SIDECHAIN") y la procesada detrás.
AUDIO TRIGGER: cada golpe del sidechain reinicia la curva (sin sidechain, o si no llega señal en 1 s, sigue a la propia entrada).


1. **Quitar los números de rate (1/16, 1/8, 1/4, 1/2, 1/1) de dentro del visor de la curva.** Tapan los puntos de arriba:
   al subir un punto al máximo cae encima de esos números y ya no se puede arrastrar. Dejar solo el selector de abajo a la derecha.
2. **Entrada de SIDECHAIN externa.** Añadir un bus de sidechain al plugin para poder elegir otra pista (p. ej. el kick)
   como fuente (en Ableton se elige en el selector "Sidechain" del propio dispositivo).
   - El visor debe mostrar la **onda de la señal de sidechain** (el kick) en lugar de la señal procesada (el bajo),
     para ver dónde baja el volumen y dónde vuelve.
   - Opcional: modo "AUDIO TRIGGER" — que el ducking se dispare con los golpes de la señal de sidechain.

---

## LT TUNE — afinador vocal propio (próximo plugin)
(Idea acordada en otra sesión; "Auto-Tune" es marca de Antares, por eso el nombre LT TUNE.)

- Corrección de afinación en tiempo real con **baja latencia** (para grabar con el efecto puesto) y **preservación de formantes** (sin efecto "ardilla").
- **MATCH REFERENCE:** cargas una canción de referencia, se separa la voz y se deducen: **tonalidad y escala**, **retune speed**,
  **humanize / flex**, **vibrato** y **formantes**. Se aplican a LT TUNE y **todo queda editable**, con un control de cuánto se copia.
- Estética: familia espacial LT (negro/morado y modo claro celeste/amarillo), cristal casi invisible, partículas.
- **Diseño FINAL aprobado (versión 2, la imagen se adjuntará al programar):** logo "LT~TUNE" (onda senoidal entre LT y TUNE),
  VOICE y KEY & SCALE arriba en el centro, nota gigante fina con "#" en superíndice y resplandor morado detrás, medidor de cents,
  EDIT NOTES con orbes, panel derecho con RETUNE SPEED como planeta oscuro con anillo, 6 knobs, barra inferior DETUNE / MODE / IN-OUT
  horizontales. Al programar quitar los corchetes de "[Tenor]" y "[A Minor]".
- Versión 1 de referencia: `design/tune/main-dark.webp` (vista tipo afinador clásico, NO piano roll):
  VOICE, KEY & SCALE y preset arriba; nota detectada gigante + medidor de cents (-100…+100); EDIT NOTES con 12 notas
  como orbes/planetas (las de la escala encendidas, la que se canta destella); columna derecha con RETUNE SPEED (planeta
  con anillo, NATURAL↔ROBOT), HUMANIZE, FLEX TUNE, VIBRATO, FORMANT (+PRESERVE), TRANSPOSE, MIX; abajo DETUNE 440 Hz,
  MODE LOW LATENCY | HQ, medidores IN/OUT, deshacer/rehacer, ajustes, bypass.
  Correcciones al programar: quitar el texto "planet-style"; volver a poner SAVE, MATCH REFERENCE y THEME en la barra
  superior; en EDIT NOTES encender exactamente las notas de la escala elegida (A minor = A B C D E F G).
- Pendiente: ventana MATCH REFERENCE y versión clara; luego implementación.

## Igualador por referencia (proyecto futuro, sin nombre aún)
Modos **Máster / Voz / Beat**. Cada parámetro (EQ, compresión, brillo, de-esser, saturación, punch, LUFS, estéreo…) con su knob y on/off.
Opcional: analizar también la pista propia para compensar mic/sala. Medir el balance voz/beat de la referencia.

---

## Ideas para plugins futuros (anotadas el 2026-11-17)

### Buscador de sonidos similares (sample finder)
- Escanea tus librerías de sonidos (carpetas que elijas) y crea un índice.
- Le das un sonido (un kick, un sample de voz, un loop…) y te muestra los **más parecidos** de tus librerías, ordenados por similitud.
- Si es tonal (voz, melodía), detecta la **tonalidad**: busca el mismo sample o uno parecido en la misma tonalidad, y si está en otra,
  te dice **cuántos semitonos subir o bajar** para que coincida (y opcionalmente lo transpone).
- Filtros por tipo (kick, snare, hat, voz, loop…), BPM y tonalidad; escuchar y arrastrar el resultado directo al DAW.

### Plugin de "strokes" / efectos de transición para voces
- Reproduce la voz (o el sonido) y genera versiones procesadas para arrastrar directo al DAW:
  **solo la cola de reverb**, **solo el delay**, **pitch down** (bajada de tono / tape stop), **distorsión**, reverse, etc.
- Cada efecto como un "stroke" listo: botón para escuchar y arrastrar el audio resultante a una pista.

### Más adelante
- **Sintetizadores propios LT.**
- **Efectos de voz:** reversa, voces duplicadas/dobladas, cortes y chops (voces cortadas tipo glitch), y similares.
  Referencia enviada por el usuario (video "The FASTEST Vocal Effects"): dos herramientas rápidas que funcionan igual,
  **capturar → escuchar → arrastrar al DAW**:
  - **Stutter de voz (tipo "StutterVox"):** capturas una palabra o frase de la pista, el plugin la repite/corta al ritmo
    (RATE sincronizado: 1/4, 1/8, 1/16, 1/32…), con FADE IN, PAN, OUTPUT y efectos extra (PHONE, CHORUS, BACKVERB…).
    Botones PLAY WORD / PREVIEW / **DRAG TO DAW** (genera el audio ya procesado).
  - **Reverb reversa (tipo "Backverb"):** captura la voz y genera la cola de reverb al revés que "sube" antes de la palabra.
    Knobs LENGTH (en compases), SIZE, TONE, WIDTH, TREMOLO, LOW CUT, OUTPUT; CAPTURE / PLAY / **DRAG TO DAW**.
  - **DECIDIDO:** unirlas con los "strokes" de voz en un solo plugin con pestañas (STUTTER, BACKVERB, TAIL, DELAY THROW,
    PITCH DOWN, DISTORT…), todas con captura automática de la selección y arrastrar al DAW. Estética espacial LT.

### Dispositivos Max for Live (Ableton)
- Crear dispositivos propios **Max for Live** (.amxd) con la marca LT: se integran directamente en Ableton
  (acceso a clips, pistas, tempo, automatización y escenas de Live).
- Ideas: herramientas que actúan sobre el proyecto (p. ej. crear clips MIDI de acordes directamente desde LT TYPE BEATS,
  generadores/secuenciadores, modulación LFO que controla cualquier parámetro de Live, utilidades de mezcla).
- Nota: requieren Ableton Live Suite (o Max for Live) para usarlos.

### LT VOCAL SUITE — cadena vocal completa con asistente (referencia: idea tipo "Vocalings Suite")
- Un solo plugin para la voz principal con módulos en pestañas: **TUNE** (usa el motor de LT TUNE), **CLEANUP**
  (puerta de ruido, de-esser, quitar respiraciones/ruido), **TONE** (EQ, brillo, cuerpo), **DYNAMICS** (compresión, saturación),
  **SPACE** (reverb/delay de LT VOID y LT ORBIT) y **FX** (efectos creativos).
- **Asistente con chat (y micrófono para dictar):** escribes en lenguaje natural "más brillo", "menos sibilancia",
  "voz tipo trap melódico, autotune duro y delay"… y el asistente ajusta los módulos. Muestra qué cambió y se puede deshacer.
  Los presets "estilo artista" describen el **procesamiento** (no clonan la voz de nadie).
- Pantalla con mascota/personaje espacial LT (en vez del robot de la referencia), estética de la familia LT.
- **La IA integrada es el centro del plugin:** escribes "quiero un vocal chain estilo X artista" y la IA arma la cadena completa
  (afinación, EQ, compresión, saturación, de-esser, reverb, delay, FX) con valores concretos, y luego puedes seguir pidiendo cambios.
  Implementación prevista: el plugin envía la petición + la lista de parámetros disponibles a un modelo de IA (API de Claude),
  que devuelve los ajustes en formato estructurado (JSON) y el plugin los aplica. Requiere internet y una clave de API
  (de LT, guardada en un servidor propio, o del usuario). Alternativa sin conexión: biblioteca de presets por estilo.
  Opcional: que la IA también "escuche" un análisis de la voz (brillo, sibilancia, dinámica) para adaptar los ajustes.
- BYPASS para comparar con la voz original.
