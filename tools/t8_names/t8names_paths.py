"""Every location the T8 name tools use. Nothing else in this folder holds a machine path.

The code (this folder) is read-only. Everything the tools read from a game copy or write as results lives in a
workspace folder that you choose:

    T8_NAMES_WORKSPACE   required. The workspace root. Created by bootstrap_workspace.py.

    <workspace>/game/movenames/     <code>.csv and ALL_movenames_retail.csv (motbin_retail.py)
    <workspace>/game/movesets/      <code>.json, full move tables (motbin_retail_full.py); only the judge needs them
    <workspace>/                    seed tables of the first recovery rounds (known_anim_names.csv, ...)
    <workspace>/recovery/           accepted.csv, remaining_unresolved.csv, accepted_*_overlay*.csv, label corrections
    <workspace>/recovery/method_lab/  working files: typed_corpus/, keyed_runs/, caches, result tables
    <workspace>/merged/             output of merge_t8_names.py

Optional overrides (each replaces one default above or one tool):

    T8_NAMES_MOVENAMES   folder of the move-name tables
    T8_NAMES_MOVESETS    folder of the full move tables
    T8_NAMES_LEGACY      folder of older-game extracts (TekkenMovesetExtractor "extracted_chars": t7_*/..., tag2_*/...).
                         Default <workspace>/legacy_extracts. Without it the T7 part of the corpus is empty and the
                         judge has no older-game evidence.
    T8_NAMES_CC          C compiler for the search programs. Default: "gcc" from PATH.
"""
import os
import shutil
from pathlib import Path

SRC = Path(__file__).resolve().parent


def _workspace():
    value = os.environ.get('T8_NAMES_WORKSPACE')
    if not value:
        raise SystemExit('Set T8_NAMES_WORKSPACE to your workspace folder (see tools/t8_names/README.md).')
    return Path(value).resolve()


def _folder(variable, default):
    value = os.environ.get(variable)
    return Path(value).resolve() if value else default


WORKSPACE = _workspace()
HANDOFF = WORKSPACE
RECOVERY = WORKSPACE/'recovery'
LAB = RECOVERY/'method_lab'
MOVENAMES = _folder('T8_NAMES_MOVENAMES', WORKSPACE/'game'/'movenames')
MOVESETS = _folder('T8_NAMES_MOVESETS', WORKSPACE/'game'/'movesets')
LEGACY = _folder('T8_NAMES_LEGACY', WORKSPACE/'legacy_extracts')
CC = os.environ.get('T8_NAMES_CC') or shutil.which('gcc') or 'gcc'


def shipped(name):
    """A file that ships with the code (a trained model); a copy in the working folder wins, so it can be refitted."""
    local = LAB/name
    return local if local.exists() else SRC/name
