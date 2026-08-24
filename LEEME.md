# The XaLZa — VST3 (proyecto fuente JUCE)

Esto es el código fuente completo de un plugin **VST3 real**, escrito en
C++ con el framework JUCE — el mismo que usan la mayoría de plugins
comerciales serios. No es un mockup ni una simulación: es una cadena de
voz completa (8 módulos) que compila a un `.vst3` de verdad.

**Por qué te doy código fuente y no un `.vst3` ya compilado:** yo trabajo
en un sandbox Linux en la nube, y un VST3 compilado en Linux no funciona
en tu Ableton de Windows — los formatos binarios son distintos por
sistema operativo. Verifiqué que este código compila perfectamente aquí
mismo (herramientas de línea de comandos, Linux), pero el `.vst3` que tú
vas a usar en Ableton lo tienes que compilar tú, en tu propia máquina
Windows, con Visual Studio. Es un proceso de 15-20 minutos, la mayoría
esperando a que se descargue JUCE la primera vez. Te lo explico paso a
paso abajo.

## Qué hace el plugin

Los mismos 8 módulos que la versión Max for Live y el mockup web, en el
mismo orden: **Preamp → Compresor → EQ → Saturador → Reverb + Delay
(en paralelo, con duck) → Limitador → Ancho Estéreo**, más Ganancia de
Entrada/Salida. 29 parámetros en total. Cada módulo (menos Master) tiene
un knob "macro" grande que mueve varios parámetros finos a la vez, con
el mismo comportamiento que las otras dos versiones: si tocas el macro,
manda el macro; si después tocas un knob fino a mano, ese knob manda
hasta que vuelvas a tocar el macro ("el último que tocaste gana").

DSP usado (todo con las librerías estándar de JUCE, no matemática
inventada): filtro paso-alto y shelving EQ (`juce::dsp::IIR::Filter`),
compresor (`juce::dsp::Compressor`), saturación tipo tanh a mano,
reverb (`juce::dsp::Reverb`), delay ping-pong estéreo con feedback
cruzado (`juce::dsp::DelayLine`), limitador (`juce::dsp::Limiter`), y
ancho estéreo mid-side hecho a mano.

## Dos formas de compilarlo

**Opción A — no instalas nada en tu computadora** (recomendada si no
quieres pelear con instaladores): subes esta carpeta a GitHub (gratis) y
una máquina de GitHub en la nube lo compila por ti en unos 10-15
minutos. Tú solo descargas el `.vst3` ya terminado al final.

**Opción B — lo compilas tú mismo en tu PC** con Visual Studio (gratis,
pero son ~6 GB de instalación y más pasos).

---

### Opción A: que GitHub lo compile por ti (sin instalar nada)

1. Ve a **github.com** y crea una cuenta gratis si no tienes una
   (botón "Sign up").
2. Ya con sesión iniciada, haz clic en el botón verde **"New"** (o el
   símbolo `+` arriba a la derecha → "New repository") para crear un
   repositorio nuevo.
3. Ponle un nombre, por ejemplo `xalza-vst`. Déjalo como **"Public"**.
   No marques ninguna casilla de "Add a README". Dale a **"Create
   repository"**.
4. En la página que aparece, busca el enlace que dice algo como
   **"uploading an existing file"**. Haz clic ahí.
5. Descomprime el `.zip` que te mandé en tu computadora, y luego
   **arrastra toda la carpeta** `XaLZa-VST-Source` (o su contenido:
   `CMakeLists.txt`, la carpeta `Source`, y la carpeta `.github`) hacia
   el recuadro de la página de GitHub. Espera a que termine de subir y
   dale a **"Commit changes"** (el botón verde de abajo).
   - Importante: la carpeta `.github` (con el punto adelante) tiene que
     quedar subida también — es la que le dice a GitHub cómo compilar.
     Si tu navegador no te deja arrastrar carpetas ocultas, sube primero
     todo lo demás, y luego usa "Add file → Create new file", escribe en
     el nombre exactamente `.github/workflows/build.yml` (GitHub crea
     las carpetas solo al ver las barras `/`), y pega ahí el contenido
     de ese archivo (está dentro del zip que te mandé).
6. Arriba en el menú del repositorio, haz clic en la pestaña
   **"Actions"**. Debería aparecer una compilación corriendo sola
   (círculo amarillo girando) — si no aparece nada, haz clic en el
   flujo de trabajo listado a la izquierda y luego en el botón
   **"Run workflow"**.
7. Espera unos 10-15 minutos. Cuando el círculo se vuelva un ✅ verde,
   haz clic sobre esa ejecución (el texto azul).
8. Abajo del todo de esa página verás una sección **"Artifacts"** con
   un archivo llamado **"The-XaLZa-VST3-Windows"**. Haz clic para
   descargarlo — es un `.zip` con tu plugin ya compilado adentro.
9. Descomprime ese `.zip`. Vas a encontrar una carpeta llamada
   **"The XaLZa.vst3"**. Cópiala (la carpeta entera) dentro de:
   `C:\Program Files\Common Files\VST3\`
   (puede pedirte permiso de administrador, dile que sí).
10. Abre o reinicia Ableton Live. En **Plug-ins → VST3** debería
    aparecer **"The XaLZa"**. ¡Ya lo puedes usar!

### Opción B: compilarlo tú mismo con Visual Studio

1. Instala **Visual Studio 2022 Community** —
   https://visualstudio.microsoft.com/ — y al instalar, marca la carga
   de trabajo **"Desarrollo para el escritorio con C++"** (Desktop
   development with C++).
2. Instala **Git para Windows** — https://git-scm.com/download/win
   (dale "Next" a todo, no hay que cambiar nada).
3. Descomprime esta carpeta en algún sitio simple, por ejemplo
   `C:\Dev\XaLZa-VST-Source\`.
4. Abre **Visual Studio 2022** → **"Abrir una carpeta local"** (Open a
   local folder) → selecciona `XaLZa-VST-Source`.
5. Espera a que termine de configurarse solo (descarga JUCE, tarda
   varios minutos la primera vez).
6. Arriba, cambia la configuración de `x64-Debug` a **`x64-Release`**.
7. Menú **Build → Build All** (o `Ctrl+Shift+B`).
8. El `.vst3` queda en `build\XaLZa_artefacts\Release\VST3\The XaLZa.vst3`
   y se intenta copiar solo a `C:\Program Files\Common Files\VST3\`
   (si falla por permisos, cópialo tú a mano ahí).
9. Abre o reinicia Ableton Live. En **Plug-ins → VST3** debería
   aparecer **"The XaLZa"**.

## Si algo falla

- **(Opción A) La pestaña "Actions" no muestra nada corriendo**: revisa
  que subiste la carpeta `.github/workflows/build.yml` — sin ese
  archivo, GitHub no sabe que tiene que compilar nada.
- **(Opción A) La ejecución termina con una ❌ roja**: haz clic en la
  ejecución, luego en "build-windows" para ver el error exacto, y
  pégamelo — lo reviso y te doy el archivo corregido.
- **(Opción B) "git no se reconoce como comando"**: no tienes Git
  instalado, o no está en el PATH — instala Git para Windows y reinicia
  Visual Studio.
- **(Opción B) Error de red al configurar** (no puede clonar JUCE):
  revisa tu conexión a internet; si estás detrás de un firewall
  corporativo, puede que bloquee `github.com`.
- **No aparece en Ableton tras compilar (cualquier opción)**: confirma
  que la carpeta `The XaLZa.vst3` esté realmente en
  `C:\Program Files\Common Files\VST3\`, y en Ableton ve a Preferencias
  → Plug-ins y revisa que esa carpeta esté entre las carpetas VST3
  escaneadas, luego usa "Rescan".
- **Quieres cambiar la versión de JUCE**: en `CMakeLists.txt`, la línea
  `GIT_TAG 9.0.1` fija qué versión de JUCE se descarga. Puedes cambiarla
  a otra versión publicada si quieres, pero 9.0.1 es la que verifiqué
  que compila este código sin errores.

## Estructura del proyecto

```
XaLZa-VST-Source/
  CMakeLists.txt          <- configuración del build (descarga JUCE solo)
  Source/
    Params.h              <- los 29 parámetros, rangos, y el sistema de macros
    PluginProcessor.h/.cpp <- todo el DSP (la cadena de audio real)
    PluginEditor.h/.cpp    <- la interfaz gráfica (los knobs)
```

Si en algún momento quieres que le añada más cosas (un preset browser,
más bandas de EQ, un medidor de nivel visual, modo mono, etc.), dímelo
y seguimos construyendo sobre esta misma base.
