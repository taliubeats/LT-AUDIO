# Instalar los productos LT

Descargas: pestaña **Releases** del repositorio → **latest**.

## Mac

1. Descarga el `.zip` y ábrelo.
2. **LT License Studio:** arrastra `LT License Studio.app` a **Aplicaciones**.
   **Plugins:** copia el `.vst3` a `~/Library/Audio/Plug-Ins/VST3` y el `.component` a `~/Library/Audio/Plug-Ins/Components`.
3. Como no están firmados con una cuenta de Apple Developer, macOS los bloquea la primera vez. Abre **Terminal** y ejecuta:

   ```bash
   xattr -dr com.apple.quarantine "/Applications/LT License Studio.app" ~/Library/Audio/Plug-Ins/VST3/LT*.vst3 ~/Library/Audio/Plug-Ins/Components/LT*.component
   ```

4. En Ableton: *Settings → Plug-Ins →* activa **Use VST3 / Audio Units** y pulsa **Rescan**.

## Windows

1. Copia el `.vst3` a `C:\Program Files\Common Files\VST3`.
2. Si Windows muestra "editor desconocido", elige **Más información → Ejecutar de todas formas**.
3. En Ableton: *Options → Preferences → Plug-Ins →* **Rescan**.

## Licencias (configuración única de LT License Studio)

1. Abre LT License Studio → **SETTINGS** → **COPY PUBLIC KEY**.
2. En GitHub abre `shared/license_public_key.txt`, pulsa el lápiz, reemplaza `NONE` por la clave y guarda (*Commit changes*).
3. GitHub vuelve a compilar los plugins con tu clave. Instala esa versión nueva.
4. **EXPORT KEY BACKUP** y guarda la copia en un lugar privado. Sin ella no podrás crear licencias nuevas.
