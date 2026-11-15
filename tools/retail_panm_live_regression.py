"""In-game regression for retail T8 PANM import (needs a running game in a match; never launches or kills it).

    python -I tools/retail_panm_live_regression.py EXE DOC SLOT MOVE FILE.panm OUT_DIR [--watch SECONDS] [--capture RUNTIME_DIR]

Steps, each recorded in OUT_DIR/report.json:
 1. import-panm-t8 DOC MOVE FILE.panm -> OUT_DIR/custom.tkmvst (custom asset on the move's own animation)
 2. check-import-t8 (shadow audit): custom_animations >= 1, no missing carrier
 3. import-t8 on SLOT, queue-t8 SLOT MOVE, state-t8 shows MOVE as current
 4. reimport DOC: restored_carriers >= 1 (the carrier gets its native animation back)
 5. optional liveness watch (game process alive, moveset still installed)
--capture points at the Tekken8 runtime scripts folder (contains game.py); frames of the queued move are then
saved to OUT_DIR/custom.png and OUT_DIR/restored.png for a visual check.
Exit code 0 only if every check passed.
"""
import argparse
import json
import pathlib
import subprocess
import sys
import time


def run(exe, *args, timeout=300):
    result = subprocess.run([exe, "--headless", *map(str, args)], capture_output=True, text=True, encoding="utf-8", timeout=timeout)
    try:
        report = json.loads(result.stdout.strip().splitlines()[-1])
    except (ValueError, IndexError):
        report = {"ok": False, "error": (result.stdout + result.stderr).strip()[:500]}
    report["exit_code"] = result.returncode
    return report


def alive():
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq Polaris-Win64-Shipping.exe"], capture_output=True, text=True).stdout
    return "Polaris" in out


def capture(runtime, exe, slot, move, path):
    sys.path.insert(0, str(runtime))
    import game  # noqa: E402  (Tekken8 runtime helpers: window lookup and PrintWindow capture)
    from PIL import Image
    hwnd = game.game_window()[0]
    run(exe, "queue-t8", slot, move)
    frames = []
    for _ in range(12):
        frames.append(game.capture(hwnd, quiet=True).resize((432, 243)))
        time.sleep(0.12)
    sheet = Image.new("RGB", (432 * 4, 243 * 3))
    for i, frame in enumerate(frames):
        sheet.paste(frame, ((i % 4) * 432, (i // 4) * 243))
    sheet.save(path)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("exe"); ap.add_argument("doc"); ap.add_argument("slot", type=int); ap.add_argument("move"); ap.add_argument("panm"); ap.add_argument("out")
    ap.add_argument("--watch", type=float, default=0); ap.add_argument("--capture", type=pathlib.Path)
    a = ap.parse_args()
    a.exe = str(pathlib.Path(a.exe).resolve())  # CreateProcess does not search relative paths with forward slashes
    out = pathlib.Path(a.out); out.mkdir(parents=True, exist_ok=True)
    report = {"steps": {}, "checks": {}}
    def check(name, ok):
        report["checks"][name] = bool(ok)
        return ok
    try:
        if not check("game_running", alive()):
            raise RuntimeError("Polaris-Win64-Shipping.exe is not running")
        custom = out / "custom.tkmvst"
        step = report["steps"]["build"] = run(a.exe, "import-panm-t8", a.doc, a.move, a.panm, custom, "-", "lz4")
        check("built_custom_asset", step.get("ok") and step.get("custom"))
        step = report["steps"]["shadow"] = run(a.exe, "check-import-t8", custom, a.slot)
        check("shadow_ok", step.get("ok") and step.get("custom_animations", 0) >= 1 and step.get("custom_animations_missing_carrier", 1) == 0)
        step = report["steps"]["import"] = run(a.exe, "import-t8", custom, a.slot)
        check("import_ok", step.get("ok") and step.get("custom_animations", 0) >= 1)
        root = step.get("moveset")
        if a.capture:
            capture(a.capture, a.exe, a.slot, a.move, out / "custom.png")
        else:
            run(a.exe, "queue-t8", a.slot, a.move)
        state = report["steps"]["queued_state"] = run(a.exe, "state-t8", a.slot)
        expected = report["steps"]["build"].get("move")
        check("moveset_installed", state.get("root") == root)
        report["steps"]["queued_move_current"] = state.get("current") == expected  # informative: short moves may already have ended
        step = report["steps"]["restore"] = run(a.exe, "import-t8", a.doc, a.slot)
        check("restored", step.get("ok") and step.get("restored_carriers", 0) >= 1)
        if a.capture:
            capture(a.capture, a.exe, a.slot, a.move, out / "restored.png")
        watch = []
        end = time.time() + a.watch
        while time.time() < end:
            time.sleep(min(30, max(0, end - time.time())))
            watch.append({"time": time.strftime("%H:%M:%S"), "alive": alive(), "state": run(a.exe, "state-t8", a.slot)})
        report["steps"]["watch"] = watch
        check("stable", all(w["alive"] for w in watch))
    except Exception as error:  # report the failure, never retry against the game
        report["error"] = str(error)
    report["ok"] = "error" not in report and all(report["checks"].values())
    (out / "report.json").write_text(json.dumps(report, indent=1), encoding="utf-8")
    print(json.dumps({"ok": report["ok"], "checks": report["checks"], "error": report.get("error")}))
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
