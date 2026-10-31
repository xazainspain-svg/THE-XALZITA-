# Grabador de tutoriales de Xadaw

`xadaw_tutor.py` abre Xadaw en una pantalla virtual, lo maneja con ratón y teclado (lo hace Claude desde la
interfaz, o tú con un guion) y graba:

- `tutorial.mp4` — video con **rótulos** explicando cada paso, un **anillo amarillo en cada clic** (naranja en el
  derecho) y las **teclas** que se pulsan.
- `tutorial.srt` — subtítulos.
- `tutorial.md` — tutorial paso a paso con una captura por paso y el guion completo.
- `eventos.json` — todo lo que se hizo y cuándo.

Requisitos (Linux): `python3`, `ffmpeg` (con libx264 y libass), `xdotool`, `Xvfb`, `imagemagick` (`import`) y el
programa compilado (`Lemon-Studio/target/release/lemonstudio`, o `Xadaw` en el PATH).

## Con un guion (repetible)

```bash
python3 xadaw_tutor.py run ejemplo_xadaw.json
```

Cada paso del guion es `{"do": ..., "say": "texto del rótulo", "shot": "nombre"}`. Pasos: `say`, `click` (`x`, `y`,
`button`, `double`), `drag` (`x1 y1 x2 y2`), `key` (`keys`, como en xdotool: `ctrl+b`, `F9`), `type` (`text`),
`wait` (`seconds`), `shot` (`name`). Con `"shot"` en un paso, se guarda una captura de lo que quedó en pantalla.

## Paso a paso, en vivo (así lo usa Claude)

```bash
python3 xadaw_tutor.py start --out mi_tutorial        # abre la app y empieza a grabar
python3 xadaw_tutor.py look                           # captura la pantalla (look.png) para ver qué hay
python3 xadaw_tutor.py click 160 63 --say "Abre el menú VIEW"
python3 xadaw_tutor.py key ctrl+b --say "Duplica los clips con Ctrl+B"
python3 xadaw_tutor.py shot duplicar                  # captura que irá al tutorial.md
python3 xadaw_tutor.py stop --title "Cómo duplicar clips"   # monta video + subtítulos + markdown
```

`look` sirve para decidir el siguiente clic; los rótulos duran lo que tarda en leerse la frase.

## Límites

- No graba voz: el video lleva rótulos y subtítulos. Se puede añadir narración después con cualquier editor.
- Graba la versión Linux de Xadaw en una pantalla virtual; en tu PC con Windows usa OBS, o corre esto en Linux.
- En el contenedor no hay audio, así que no se oye la música (el video es solo imagen).
