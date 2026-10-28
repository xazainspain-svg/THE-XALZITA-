'use strict';
// Arranque: se carga el último, cuando ya existen roll.js, mixer.js y arr.js.
try { const saved = localStorage.getItem('xazalive_song'); if (saved) song = upgradeSong(JSON.parse(saved)); } catch (e) { song = demoSong(); }
$('#bpm').value = song.bpm;
initWins(); buildMenus(); initKnobs(); initToolbar(); updateWinButtons();
renderAll(); openWin('rack');
requestAnimationFrame(frame);
window.__daw = { get song() { return song; }, get eng() { return eng; }, renderWav, ensureEngine };
