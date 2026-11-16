# LT TUNE — especificación para empezar a programarlo

Afinador vocal propio de la familia LT ("Auto-Tune" es marca de Antares: no usar ese nombre).
Leer también `docs/DESIGN.md` (reglas generales, licencias, temas) y seguir la estructura de los otros plugins (`plugins/*`).

## Diseño aprobado

- Referencia guardada: `design/tune/main-dark.webp` (versión 1, casi igual a la final).
- **Las 4 imágenes finales** (principal oscuro/claro, MATCH REFERENCE oscuro/claro) las adjunta el usuario al empezar la sesión:
  guardarlas como `design/tune/main-dark-final`, `main-light`, `match-dark`, `match-light`.

### Pantalla principal
- **Arriba:** logo `LT~TUNE` (onda senoidal entre LT y TUNE, morado / en claro: LT blanco, TUNE amarillo),
  selectores **VOICE** (Soprano, Alto/Tenor, Tenor, Baritone, Bass) y **KEY & SCALE**, preset + SAVE, **MATCH REFERENCE**, **THEME**,
  deshacer/rehacer, ajustes, bypass. (En la imagen faltan SAVE, MATCH REFERENCE y THEME: añadirlos. Quitar los corchetes de "[Tenor]".)
- **Centro:** nota detectada gigante con fuente muy fina y "#" en superíndice, con resplandor detrás;
  **medidor de afinación** -100…+100 cents con barra que se mueve en tiempo real.
  Animación: la nota brilla cuando estás afinado y tiembla un poco al desafinar.
- **EDIT NOTES:** 12 orbes de cristal (7 naturales abajo, 5 sostenidos arriba desplazados). Encendidas = notas permitidas
  (por defecto las de la escala elegida: A minor = A B C D E F G). Clic = activar/desactivar. La nota cantada destella. Botón ↺ = volver a la escala.
- **Derecha:** knob grande **RETUNE SPEED** como planeta oscuro con anillo (NATURAL ↔ ROBOT, valor en ms; destella en cada corrección),
  y knobs **HUMANIZE, FLEX TUNE, VIBRATO, FORMANT (+ PRESERVE), TRANSPOSE, MIX**.
- **Abajo:** **DETUNE** (referencia, 440.0 Hz), **MODE: LOW LATENCY | HQ**, medidores IN/OUT horizontales.

### Ventana MATCH REFERENCE
- Zona para soltar una canción ("DROP A SONG TO COPY ITS TUNING"), onda de polvo de estrellas con la voz aislada resaltada,
  estado "Vocal isolated · Analysis complete".
- Lista de ajustes detectados, cada uno con su valor y un **interruptor on/off**: KEY & SCALE, RETUNE SPEED (ms + HARD/SOFT),
  HUMANIZE, FLEX TUNE, VIBRATO (Kept/Flattened), FORMANT.
- PITCH PREVIEW (curva de afinación de la voz de referencia), knob **MATCH AMOUNT** (0–100 %), **APPLY TO LT TUNE**, CANCEL.

## Parámetros (propuesta)
| ID | Rango | Notas |
|---|---|---|
| key | C…B | |
| scale | Chromatic, Major, Minor, Harmonic Minor, Pentatonic Major/Minor, Blues, Custom | Custom = notas de EDIT NOTES |
| noteMask | 12 bools | |
| voice | Soprano/Alto/Tenor/Baritone/Bass | define el rango de detección |
| retune | 0–400 ms | 0 = robot |
| humanize | 0–100 | notas largas más naturales |
| flexTune | 0–100 | solo corrige cerca de la nota |
| vibrato | -100…+100 | negativo aplana, positivo añade |
| formant | -12…+12 st, preserve on/off | |
| transpose | -12…+12 st | |
| detune | 430–450 Hz | |
| mix | 0–100 % | |
| mode | Low Latency / HQ | latencia baja para grabar; HQ más ventana |

## Motor (DSP)
- Detección de pitch: YIN/MPM con ventana según VOICE; suavizado y detección de voz/silencio.
- Corrección: nota destino según escala/máscara + retune speed (glide), humanize (más lento en notas largas), flex (zona muerta).
- Desplazamiento de pitch con **preservación de formantes** (PSOLA para baja latencia; opción HQ con ventana mayor).
- Reportar la latencia al host (`setLatencySamples`).
- MATCH REFERENCE: separación de voz (opcional/futuro: modelo ONNX); al principio con la voz del archivo
  (o un stem de voz) → curva de pitch → estimar tonalidad/escala, velocidad de salto entre notas, vibrato, desviación en notas largas.

## Tareas
1. Guardar las imágenes finales en `design/tune/`.
2. Crear `plugins/tune` (procesador, editor, parámetros, presets) usando `lt_shared` (licencia, temas, nebulosa, knobs de cristal).
3. Motor de detección + corrección con tests (`tests/TuneTest.cpp`: senos desafinados → salida afinada).
4. UI según el diseño, modo oscuro y claro.
5. Ventana MATCH REFERENCE.
6. Añadir a `tests/CMakeLists.txt` (stress) y a la CI; el instalador se genera solo.
7. Actualizar manual: `docs/manuals/make_manuals.py`.
