# Xaza Live — mini DAW web (original)

Se abre en el navegador, sin instalar: abre `Daw/index.html` (o `python3 -m http.server` dentro de `Daw/`).

- **Session**: 8 pistas × 8 escenas, clips en bucle con lanzamiento cuantizado a compás, lanzar escena, parar pista. Clic derecho en un clip: borrarlo.
- **Arrangement (estilo Ableton)**: línea de tiempo de 64 compases; doble clic en una pista inserta un clip; arrastrar mueve, bordes recortan/alargan (el clip se repite en bucle); al soltar sobre otros clips los recorta; Duplicar (Ctrl+D), Dividir en el cursor (Ctrl+E), Borrar (Supr); regla con cursor de inicio y región de loop (Mayús+arrastrar); "Volcar escenas" pasa Session al Arrangement; automatización de volumen y panorama con puntos editables; zoom y snap.
- **Mixer (estilo FL Studio)**: 16 inserts + master con fader vertical, medidor estéreo en dB, panorama, mute/solo y 10 ranuras de efectos por insert (con on/off); cada pista se enruta a un insert; master con limitador.
- **Piano roll (estilo FL Studio)**: herramientas Dibujar / Seleccionar / Borrar / Cortar / Silenciar (atajos 1-5), snap hasta 1/4 de paso, zoom, mover y alargar notas, selección con marquesina, Ctrl+A / Ctrl+D / flechas, carril de velocidad, resaltado de escala, estampador de acordes, notas fantasma y panel maximizable. Clic derecho borra.
- **Inmersivo 3D**: cada pista puede ser un objeto con posición izquierda/derecha, atrás/frente y altura; se coloca arrastrando en una vista superior con guía 7.1.4. La escucha es binaural (HRTF, usa auriculares) y la posición se exporta como JSON. Es una mezcla inmersiva propia: no es Dolby Atmos ni usa su tecnología, formatos o licencia.
- **Pistas de audio**: importar samples (clic en una celda vacía de una pista de audio) o grabar con el micrófono (● Mic); forma de onda en Session y Arrangement. Los samples suenan a su velocidad original y no se guardan en el JSON del proyecto.
- **Grabación MIDI en vivo**: ● Rec graba en el clip seleccionado lo que toques con el teclado del ordenador o un teclado MIDI (Web MIDI), cuantizado a 1/2 paso.
- **Retornos**: dos buses de efecto (reverb y delay) con envíos por pista, fader y parámetros en el Mixer.
- **Edición de proyecto**: deshacer/rehacer (Ctrl+Z / Ctrl+Y), copiar/pegar clips, añadir y quitar pistas (hasta 16), tap tempo, swing, analizador de espectro del master y ayuda de atajos (?).
- **Channel Rack (estilo FL)**: step sequencer por patrón (escena) con pintado por arrastre, acentos y una fila por sonido de batería.
- **Herramientas del piano roll**: cuantizar, humanizar, arpegiador (↑ ↓ ↑↓ azar, 1/1–1/4 de paso), strum, voltear en tiempo y altura, pan y tono fino por nota, nombre de patrón.
- **Automatización general**: carriles para cualquier parámetro (volúmenes, panoramas, envíos, filtro del sintetizador, parámetros de cualquier efecto, tempo). Los parámetros se restauran al parar.
- **Mixer avanzado**: EQ paramétrico de 4 bandas, sidechain (ducking contra otro insert, solo en tiempo real, no en el render), enrutado de un insert a otro y presets por efecto.
- **Audio**: editor de samples (recorte, ganancia, invertir, normalizar, estirar al tempo con granular básico) y navegador de samples.
- **Instrumentos extra**: sintetizador con onda 2, sub y envolvente de filtro; sintetizador FM de dos operadores; sampler cromático con nota raíz y bucle.
- **MIDI y stems**: exportar el Arrangement (o la escena) a .mid, importar .mid al clip seleccionado y exportar un WAV por pista.
- **Edición estilo Pro Tools**: modos Slip / Shuffle / Grid; herramientas Grabber / Trim / Selector (rango con Supr); fundidos de entrada y salida, crossfade y ganancia de clip; strip silence (los fragmentos se redondean al paso de 1/16).
- **Grabación estilo Pro Tools**: región de punch in/out con pre-roll y tomas (comping): cada grabación nueva sobre un clip añade una toma, y se elige por tramo con el selector "Toma" y Ctrl+E.
- **Marcadores y mapa de tempo**: marcadores en la regla (clic para ir, clic derecho para borrar); el mapa de tempo es un carril de automatización de BPM.
- **Grupos y VCA**: grupos de pistas con VCA, mute y solo; envíos pre-fader o post-fader.
- **Automatización en vivo**: modos Leer, Touch, Latch y Escribir sobre cualquier control durante la reproducción del Arrangement.
- **Instrumentos**: sintetizador (onda, corte, resonancia, ataque, release) y caja de ritmos sintetizada (8 sonidos).
- **Efectos**: filtro, delay, reverb, distorsión, chorus, compresor, EQ 3 bandas.
- **Proyecto**: Guardar/Abrir (JSON), Exportar WAV (render offline del Arrangement).
- Teclado: Espacio = play/stop; A-K tocan la pista seleccionada; Z/X cambian de octava.

Limitaciones: no hay loop record, Elastic Audio ni Beat Detective; sin compases distintos de 4/4, sin exportar MP3, el sidechain no se aplica en el render offline, sin grabación de audio ni importación de samples, sin plugins VST (los 90 de `Plugins90/` son VST3 de escritorio y no cargan aquí), sin deshacer.
