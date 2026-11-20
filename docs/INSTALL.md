# Instalar los productos LT

Descargas: pestaña **Releases** del repositorio → **latest**.

## Mac

1. Descarga el instalador del plugin, por ejemplo `LT-TYPE-BEATS-Installer-mac.pkg`
   (o `LT-AUDIO-All-Plugins-Installer-mac.pkg` para instalar todos).
2. **La primera vez:** clic derecho sobre el `.pkg` → **Abrir** → **Abrir**.
   (O: Ajustes del Sistema → Privacidad y seguridad → **Abrir de todas formas**.)
3. **Continuar → Instalar**, escribe la contraseña de tu Mac y listo.
4. En Ableton: *Settings → Plug-Ins →* **Rescan**.

LT License Studio: `LT-License-Studio-Installer-mac.pkg` (se instala en Aplicaciones).

LT COSMOS y LT LYRA vienen como plugin (en el instalador de todos los plugins o en el suyo propio)
y también como app: la app standalone está dentro de los artefactos del build.

LT WARP (conversor): `LT-WARP-Installer-mac.pkg` (se instala en Aplicaciones). Por ahora necesita
ffmpeg instalado en el Mac: en Terminal, `brew install ffmpeg`.

## Windows

1. Descarga `LT-TYPE-BEATS-Installer-windows.exe` (o el de todos los plugins).
2. Si aparece "Windows protegió su PC": **Más información → Ejecutar de todas formas**.
3. **Siguiente → Instalar**. Se instala en `C:\Program Files\Common Files\VST3`.
4. En Ableton: *Options → Preferences → Plug-Ins →* **Rescan**.

## LT PULSAR: MIDI TRIGGER en Ableton

1. Pon LT PULSAR en la pista que quieres "bombear" y activa **MIDI TRIGGER**.
2. Crea una pista MIDI con las notas del bombo (o cualquier ritmo).
3. En esa pista MIDI: **MIDI To** → la pista con LT PULSAR → **LT PULSAR**.

## LT VACUUM (Max for Live, Ableton 11 y 12)

1. Descarga `LT-VACUUM-MaxForLive.zip` y descomprímelo.
2. Copia la carpeta `LT VACUUM` a tu User Library de Ableton, por ejemplo en
   `User Library/Presets/Audio Effects/Max Audio Effect/`. El `.amxd` y `lt_vacuum.js` tienen que quedar juntos.
3. Arrastra `LT VACUUM.amxd` a cualquier pista, pulsa **SCAN** y, si la lista te parece bien, **DELETE**.
   Se deshace con Cmd/Ctrl+Z. La primera vez, pruébalo con una copia del set.
4. Si Live no abre el `.amxd`: crea un "Max Audio Effect" vacío, abre su editor, pega dentro todo el contenido
   de `LT VACUUM.maxpat` y guárdalo.
