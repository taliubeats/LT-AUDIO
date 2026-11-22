# LT Max for Live

Cuatro dispositivos para Ableton Live 11 y 12 (con Max for Live: Live Suite, o Standard + Max for Live).

| Dispositivo | Tipo | Para qué |
|---|---|---|
| **LT CHORDS** | MIDI | Tocas una nota y suena un acorde. Además te sugiere el siguiente acorde según el género. |
| **LT CLIP GAIN** | Audio | Sube o baja en pasos exactos de dB la ganancia del clip de audio seleccionado. Puedes asignarlo a teclas. |
| **LT BOUNCE** | Audio | Pasa a audio la pista seleccionada, en el mismo sitio (bounce in place). |
| **LT VACUUM** | Audio | Busca las pistas que no llegan al Main y las borra. Antes de borrar, te las enseña. |

## 1. Primero: que Max cargue

Si en el dispositivo sale **"The selected Max application could not be loaded"**, el fallo es de Max, no del dispositivo, y ningún Max for Live va a funcionar. Esta es la solución oficial de Ableton:
1. Cierra Live.
2. Si usas el Max que viene con Live, borra **Max.app** de Aplicaciones.
3. Haz un **reset de Max for Live**.
4. Expulsa el instalador de Live si sigue montado. Abrir Live desde el instalador provoca este error.
5. Instala la última versión de Max for Live y abre Live desde Aplicaciones.

Artículo de Ableton: https://help.ableton.com/hc/en-us/articles/4404393313042

## 2. Instalar
1. Descomprime `LT-MaxForLive-Devices.zip`.
2. Copia la carpeta `LT Max for Live` a tu User Library. Por ejemplo: `Música/Ableton/User Library/Presets/`.
3. En cada carpeta, el `.amxd` y su `.js` tienen que quedar juntos. Si quieres un único archivo por dispositivo: ábrelo en el editor de Max (botón del dispositivo), pulsa **Freeze** y guarda.
4. Si Live no abre un `.amxd`, usa el `.maxpat`, que es el mismo patch en texto:
   1. Crea un "Max MIDI Effect" (para CHORDS) o un "Max Audio Effect" (para los demás).
   2. Abre su editor.
   3. Pega dentro todo el contenido del `.maxpat` y guárdalo.

## LT CHORDS (pistas MIDI, antes del instrumento)
- **MAYOR / MENOR / EN TONO**:
  - MAYOR o MENOR: todos los acordes salen mayores o menores.
  - EN TONO: cada nota da el acorde de la escala que elijas (tonalidad + MAY/MEN). En menor, el V sale mayor, como en bachata y reggaetón.
- **7a, 9a, 8a**: se combinan como quieras: 7+9, 7+8, 7+8+9...
  - En mayor, la 7a es maj7. Con **DOM7** pasa a ser 7 de dominante.
  - La 8a añade la nota raíz una octava arriba.
- **GÉNERO**:
  - Géneros: reggaetón, trap, R&B, pop, lo-fi, bachata, afro, house.
  - Después de cada acorde te dice qué acordes suelen venir a continuación y si son **mayores o menores**. Por ejemplo, en reggaetón después de Am → F mayor, porque la base i-VI-III-VII es la más usada.
- Si dos acordes comparten una nota, esa nota no se corta antes de tiempo. Al apagar el dispositivo, todas las notas se paran.

## LT CLIP GAIN (cualquier pista de audio)
- Selecciona un clip de audio. Los botones **−3 / −1 / +1 / +3 dB** y **0 dB** cambian su ganancia en pasos exactos, con la precisión que muestra Live (0.1 dB).
- Para usarlo con teclas: **Cmd/Ctrl+K**, haz clic en un botón, pulsa la tecla y vuelve a pulsar Cmd/Ctrl+K.
- Live no deja a Max ver el clip que está bajo el ratón: funciona con el clip seleccionado.
- Cada clic puede dejar varios pasos en el historial de deshacer.

## LT BOUNCE
- Live 12.2 o posterior ya trae **Bounce Track in Place** (clic derecho en la pista). LT BOUNCE es para Live 11, o para hacerlo con un botón.
- Selecciona la pista, pon el loop sobre la parte que quieres y pulsa **BOUNCE**.
  - Graba en tiempo real lo que suena la pista (después de sus efectos y del mezclador) en una pista de audio nueva, justo debajo.
  - Después, el original se silencia (MUTE), se queda igual (KEEP) o se borra (DELETE). **CANCEL** lo deshace.

## LT VACUUM
Pulsa SCAN, revisa la lista y pulsa DELETE. Antes de borrar vuelve a comprobarlo todo y respeta lo que hayas arreglado. Se deshace con Cmd/Ctrl+Z.

## Probado / sin probar
- La lógica de los cuatro está probada fuera de Live, con una Live API simulada (`m4l/*/tests`).
- Ninguno se ha abierto todavía dentro de Ableton. La primera vez, pruébalos en una copia del set.
