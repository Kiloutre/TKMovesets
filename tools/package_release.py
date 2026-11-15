#!/usr/bin/env python3
"""Build, test and package a TKMovesets2 release.

Run from anywhere with the system Python (stdlib only):

    python tools/package_release.py               # configure, build, CTest, package, verify
    python tools/package_release.py --skip-build  # reuse an existing build dir (still runs CTest)

Steps
 1. Configure two build dirs from the same sources (x64-release preset, vcpkg
    toolchain, static triplet) inside a vcvars 14.44 environment, if they are not
    configured yet:
      out/build/release-<version>-tests  BUILD_TESTING=ON, used only for CTest;
      out/build/release-<version>        BUILD_TESTING=OFF, the shipped binaries.
    BUILD_TESTING=ON compiles the test sources into TKMovesets2.exe (with their
    absolute __FILE__ paths), so the tested exe is never the shipped one. Both
    link with /PDBALTPATH:%_PDB% so no absolute PDB path is embedded.
 2. Build both.
 3. Run CTest in the test build. Any failure aborts before anything is packaged.
 4. Assemble out/releases/TKMovesets2-<version>/ from an explicit allow-list:
    TKMovesets2.exe, TKMovesetLoader.dll, game_addresses.ini, TKMInterface label
    and name files, README.txt (tools/release-notes/README.txt), LICENSE.txt,
    THIRD_PARTY_NOTICES.txt (vcpkg copyright files + vendored code) and
    SHA256SUMS.txt. Then zip it.
 5. Check the package: allow-list of names, no PDB/test/settings/moveset files,
    no DLL imports beyond Windows system DLLs, and no local paths, user names,
    e-mail addresses or Steam IDs in any file (ASCII and UTF-16).
 6. Smoke-test the zip: extract to a fresh temp folder and run the packaged exe
    headless (--help, ui-smoke, convert-t8-full on a sample T7 file, validate on
    the result) with no TKMSettings.ini present. Optional --gui-check starts the
    GUI and reads its window title (refused while Tekken is running).

The script never touches TKMSettings.ini, .dev data or the running game, and
never writes outside the build dir, out/releases and a temp folder.
"""

from __future__ import annotations

import argparse
import ctypes
import getpass
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SRC = REPO / "TKMovesets2"
RESOURCES = SRC / "Resources"

VS_ROOT_DEFAULT = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community")
VCVARS_VER = "14.44"
VCPKG_TOOLCHAIN_DEFAULT = Path(os.environ.get("LOCALAPPDATA", r"C:\Users\Default\AppData\Local")) / \
    "Temp/tkmovesets-modern-vcpkg/scripts/buildsystems/vcpkg.cmake"
VCPKG_INSTALLED_DEFAULT = REPO / "out/build/modern/vcpkg_installed"
TRIPLET = "x64-windows-static"

DEFAULT_SAMPLE = REPO / ".dev/chars/t7_tkm_extracted_chars_full_mota/T7_Mr.X.tkmvst"

# Files the application reads at runtime, relative to the package root.
# Localization files are embedded in the exe (create_resources in TKMovesets2/CMakeLists.txt).
INTERFACE_PATTERNS = (re.compile(r"^editor_[A-Za-z0-9-]+_[a-z_]+\.txt$"), re.compile(r"^[A-Za-z0-9-]+_names\.json$"))

FORBIDDEN_NAME = re.compile(
    r"(TKMSettings\.ini|\.tkmvst$|\.panm$|\.pdb$|\.ilk$|\.lib$|\.exp$|\.obj$|\.log$|"
    r"MovesetRegressionTests|FileEmbedder|anims\.txt$|animations\.json$|TKM-DEBUG)", re.I)

# Windows system DLLs the release binaries may import (everything else is linked statically).
SYSTEM_DLLS = {
    "kernel32.dll", "user32.dll", "gdi32.dll", "shell32.dll", "ole32.dll", "oleaut32.dll", "advapi32.dll",
    "comdlg32.dll", "ws2_32.dll", "crypt32.dll", "bcrypt.dll", "wldap32.dll", "normaliz.dll", "secur32.dll",
    "iphlpapi.dll", "opengl32.dll", "dwmapi.dll", "imm32.dll", "shlwapi.dll", "version.dll", "winmm.dll",
    "psapi.dll", "dbghelp.dll", "ntdll.dll", "uuid.dll", "comctl32.dll", "setupapi.dll", "userenv.dll",
    "winhttp.dll", "wininet.dll", "rpcrt4.dll", "shcore.dll", "uxtheme.dll", "hid.dll", "xinput1_4.dll",
    "cfgmgr32.dll",
}

TEKKEN_PROCESSES = ("Polaris-Win64-Shipping.exe", "TekkenGame-Win64-Shipping.exe")


class ReleaseError(RuntimeError):
    pass


def log(message: str) -> None:
    print(f"[package] {message}", flush=True)


def program_version() -> str:
    text = (SRC / "Utils/constants.h").read_text(encoding="utf-8", errors="replace")
    match = re.search(r'#\s*define\s+PROGRAM_VERSION\s+"([^"]+)"', text)
    if not match:
        raise ReleaseError("PROGRAM_VERSION not found in Utils/constants.h")
    return match.group(1)


# --------------------------------------------------------------------------- build


def run_in_vcvars(args: argparse.Namespace, commands: list[str], log_file: Path) -> None:
    """Run commands in one cmd.exe session after vcvars64.bat, teeing output to log_file."""
    vcvars = args.vs_root / "VC/Auxiliary/Build/vcvars64.bat"
    ninja_dir = args.vs_root / "Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja"
    if not vcvars.exists():
        raise ReleaseError(f"vcvars64.bat not found under {args.vs_root}")
    lines = ["@chcp 65001 >nul", "@set VSLANG=1033",
             f'@call "{vcvars}" -vcvars_ver={VCVARS_VER} >nul || exit /b 1',
             f'@set "PATH={ninja_dir};%PATH%"',
             f'@cd /d "{REPO}"']
    lines += [f"{command} || exit /b 1" for command in commands]
    script = log_file.with_suffix(".cmd")
    script.write_text("\r\n".join(lines) + "\r\n", encoding="utf-8")
    with log_file.open("w", encoding="utf-8", errors="replace") as out:
        process = subprocess.Popen(["cmd.exe", "/d", "/c", str(script)], stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace")
        assert process.stdout
        for line in process.stdout:
            out.write(line)
            if args.verbose:
                sys.stdout.write(line)
        code = process.wait()
    if code != 0:
        tail = log_file.read_text(encoding="utf-8", errors="replace").splitlines()[-40:]
        raise ReleaseError(f"command failed (exit {code}), see {log_file}:\n" + "\n".join(tail))


def cmake_exe(args: argparse.Namespace) -> Path:
    return args.vs_root / "Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"


def configure_and_build(args: argparse.Namespace, build_dir: Path, testing: bool) -> None:
    cmake = cmake_exe(args)
    commands = []
    # %% survives cmd.exe; the linker then sees %_PDB% and records only the PDB file name.
    pdb_flags = "/INCREMENTAL:NO /PDBALTPATH:%%_PDB%%"
    if not (build_dir / "build.ninja").exists():
        if not args.vcpkg_toolchain.exists():
            raise ReleaseError(f"vcpkg toolchain not found: {args.vcpkg_toolchain}")
        log(f"configuring {build_dir}")
        commands.append(
            f'"{cmake}" --preset x64-release -B "{build_dir.as_posix()}"'
            f' -DCMAKE_TOOLCHAIN_FILE="{args.vcpkg_toolchain.as_posix()}"'
            f' -DVCPKG_INSTALLED_DIR="{args.vcpkg_installed.as_posix()}"'
            f" -DVCPKG_TARGET_TRIPLET={TRIPLET} -DVCPKG_MANIFEST_INSTALL=OFF"
            f" -DBUILD_TESTING={'ON' if testing else 'OFF'}"
            f' -DCMAKE_EXE_LINKER_FLAGS_RELEASE="{pdb_flags}" -DCMAKE_SHARED_LINKER_FLAGS_RELEASE="{pdb_flags}"')
    log("building (this takes a while; --verbose streams the output)")
    commands.append(f'"{cmake}" --build "{build_dir.as_posix()}"')
    run_in_vcvars(args, commands, build_dir.parent / f"{build_dir.name}-build.log")
    cache = (build_dir / "CMakeCache.txt").read_text(encoding="utf-8", errors="replace")
    if "TKM_EXECUTABLE_NAME" in cache:
        raise ReleaseError("build dir sets TKM_EXECUTABLE_NAME; the release exe must be TKMovesets2.exe")
    if not re.search(r"^CMAKE_TKM_BUILD_TYPE:\w+=Release$", cache, re.M):
        raise ReleaseError("build dir is not a release (CMAKE_TKM_BUILD_TYPE != Release)")
    expected = "ON" if testing else "OFF"
    if not re.search(rf"^BUILD_TESTING:\w+={expected}$", cache, re.M):
        raise ReleaseError(f"{build_dir.name}: BUILD_TESTING is not {expected}")
    if "PDBALTPATH" not in cache:
        raise ReleaseError(f"{build_dir.name}: configured without /PDBALTPATH (delete the dir to reconfigure)")


def run_ctest(args: argparse.Namespace, build_dir: Path) -> str:
    ctest = cmake_exe(args).with_name("ctest.exe")
    log_file = build_dir.parent / f"{build_dir.name}-ctest.log"
    log("running CTest")
    run_in_vcvars(args, [f'"{ctest}" --test-dir "{build_dir.as_posix()}" --output-on-failure --timeout 1800'],
                  log_file)
    text = log_file.read_text(encoding="utf-8", errors="replace")
    summary = re.search(r"\d+% tests passed, \d+ tests failed out of \d+", text)
    if not summary or " 0 tests failed" not in summary.group(0):
        raise ReleaseError(f"CTest did not pass, see {log_file}")
    log(summary.group(0))
    return summary.group(0)


# --------------------------------------------------------------------------- assemble


def third_party_notices(vcpkg_installed: Path) -> str:
    info = vcpkg_installed / "vcpkg/info"
    share = vcpkg_installed / TRIPLET / "share"
    ports = []
    for entry in sorted(info.glob(f"*_{TRIPLET}.list")):
        name, version = entry.name[: -len(f"_{TRIPLET}.list")].rsplit("_", 1)
        ports.append((name, version))
    if not ports:
        raise ReleaseError(f"no {TRIPLET} ports listed in {info}")

    parts = [
        "Third-party notices for TKMovesets2",
        "===================================",
        "",
        "TKMovesets2 is licensed under the GNU GPL v3 (LICENSE.txt). TKMovesets2.exe and",
        "TKMovesetLoader.dll statically link the libraries below (vcpkg triplet",
        f"{TRIPLET}) and include the vendored sources listed at the end. Their",
        "license texts follow, as installed by vcpkg or shipped with the sources.",
        "",
        "Libraries:",
    ]
    parts += [f"  - {name} {version}" for name, version in ports]
    parts += ["  - imgui_markdown (vendored, imgui_markdown/)", "  - Sig by HoShiMin (vendored, Sig/)",
              "  - Steamworks SDK headers (compiled in; no Valve binaries are distributed)", ""]
    for name, version in ports:
        copyright_file = share / name / "copyright"
        if not copyright_file.exists():
            raise ReleaseError(f"missing copyright file for {name}: {copyright_file}")
        parts += ["", "=" * 78, f"{name} {version}", "=" * 78, "",
                  copyright_file.read_text(encoding="utf-8", errors="replace").strip(), ""]
    for title, path in (("imgui_markdown", REPO / "imgui_markdown/License.txt"), ("Sig", REPO / "Sig/LICENSE")):
        parts += ["", "=" * 78, title, "=" * 78, "", path.read_text(encoding="utf-8", errors="replace").strip(), ""]
    parts += ["", "=" * 78, "Steamworks SDK", "=" * 78, "",
              "Steamworks SDK headers (steam_api) are used to call the Steam API already loaded by the game.",
              "They are used under the Steamworks SDK Access Agreement. No Valve binary is distributed.", ""]
    return "\n".join(parts).replace("\r\n", "\n")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for block in iter(lambda: file.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def assemble(args: argparse.Namespace, build_dir: Path, package: Path) -> list[Path]:
    bin_dir = build_dir / "TKMovesets2"
    exe, dll = bin_dir / "TKMovesets2.exe", bin_dir / "TKMovesetLoader.dll"
    for path in (exe, dll):
        if not path.exists():
            raise ReleaseError(f"build output missing: {path}")

    if package.exists():
        if package.parent != REPO / "out/releases":
            raise ReleaseError(f"refusing to delete {package}")
        shutil.rmtree(package)
    (package / "TKMInterface").mkdir(parents=True)

    shutil.copy2(exe, package / "TKMovesets2.exe")
    shutil.copy2(dll, package / "TKMovesetLoader.dll")
    # The exe recreates the loader DLL when it is older than the exe (Online.cpp); keep them equal.
    stamp = os.stat(package / "TKMovesets2.exe").st_mtime
    os.utime(package / "TKMovesetLoader.dll", (stamp, stamp))
    shutil.copyfile(RESOURCES / "game_addresses.ini", package / "game_addresses.ini")

    interface = sorted(p for p in (RESOURCES / "TKMInterface").iterdir()
                       if p.is_file() and any(rx.match(p.name) for rx in INTERFACE_PATTERNS))
    unknown = sorted(p.name for p in (RESOURCES / "TKMInterface").iterdir() if p not in interface)
    if unknown:
        raise ReleaseError(f"unexpected files in Resources/TKMInterface (update INTERFACE_PATTERNS?): {unknown}")
    for path in interface:
        shutil.copyfile(path, package / "TKMInterface" / path.name)
        built = bin_dir / "TKMInterface" / path.name
        if not built.exists() or sha256(built) != sha256(path):
            raise ReleaseError(f"build dir copy of {path.name} differs from the source (stale build?)")

    shutil.copyfile(REPO / "tools/release-notes/README.txt", package / "README.txt")
    shutil.copyfile(REPO / "LICENSE.txt", package / "LICENSE.txt")
    (package / "THIRD_PARTY_NOTICES.txt").write_text(third_party_notices(args.vcpkg_installed), encoding="utf-8",
                                                     newline="\r\n")

    files = sorted(p for p in package.rglob("*") if p.is_file())
    sums = "".join(f"{sha256(p)} *{p.relative_to(package).as_posix()}\n" for p in files)
    (package / "SHA256SUMS.txt").write_text(sums, encoding="ascii", newline="\n")
    return sorted(p for p in package.rglob("*") if p.is_file())


def make_zip(package: Path) -> Path:
    archive = package.parent / f"{package.name}.zip"  # not with_suffix: "1.0" would lose ".0"
    if archive.exists():
        archive.unlink()
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for path in sorted(p for p in package.rglob("*") if p.is_file()):
            zf.write(path, f"{package.name}/{path.relative_to(package).as_posix()}")
    return archive


# --------------------------------------------------------------------------- checks


def identity_terms() -> list[str]:
    """Local identity strings to look for, read at run time so none is written into this file."""
    terms = set()
    for value in (getpass.getuser(), os.environ.get("USERNAME", ""), Path.home().name):
        if len(value) >= 3:
            terms.add(value)
    for key in ("user.name", "user.email"):
        try:
            value = subprocess.run(["git", "-c", "safe.directory=*", "-C", str(REPO), "config", key],
                                   capture_output=True, text=True, timeout=30).stdout.strip()
        except OSError:
            value = ""
        if len(value) >= 3:
            terms.add(value)
            if "@" in value:
                terms.add(value.split("@")[0])
    return sorted(terms)


# Matches that are expected and public: the project's GitHub repository (updater URL).
ALLOWED_CONTEXT = (re.compile(rb"github\.com/[^/]+/TKMovesets", re.I),
                   re.compile(rb"raw\.githubusercontent\.com", re.I))


def scan_file(path: Path, terms: list[str]) -> list[str]:
    data = path.read_bytes()
    text_like = path.suffix.lower() in (".txt", ".ini", ".json")
    patterns: list[tuple[str, re.Pattern[bytes]]] = [
        # Any drive path except the system font folder the GUI loads (C:/Windows/Fonts/).
        # Either a bare NUL-delimited "G:/" string literal, or a drive letter that starts a token and is followed by
        # a path name (two word characters first: "G:\Dev", "C:\Users"). Random "x:/" bytes in code or compressed
        # data match neither (the embedded loader DLL's compressed bytes once held "H:\9 iE").
        ("drive path", re.compile(
            rb"(?<=\x00)[A-Z]:[\\/]{1,2}(?=\x00)"
            rb"|(?<![\x21-\x7e])[A-Z]:[\\/]{1,2}(?!Windows[\\/]Fonts)[A-Za-z0-9_]{2}[\w .()+-]{1,}", re.I)),
        ("user profile", re.compile(rb"\\Users\\|/Users/", re.I)),
        ("steam id", re.compile(rb"7656119\d{10}")),
        ("pdb path", re.compile(rb"[\w.-]*[\\/][\w\\/:.-]*\.pdb\b", re.I)),
    ]
    for term in terms:
        patterns.append((f"identity '{term[:3]}...'", re.compile(re.escape(term.encode()), re.I)))
    if text_like:
        patterns.append(("e-mail", re.compile(rb"[\w.+-]+@[\w-]+\.[A-Za-z]{2,}")))
    findings = []
    views = [("ascii", data)]
    if not text_like:
        # Collapse UTF-16LE text runs to ASCII so wide strings are scanned too.
        views.append(("utf16", re.sub(rb"([\x20-\x7e])\x00", rb"\1", data)))
    # In a PE image, a 4-byte-aligned bare "X:\" is usually an image-relative address whose bytes spell it
    # (0x005C3A58 = 58 3A 5C 00); such a value inside SizeOfImage is not a path. Named paths are still caught.
    image_size = 0
    if data[:2] == b"MZ" and len(data) > 0x40:
        pe = int.from_bytes(data[0x3c:0x40], "little")
        if data[pe:pe + 4] == b"PE\x00\x00":
            image_size = int.from_bytes(data[pe + 24 + 56:pe + 24 + 60], "little")  # OptionalHeader.SizeOfImage
    def image_address(view_name: str, match: re.Match[bytes]) -> bool:
        if view_name != "ascii" or not image_size or match.start() % 4 or match.end() - match.start() > 4:
            return False
        value = int.from_bytes(data[match.start():match.start() + 4], "little")
        return 0x1000 <= value < image_size
    for view_name, view in views:
        for label, rx in patterns:
            for match in rx.finditer(view):
                if label == "drive path" and image_address(view_name, match):
                    continue
                window = view[max(0, match.start() - 40): match.end() + 40]
                if any(allowed.search(window) for allowed in ALLOWED_CONTEXT) and label.startswith("identity"):
                    continue
                if label == "e-mail" and path.name == "THIRD_PARTY_NOTICES.txt":
                    continue  # upstream authors' published contact addresses in license texts
                findings.append(f"{path.name}: {label} ({view_name}) at 0x{match.start():x}: "
                                f"{window.decode('ascii', 'replace')!r}")
                break
    return findings


def dll_imports(args: argparse.Namespace, binary: Path, scratch: Path) -> list[str]:
    dumpbin_log = scratch / f"dumpbin-{binary.stem}.log"
    run_in_vcvars(args, [f'dumpbin /nologo /dependents "{binary}"'], dumpbin_log)
    text = dumpbin_log.read_text(encoding="utf-8", errors="replace")
    return sorted({m.lower() for m in re.findall(r"^\s+(\S+\.dll)\s*$", text, re.M | re.I)})


def check_package(args: argparse.Namespace, package: Path, files: list[Path], scratch: Path) -> dict:
    results: dict = {"files": [], "problems": []}
    allowed_top = {"TKMovesets2.exe", "TKMovesetLoader.dll", "game_addresses.ini", "README.txt", "LICENSE.txt",
                   "THIRD_PARTY_NOTICES.txt", "SHA256SUMS.txt"}
    for path in files:
        rel = path.relative_to(package).as_posix()
        results["files"].append({"path": rel, "bytes": path.stat().st_size})
        top_ok = rel in allowed_top
        interface_ok = rel.startswith("TKMInterface/") and rel.count("/") == 1 and \
            any(rx.match(path.name) for rx in INTERFACE_PATTERNS)
        if not (top_ok or interface_ok):
            results["problems"].append(f"not in allow-list: {rel}")
        if FORBIDDEN_NAME.search(rel):
            results["problems"].append(f"forbidden file: {rel}")
    # The GUI title is the literal PROGRAM_TITLE " " PROGRAM_VERSION (run_gui.cpp); a " - DEBUG" suffix means a debug build.
    constants = (SRC / "Utils/constants.h").read_text(encoding="utf-8", errors="replace")
    title = re.search(r'#\s*define\s+PROGRAM_TITLE\s+"([^"]+)"', constants).group(1) + " " + program_version()
    exe_bytes = (package / "TKMovesets2.exe").read_bytes()
    results["window_title_in_exe"] = title if (title + "\0").encode() in exe_bytes else None
    if results["window_title_in_exe"] is None:
        results["problems"].append(f"exe does not contain the release window title {title!r}")
    if (title + " - DEBUG").encode() in exe_bytes:
        results["problems"].append("exe contains the debug window title")
    terms = identity_terms()
    results["identity_terms_checked"] = len(terms)
    for path in files:
        results["problems"] += scan_file(path, terms)
    for name in ("TKMovesets2.exe", "TKMovesetLoader.dll"):
        imports = dll_imports(args, package / name, scratch)
        results[f"{name} imports"] = imports
        extra = [dll for dll in imports if dll not in SYSTEM_DLLS and not dll.startswith("api-ms-win-")]
        if extra:
            results["problems"].append(f"{name} imports non-system DLLs: {extra}")
    return results


def run_exe(exe: Path, arguments: list[str], cwd: Path, timeout: int = 600) -> dict:
    started = time.time()
    process = subprocess.run([str(exe), "--headless", *arguments], cwd=cwd, capture_output=True, timeout=timeout)
    output = process.stdout.decode("utf-8", "replace")
    return {"args": arguments, "exit": process.returncode, "seconds": round(time.time() - started, 1),
            "stdout": output, "stderr": process.stderr.decode("utf-8", "replace")[-1500:]}


def running_processes() -> set[str]:
    output = subprocess.run(["tasklist", "/fo", "csv", "/nh"], capture_output=True, text=True).stdout
    return {line.split('","')[0].strip('"').lower() for line in output.splitlines() if line}


def window_title(pid: int, timeout: float) -> str:
    user32 = ctypes.windll.user32
    found: list[str] = []
    callback_type = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)

    def callback(hwnd, _):
        owner = ctypes.c_ulong()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user32.IsWindowVisible(hwnd):
            length = user32.GetWindowTextLengthW(hwnd)
            buffer = ctypes.create_unicode_buffer(length + 1)
            user32.GetWindowTextW(hwnd, buffer, length + 1)
            if buffer.value:
                found.append(buffer.value)
        return True

    deadline = time.time() + timeout
    while time.time() < deadline and not found:
        user32.EnumWindows(callback_type(callback), 0)
        time.sleep(0.5)
    return found[0] if found else ""


def smoke_test(args: argparse.Namespace, archive: Path, package_name: str, version: str) -> dict:
    results: dict = {"runs": [], "problems": []}
    root = Path(tempfile.mkdtemp(prefix="tkm-release-check-"))
    try:
        with zipfile.ZipFile(archive) as zf:
            if zf.testzip() is not None:
                results["problems"].append("zip CRC check failed")
            zf.extractall(root)
        app = root / package_name
        exe = app / "TKMovesets2.exe"
        if (app / "TKMSettings.ini").exists():
            results["problems"].append("TKMSettings.ini present in the extracted package")
        # Checksums as a user would verify them.
        for line in (app / "SHA256SUMS.txt").read_text(encoding="ascii").splitlines():
            digest, name = line.split(" *", 1)
            if sha256(app / name) != digest:
                results["problems"].append(f"SHA256 mismatch: {name}")
        before = {p.relative_to(app).as_posix() for p in app.rglob("*")}
        work = root / "work"
        work.mkdir()

        runs = [run_exe(exe, ["--help"], work), run_exe(exe, ["ui-smoke"], work)]
        if args.sample and args.sample.exists():
            sample = work / "T7_sample.tkmvst"
            shutil.copyfile(args.sample, sample)  # temp copy; never packaged
            runs.append(run_exe(exe, ["validate", str(sample)], work))
            runs.append(run_exe(exe, ["convert-t8-full", str(sample), str(work / "T8_sample.tkmvst"), "lz4"], work))
            runs.append(run_exe(exe, ["validate", str(work / "T8_sample.tkmvst")], work))
        else:
            results["problems"].append(f"sample T7 file not found ({args.sample}); conversion not exercised")
        for run in runs:
            ok = run["exit"] == 0
            if run["args"][0] == "--help":
                ok = ok and "convert-t8-full" in run["stdout"]
            if run["args"][0] == "ui-smoke":
                run["stdout"] = run["stdout"][-400:]
            if not ok:
                results["problems"].append(f"headless {run['args'][0]} failed (exit {run['exit']})")
            # checked on the full output above (the help text outgrew the 1500-character tail); trimmed for the report
            run["stdout"] = run["stdout"][-1500:]
            run["args"] = [a.replace(str(root), "<tmp>") for a in run["args"]]
            run["stdout"] = run["stdout"].replace(str(root), "<tmp>").replace(str(root).replace("\\", "\\\\"), "<tmp>")
            results["runs"].append(run)

        if args.gui_check:
            busy = {p.lower() for p in TEKKEN_PROCESSES} & running_processes()
            if busy:
                results["gui"] = {"skipped": f"Tekken is running ({sorted(busy)})"}
            else:
                process = subprocess.Popen([str(exe)], cwd=app)
                try:
                    title = window_title(process.pid, 30)
                    alive = process.poll() is None
                finally:
                    process.terminate()
                    process.wait(15)
                results["gui"] = {"title": title, "alive_after_title": alive}
                if not title.endswith(f" {version}"):
                    results["problems"].append(f"window title {title!r} does not report {version}")
        after = {p.relative_to(app).as_posix() for p in app.rglob("*")}
        results["files_created_in_package_dir"] = sorted(after - before)
    finally:
        shutil.rmtree(root, ignore_errors=True)
    return results


# --------------------------------------------------------------------------- main


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--build-dir", type=Path, help="default: out/build/release-<version>")
    parser.add_argument("--skip-build", action="store_true", help="do not configure/build (CTest still runs)")
    parser.add_argument("--skip-tests", action="store_true", help="skip CTest (dry runs only, never for a release)")
    parser.add_argument("--no-smoke", action="store_true", help="skip running the packaged exe")
    parser.add_argument("--gui-check", action="store_true", help="also start the GUI and read its window title")
    parser.add_argument("--sample", type=Path, default=DEFAULT_SAMPLE, help="T7 .tkmvst used for the conversion smoke test")
    parser.add_argument("--vs-root", type=Path, default=VS_ROOT_DEFAULT)
    parser.add_argument("--vcpkg-toolchain", type=Path, default=VCPKG_TOOLCHAIN_DEFAULT)
    parser.add_argument("--vcpkg-installed", type=Path, default=VCPKG_INSTALLED_DEFAULT)
    parser.add_argument("--report", type=Path, help="write the JSON report here (default: next to the zip)")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()
    for stream in (sys.stdout, sys.stderr):
        stream.reconfigure(encoding="utf-8", errors="replace")

    version = program_version()
    build_dir = (args.build_dir or REPO / f"out/build/release-{version}").resolve()
    test_dir = build_dir.with_name(build_dir.name + "-tests")
    package = REPO / "out/releases" / f"TKMovesets2-{version}"
    report: dict = {"version": version, "build_dir": build_dir.relative_to(REPO).as_posix()
                    if build_dir.is_relative_to(REPO) else "<external>"}
    try:
        commit = subprocess.run(["git", "-c", "safe.directory=*", "-C", str(REPO), "rev-parse", "HEAD"],
                                capture_output=True, text=True).stdout.strip()
        dirty = subprocess.run(["git", "-c", "safe.directory=*", "-C", str(REPO), "status", "--porcelain",
                                "--untracked-files=no"], capture_output=True, text=True).stdout.strip()
        report["commit"] = commit + (" (+ uncommitted changes)" if dirty else "")
        for path, testing in ((test_dir, True), (build_dir, False)):
            if testing and args.skip_tests:
                continue
            path.mkdir(parents=True, exist_ok=True)
            if not args.skip_build:
                configure_and_build(args, path, testing)
        report["ctest"] = "skipped" if args.skip_tests else run_ctest(args, test_dir)
        log(f"assembling {package}")
        files = assemble(args, build_dir, package)
        archive = make_zip(package)
        report["zip"] = {"name": archive.name, "bytes": archive.stat().st_size, "sha256": sha256(archive)}
        with tempfile.TemporaryDirectory(prefix="tkm-release-scan-") as scratch:
            report["package"] = check_package(args, package, files, Path(scratch))
        if not args.no_smoke:
            log("smoke-testing the extracted zip")
            report["smoke"] = smoke_test(args, archive, package.name, version)
    except ReleaseError as error:
        log(f"FAILED: {error}")
        return 1
    problems = report.get("package", {}).get("problems", []) + report.get("smoke", {}).get("problems", [])
    report["ok"] = not problems
    report_path = args.report or package.parent / f"{package.name}-report.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    for problem in problems:
        log(f"PROBLEM: {problem}")
    log(f"{'OK' if not problems else 'NOT OK'}: {package.relative_to(REPO)} + zip, report {report_path.name}")
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
