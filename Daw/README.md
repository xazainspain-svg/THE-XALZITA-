# Xaza Live — mini DAW web (original)

Se abre en el navegador, sin instalar: abre `Daw/index.html` (o `python3 -m http.server` dentro de `Daw/`).

- **Session**: 8 pistas × 8 escenas, clips en bucle con lanzamiento cuantizado a compás, lanzar escena, parar pista. Clic derecho en un clip: borrarlo.
- **Arrangement**: 32 compases; clic en una celda para ciclar entre los clips de la pista; automatización de volumen por compás.
- **Piano roll**: clic añade, arrastrar el borde alarga, clic en nota borra; clips de 1/2/4 compases; filas de batería para pistas de drums.
- **Instrumentos**: sintetizador (onda, corte, resonancia, ataque, release) y caja de ritmos sintetizada (8 sonidos).
- **Efectos por pista**: filtro, delay, reverb, distorsión, chorus, compresor, EQ 3 bandas.
- **Mezclador**: volumen, panorama, mute, solo, medidores. Limitador en master.
- **Proyecto**: Guardar/Abrir (JSON), Exportar WAV (render offline del Arrangement).
- Teclado: Espacio = play/stop; A-K tocan la pista seleccionada; Z/X cambian de octava.

Limitaciones: sin grabación de audio ni importación de samples, sin plugins VST (los 90 de `Plugins90/` son VST3 de escritorio y no cargan aquí), sin deshacer.
