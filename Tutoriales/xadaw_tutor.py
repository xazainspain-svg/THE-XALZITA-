#!/usr/bin/env python3
"""Grabador de tutoriales para Xadaw (y cualquier app de escritorio X11).

Abre la app en una pantalla virtual, la maneja con raton y teclado, y graba un video con rotulos, anillos en cada
clic y las teclas pulsadas. Al terminar deja el video (tutorial.mp4), los subtitulos (tutorial.srt) y un tutorial
paso a paso con capturas (tutorial.md).

Uso interactivo (sesion que va guardando su estado):
  xadaw_tutor.py start --app /ruta/a/Xadaw
  xadaw_tutor.py say "Abre el mixer"          # rotulo en pantalla
  xadaw_tutor.py click 1093 63 --say "Clic en el boton del mixer"
  xadaw_tutor.py key F9
  xadaw_tutor.py shot nombre                  # captura (tambien va al tutorial.md)
  xadaw_tutor.py stop                         # renderiza video + markdown
Uso por guion:  xadaw_tutor.py run guion.json   (ver ejemplo_xadaw.json)
Mirar la pantalla en cualquier momento:  xadaw_tutor.py look   (guarda look.png)
"""
import argparse, json, os, shutil, signal, subprocess, sys, time

STATE = os.environ.get("XT_STATE", "/tmp/xadaw_tutor/state.json")


def load():
    if not os.path.exists(STATE):
        sys.exit("No hay sesion: ejecuta primero 'start'.")
    return json.load(open(STATE))


def save(st):
    os.makedirs(os.path.dirname(STATE), exist_ok=True)
    json.dump(st, open(STATE, "w"), indent=1)


def env(st):
    e = dict(os.environ)
    e["DISPLAY"] = st["display"]
    return e


def now(st):
    return time.time() - st["t0"]


def log(st, kind, **kw):
    st["events"].append({"t": round(now(st), 3), "wall": round(time.time() % 60, 2), "kind": kind, **kw})
    save(st)


def xdo(st, *args):
    subprocess.run(["xdotool", *map(str, args)], env=env(st), check=False)


def default_app():
    here = os.path.dirname(os.path.abspath(__file__))
    for c in (os.path.join(here, "..", "Lemon-Studio", "target", "release", "lemonstudio"), shutil.which("xadaw") or "", shutil.which("Xadaw") or ""):
        if c and os.path.exists(c):
            return os.path.abspath(c)
    return "xadaw"


def free_display():
    for n in range(90, 120):
        if not os.path.exists(f"/tmp/.X11-unix/X{n}") and not os.path.exists(f"/tmp/.X{n}-lock"):
            return f":{n}"
    sys.exit("No encuentro una pantalla libre.")


# ------------------------------------------------------------------ comandos
def cmd_start(a):
    out = os.path.abspath(a.out)
    os.makedirs(os.path.join(out, "capturas"), exist_ok=True)
    w, h = a.size.split("x")
    disp = free_display()
    xvfb = subprocess.Popen(["Xvfb", disp, "-screen", "0", f"{w}x{h}x24", "-nolisten", "tcp"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
    time.sleep(1.5)
    e = dict(os.environ, DISPLAY=disp)
    try:
        app = subprocess.Popen(a.app.split(), env=e, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
    except OSError as err:
        xvfb.terminate()
        sys.exit(f"No pude abrir la app ({a.app}): {err}")
    wait_ready(e, a.boot)
    raw = os.path.join(out, "raw.mkv")
    prog = os.path.join(out, "progreso.txt")
    open(prog, "w").close()
    ff = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-use_wallclock_as_timestamps", "1", "-analyzeduration", "0", "-probesize", "32", "-f", "x11grab", "-draw_mouse", os.environ.get("XT_MOUSE", "1"),
                           "-framerate", "25", "-video_size", f"{w}x{h}", "-i", disp, "-vf", "fps=25", "-vsync", "cfr",
                           "-c:v", "libx264", "-preset", "ultrafast", "-crf", "20", "-pix_fmt", "yuv420p", "-progress", prog, "-stats_period", "0.2", raw],
                          stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=e, start_new_session=True)
    # t0 = el instante en que ffmpeg empieza a capturar de verdad (el primer fotograma), no cuando aparece el archivo.
    t_first = None
    for _ in range(600):
        time.sleep(0.05)
        txt = open(prog).read()
        us = [l for l in txt.splitlines() if l.startswith("out_time_us=")]
        fr = [l for l in txt.splitlines() if l.startswith("frame=")]
        if fr and us and fr[-1] != "frame=0":
            try:
                t_first = time.time() - max(0.0, int(us[-1].split("=")[1])) / 1e6
            except ValueError:
                t_first = time.time()
            break
    if t_first is None:
        t_first = time.time()
    st = {"display": disp, "size": [int(w), int(h)], "out": out, "xvfb": xvfb.pid, "app": app.pid, "ff": ff.pid,
          "t0": t_first, "t0_wall": round(t_first % 60, 2), "events": [], "shots": []}
    save(st)
    print(f"Grabando en {disp} ({w}x{h}). Salida: {out}")


def wait_ready(e, minimum):
    """
    Espera a que la app termine de arrancar y responda: mueve el raton sobre un boton y sobre un hueco vacio y
    comprueba que la pantalla reacciona (la pista de ayuda cambia). Hace falta que reaccione 2 veces seguidas."""
    import hashlib
    def shot():
        r = subprocess.run(["import", "-window", "root", "-depth", "8", "rgb:-"], env=e, capture_output=True)
        return hashlib.md5(r.stdout).hexdigest()
    t0, ok = time.time(), 0
    while time.time() - t0 < 120:
        time.sleep(1.5)
        subprocess.run(["xdotool", "mousemove", "1093", "63"], env=e)
        time.sleep(0.8)
        a = shot()
        subprocess.run(["xdotool", "mousemove", "800", "600"], env=e)
        time.sleep(0.8)
        b = shot()
        ok = ok + 1 if a != b else 0
        if ok >= 2 and time.time() - t0 >= minimum:
            return
    print("Aviso: la app tardo mucho en responder; sigo igualmente.")


def hold(st, say, seconds):
    if say:
        log(st, "caption", text=say, dur=seconds)
        time.sleep(seconds)


def cmd_say(a):
    st = load()
    hold(st, a.text, a.seconds)


def read_time(text):
    return max(2.0, min(9.0, 1.2 + len(text) / 14.0))


def move_to(st, x, y, steps=14):
    out = subprocess.run(["xdotool", "getmouselocation"], env=env(st), capture_output=True, text=True).stdout
    try:
        cx, cy = [int(p.split(":")[1]) for p in out.split()[:2]]
    except Exception:
        cx, cy = x, y
    for i in range(1, steps + 1):
        t = i / steps
        t = t * t * (3 - 2 * t)
        xdo(st, "mousemove", round(cx + (x - cx) * t), round(cy + (y - cy) * t))
        time.sleep(0.02)


def do_click(st, x, y, button=1, double=False, say=None):
    if say:
        log(st, "caption", text=say, dur=read_time(say))
    move_to(st, x, y)
    time.sleep(0.25)
    log(st, "click", x=x, y=y, button=button)
    xdo(st, "click", "--repeat", 2 if double else 1, "--delay", 120, button)
    time.sleep(0.6)
    if say:
        time.sleep(max(0.0, read_time(say) - 1.0))


def cmd_click(a):
    do_click(load(), a.x, a.y, a.button, a.double, a.say)


def do_key(st, keys, say=None):
    if say:
        log(st, "caption", text=say, dur=read_time(say))
    log(st, "key", keys=keys)
    xdo(st, "key", *keys.split())
    time.sleep(0.7)
    if say:
        time.sleep(max(0.0, read_time(say) - 0.7))


def cmd_key(a):
    do_key(load(), a.keys, a.say)


def do_type(st, text, say=None):
    if say:
        log(st, "caption", text=say, dur=read_time(say))
    log(st, "key", keys=f'escribe "{text}"')
    xdo(st, "type", "--delay", 90, text)
    time.sleep(0.6)


def do_drag(st, x1, y1, x2, y2, say=None):
    if say:
        log(st, "caption", text=say, dur=read_time(say))
    move_to(st, x1, y1)
    time.sleep(0.2)
    log(st, "click", x=x1, y=y1, button=1)
    xdo(st, "mousedown", 1)
    move_to(st, x2, y2, steps=30)
    xdo(st, "mouseup", 1)
    log(st, "click", x=x2, y=y2, button=1)
    time.sleep(0.6)


def cmd_drag(a):
    do_drag(load(), a.x1, a.y1, a.x2, a.y2, a.say)


def do_shot(st, name, caption=None):
    n = len(st["shots"]) + 1
    path = os.path.join(st["out"], "capturas", f"{n:02d}-{name}.png")
    subprocess.run(["import", "-window", "root", path], env=env(st), check=False)
    caption = caption or next((e["text"] for e in reversed(st["events"]) if e["kind"] == "caption"), name)
    st["shots"].append({"t": round(now(st), 2), "file": os.path.relpath(path, st["out"]), "text": caption})
    save(st)
    return path


def cmd_shot(a):
    st = load()
    print(do_shot(st, a.name, a.text))


def cmd_look(a):
    st = load()
    path = os.path.join(st["out"], "look.png")
    subprocess.run(["import", "-window", "root", path], env=env(st), check=False)
    print(path)


def cmd_wait(a):
    time.sleep(a.seconds)


def cmd_run(a):
    plan = json.load(open(a.script))
    if not os.path.exists(STATE):
        class A:  # sesion nueva con los datos del guion
            app = plan.get("app", default_app()); size = plan.get("size", "1600x900"); out = plan.get("out", "tutorial_out"); boot = plan.get("boot", 8)
        cmd_start(A)
    st = load()
    for step in plan["steps"]:
        st = load()
        d, say = step.get("do"), step.get("say")
        if d == "say":
            hold(st, say, step.get("seconds", read_time(say or "")))
        elif d == "click":
            do_click(st, step["x"], step["y"], step.get("button", 1), step.get("double", False), say)
        elif d == "key":
            do_key(st, step["keys"], say)
        elif d == "type":
            do_type(st, step["text"], say)
        elif d == "drag":
            do_drag(st, step["x1"], step["y1"], step["x2"], step["y2"], say)
        elif d == "wait":
            time.sleep(step.get("seconds", 1))
        elif d == "shot":
            do_shot(st, step.get("name", "paso"), say)
        else:
            sys.exit(f"Paso desconocido: {step}")
        if step.get("shot") and d != "shot":
            do_shot(load(), step["shot"], say)
    cmd_stop(a)


# ------------------------------------------------------------------ render
def ass_time(t):
    h, rem = divmod(max(0, t), 3600)
    m, s = divmod(rem, 60)
    return f"{int(h)}:{int(m):02d}:{s:05.2f}"


def circle(r):
    import math
    pts = [(round(r + r * math.cos(2 * math.pi * i / 28)), round(r + r * math.sin(2 * math.pi * i / 28))) for i in range(28)]
    return "m " + " l ".join(f"{x} {y}" for x, y in pts)


def build_ass(st, path):
    w, h = st["size"]
    L = [f"[Script Info]\nScriptType: v4.00+\nPlayResX: {w}\nPlayResY: {h}\n",
         "[V4+ Styles]\nFormat: Name,Fontname,Fontsize,PrimaryColour,SecondaryColour,OutlineColour,BackColour,Bold,Italic,Underline,StrikeOut,ScaleX,ScaleY,Spacing,Angle,BorderStyle,Outline,Shadow,Alignment,MarginL,MarginR,MarginV,Encoding",
         f"Style: Cap,DejaVu Sans,{max(22, h // 30)},&H00FFFFFF,&H00FFFFFF,&H00000000,&HB0000000,1,0,0,0,100,100,0,0,3,6,0,2,40,40,{h // 18},1",
         "Style: Ring,DejaVu Sans,20,&H00FFFFFF,&H00FFFFFF,&H0000E5FF,&H00000000,0,0,0,0,100,100,0,0,1,3,0,7,0,0,0,1",
         f"Style: Key,DejaVu Sans,{max(18, h // 40)},&H0000E5FF,&H0000E5FF,&H00000000,&HB0000000,1,0,0,0,100,100,0,0,3,5,0,7,24,24,24,1",
         "\n[Events]\nFormat: Layer,Start,End,Style,Name,MarginL,MarginR,MarginV,Effect,Text"]
    for e in st["events"]:
        t = e["t"]
        if e["kind"] == "caption":
            L.append(f"Dialogue: 1,{ass_time(t)},{ass_time(t + e['dur'])},Cap,,0,0,0,,{e['text'].replace(chr(10), '\\N')}")
        elif e["kind"] == "key":
            L.append(f"Dialogue: 1,{ass_time(t)},{ass_time(t + 1.4)},Key,,0,0,0,,Tecla: {e['keys']}")
        elif e["kind"] == "click":
            col = "&H0000E5FF&" if e["button"] == 1 else "&H00FFA500&"
            for k, (r, dt) in enumerate([(14, 0.0), (26, 0.12), (38, 0.24)]):
                L.append(f"Dialogue: 2,{ass_time(t + dt)},{ass_time(t + dt + 0.14)},Ring,,0,0,0,,{{\\an7\\pos({e['x'] - r},{e['y'] - r})\\1a&HFF&\\3c{col}\\bord3\\p1}}{circle(r)}{{\\p0}}")
    open(path, "w").write("\n".join(L) + "\n")


def build_srt(st, path):
    caps = [e for e in st["events"] if e["kind"] == "caption"]
    def f(t):
        ms = int(t * 1000); return f"{ms // 3600000:02d}:{ms // 60000 % 60:02d}:{ms // 1000 % 60:02d},{ms % 1000:03d}"
    with open(path, "w") as fh:
        for i, e in enumerate(caps, 1):
            fh.write(f"{i}\n{f(e['t'])} --> {f(e['t'] + e['dur'])}\n{e['text']}\n\n")


def build_md(st, path, title):
    with open(path, "w") as fh:
        fh.write(f"# {title}\n\nVideo: `tutorial.mp4` · Subtitulos: `tutorial.srt`\n\n")
        for i, s in enumerate(st["shots"], 1):
            fh.write(f"## Paso {i}\n\n{s['text']}\n\n![paso {i}]({s['file']})\n\n")
        caps = [e for e in st["events"] if e["kind"] == "caption"]
        fh.write("## Guion completo\n\n")
        for e in caps:
            m, s = divmod(int(e["t"]), 60)
            fh.write(f"- `{m}:{s:02d}` {e['text']}\n")


def cmd_stop(a):
    st = load()
    time.sleep(0.8)
    try:
        os.kill(st["ff"], signal.SIGINT)
        for _ in range(30):
            time.sleep(0.1)
            try:
                os.kill(st["ff"], 0)
            except ProcessLookupError:
                break
    except ProcessLookupError:
        pass
    try:
        os.kill(st["ff"], signal.SIGTERM)
        time.sleep(0.5)
    except ProcessLookupError:
        pass
    for k in ("app", "xvfb"):
        try:
            os.kill(st[k], signal.SIGTERM)
        except ProcessLookupError:
            pass
    out = st["out"]
    json.dump(st, open(os.path.join(out, "eventos.json"), "w"), indent=1)
    build_ass(st, os.path.join(out, "rotulos.ass"))
    build_srt(st, os.path.join(out, "tutorial.srt"))
    title = getattr(a, "title", None) or "Tutorial de Xadaw"
    build_md(st, os.path.join(out, "tutorial.md"), title)
    r = subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", os.path.join(out, "raw.mkv"), "-vf", f"crop={st['size'][0]}:{st['size'][1]}:0:0,ass={os.path.join(out, 'rotulos.ass')}",
                        "-c:v", "libx264", "-crf", "21", "-pix_fmt", "yuv420p", "-movflags", "+faststart", os.path.join(out, "tutorial.mp4")])
    if r.returncode == 0 and not os.environ.get("XT_KEEP"):
        os.remove(os.path.join(out, "raw.mkv"))
        try:
            os.remove(os.path.join(out, "progreso.txt"))
        except OSError:
            pass
    shutil.rmtree(os.path.dirname(STATE), ignore_errors=True)
    print(f"Listo: {out}/tutorial.mp4  tutorial.srt  tutorial.md")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = p.add_subparsers(dest="cmd", required=True)
    s = sp.add_parser("start"); s.add_argument("--app", default=default_app()); s.add_argument("--size", default="1600x900"); s.add_argument("--out", default="tutorial_out"); s.add_argument("--boot", type=float, default=8); s.set_defaults(f=cmd_start)
    s = sp.add_parser("say"); s.add_argument("text"); s.add_argument("--seconds", type=float, default=3); s.set_defaults(f=cmd_say)
    s = sp.add_parser("click"); s.add_argument("x", type=int); s.add_argument("y", type=int); s.add_argument("--button", type=int, default=1); s.add_argument("--double", action="store_true"); s.add_argument("--say"); s.set_defaults(f=cmd_click)
    s = sp.add_parser("drag"); [s.add_argument(n, type=int) for n in ("x1", "y1", "x2", "y2")]; s.add_argument("--say"); s.set_defaults(f=cmd_drag)
    s = sp.add_parser("key"); s.add_argument("keys"); s.add_argument("--say"); s.set_defaults(f=cmd_key)
    s = sp.add_parser("shot"); s.add_argument("name", nargs="?", default="paso"); s.add_argument("--text"); s.set_defaults(f=cmd_shot)
    s = sp.add_parser("look"); s.set_defaults(f=cmd_look)
    s = sp.add_parser("wait"); s.add_argument("seconds", type=float); s.set_defaults(f=cmd_wait)
    s = sp.add_parser("run"); s.add_argument("script"); s.set_defaults(f=cmd_run)
    s = sp.add_parser("stop"); s.add_argument("--title"); s.set_defaults(f=cmd_stop)
    a = p.parse_args()
    a.f(a)


if __name__ == "__main__":
    main()
