# Tekken 8 retail name recovery tools

Retail Tekken 8 stores no move or animation names, only 32-bit hashes of them. These tools recover names from the
hashes and produce `TKMovesets2/Resources/TKMInterface/t8-retail_names.json`. They are a copy of a research lab,
made location-independent; they are for developers who extend the name table (new game build, new character), not
for users of TKMovesets2.

Read this file first. The scripts do nothing useful without the input data described below.

## What the game gives us

Every move record of a retail move list (`mothead/bin/<code>.motbin` inside `tkdata.bin`, record size 0x448) holds
encrypted 32-bit fields. Three matter here:

| Offset | Field | Meaning |
|---|---|---|
| 0x00 | name hash | `Kamui(name)`, the hash of the move name |
| 0x20 | animation hash | `Kamui(animation name)` |
| 0xF0 | move key | `Kamui(CODE + "_" + name)`, `CODE` = the three-letter code of the move list in upper case |

`Kamui` is the game's string hash (`kamuiHash.py`, `kamui_search.c`, and `RetailNames::Hash` in the C++ sources).

Two consequences drive everything:

- **Collisions.** Many strings share a hash, so a string that matches the name hash may still be wrong.
- **The move key decides move names.** A wrong string that matches the name hash matches the move key of one row
  with probability 2^-32. On known names the key agrees on 99.94 % of rows. Animation names have no such key.

## Requirements

- Python 3.11 or later, with `zstandard` and `pycryptodome` (archive extraction) and `numpy` (judge training only).
- A C compiler for the search programs: `gcc` on `PATH`, or set `T8_NAMES_CC` to its full path.
- Your own copy of the game. No game file is in this repository.

## Locations: one settings file

`t8names_paths.py` is the only place that knows where anything is. The code folder is never written to. You choose a
**workspace** folder and set one environment variable:

```
set T8_NAMES_WORKSPACE=D:\t8names          (PowerShell: $env:T8_NAMES_WORKSPACE = "D:\t8names")
```

| Workspace path | Content | Written by |
|---|---|---|
| `game/motbin/mothead/bin/<code>.motbin` | move lists extracted from the game | `tkdata.py` |
| `game/names_dictionary.json` | names known so far, `{decimal hash: name}` | `bootstrap_workspace.py` |
| `game/movenames/<code>.csv`, `ALL_movenames_retail.csv` | one row per move (format below) | `motbin_retail.py` |
| `game/movesets/<code>.json` | every table of every move list | `motbin_retail_full.py` |
| `recovery/` | accepted names: `accepted_moves_overlay*.csv`, `accepted_animations_overlay*.csv`, label corrections | `accept_move_key_confirmed.py`, you |
| `recovery/method_lab/typed_corpus/` | typed corpus and the lists of unresolved hashes | `build_name_corpora.py` |
| `recovery/method_lab/keyed_runs/`, `*.json`, `*.csv` | search results and caches | the search runners |
| `merged/` | merged table, extra names, conflict file | `merge_t8_names.py` |

Optional variables: `T8_NAMES_MOVENAMES`, `T8_NAMES_MOVESETS` (use tables that live elsewhere), `T8_NAMES_LEGACY`
(older-game extracts, see below), `T8_NAMES_CC`.

## Step 1: build the workspace from the game

The game archive is `<game folder>/Polaris/Content/Binary/pak/tkdata.bin` (about 2 GB). It is a
`__TEKKEN8FILES__` archive: an AES-encrypted, zstd-compressed table of contents and files, addressed by the Kamui
hash of their path. `tkdata.py` documents the format in its header and reads it without running the game.

```
python bootstrap_workspace.py --tkdata "<game folder>/Polaris/Content/Binary/pak/tkdata.bin"
```

or, if you already have the `.motbin` files: `python bootstrap_workspace.py --motbins <folder>`.

It extracts the move lists, dumps the tables, and builds the corpus. By default the names it starts from are those
of the name table in this repository; add public dictionaries (for example `name_keys.json` of
`AliK3112/tekken-8-movesets-scripts`) with `--names`. New characters need their three-letter code in `--codes`,
because archive paths are stored as hashes and can only be found by name.

Format of `game/movenames/<code>.csv` (and `ALL_movenames_retail.csv`, which adds a first column `code`):

| Column | Meaning |
|---|---|
| `id` | index of the move in the list |
| `name_hash`, `name` | hash of the move name; the name when the dictionary has it, else empty |
| `anim_hash`, `anim_name` | the same for the animation |
| `vuln`, `hitlevel`, `chara_id` | other decoded fields, not used by the name tools |
| `move_key` | `Kamui(CODE_name)` as stored in the game |
| `move_key_ok` | 1 when the dictionary name reproduces the key, 0 when it does not (the label is a collision), empty when unnamed |

A row with an empty `name` is an open move hash; a row with `move_key_ok` = 0 carries a wrong label.

### Older-game extracts (optional)

Tekken 8 reuses many names of Tekken 7 and earlier. The corpus builder and the judge read move lists exported with
TekkenMovesetExtractor (`extracted_chars/t7_<CHAR>/<CHAR>.json`, `tag2_...`): point `T8_NAMES_LEGACY` at that
`extracted_chars` folder before step 1. Without it everything runs, but the judge has no older-game evidence and
its scores differ from the ones it was calibrated with.

## Step 2: check a candidate name

```
python name_judge.py --hash 0x277A8FB4 cGrd_lR21 cAG_1_MB7        # judge: how well does each spelling fit?
python -c "import move_key as K; k=K.load_keys(); print(K.agree(k, 0x277A8FB4, 'cGrd_lR21'))"   # (7, 7): all keys agree
```

Order of evidence for a move name: the move key first; the judge only where the key cannot separate two names.
The judge (`name_judge.py`, model `name_judge_model.json`) scores spelling, neighbouring moves, the animation name,
older-game names and the move's own data. It accepts nothing by itself and handles move names only.

## Step 3: search for names

Run from this folder. Each runner writes into `recovery/method_lab/` and prints what it found. Start with the cheap
complete searches. Times are for 10 worker processes on a desktop CPU; set `EXHAUSTIVE_WORKERS` to change the count.

| Name length | Tool | What it does | Cost |
|---|---|---|---|
| 5 to 10 | `keyed_short_run.py targets`, then `run <n>`, then `report` | complete: every string of that length, hash and key together | seconds to about 1.5 h (length 10) |
| 11 to 20 | `keyed_long_run.py sample`, `count <n> <bits>`, `run <n> <bits>`, `report` | the first characters within a budget of a character model, the last four computed from the hashes | minutes to an hour per length |
| 21 to 24 | `keyed_xl_run.py sample`, `count`, `run <n> <bits>`, `report` | the same idea; the key is only a check there, so it is slower | 1 to 20 h per length |
| any | `keyed_expand.py` | small edits of confirmed names (digits, suffixes) | seconds |

`<bits>` is the search budget: each extra bit doubles the work. Use `count` to see the size before `run`.
`KEYED_ROUND=2` (3, ...) reruns the 11 to 20 search with the names confirmed so far added to its character model.
Names above 24 characters are out of reach of these searches. `short_exhaustive_run.py` and
`exhaustive_10_12_run.py` are the older, key-less complete searches; they are kept because other modules import
them.

Then:

```
python keyed_report_tables.py          # merges every result into keyed_confirmed_all.csv and a readable table
python accept_move_key_confirmed.py    # dry run: what the acceptance rule would record
python accept_move_key_confirmed.py --write
```

Acceptance rule (in the tool's header): a name is recorded when it re-hashes and every move key of its rows agrees;
a name with a single key that came out of a large search also needs support from its row (a neighbour with the same
head, a part shared with the animation name, or a confirmed sibling). Everything else stays a candidate. Accepted
names go to a new `recovery/accepted_moves_overlay_move_key_batch<k>.csv`; no file is ever overwritten.

Animation names cannot be confirmed this way. Record them yourself in a `recovery/accepted_animations_overlay_<what>.csv`
(columns at least `namespace,hash,name,status` with `status` = `accepted`) once you have independent evidence.

## Step 4: merge and update TKMovesets2

```
python merge_t8_names.py --public <name_keys.json>
python judge_collisions.py <workspace>/merged/name_conflicts.csv <workspace>/merged/judge_collisions.csv
python merge_t8_names.py --public <name_keys.json> --judge <workspace>/merged/judge_collisions.csv
python ../generate_game_static_data.py --fighters <fighters-retail.csv> --moves <workspace>/merged/ALL_movenames_retail.csv --extra-names <workspace>/merged/extra_names.json
```

The first merge lists the collisions, the judge scores them, the second merge records the judge's rank next to
each candidate. `<fighters-retail.csv>` is the roster (code, chr_id, fighter name, ...) that the generator already
expects; see `generate_game_static_data.py`.

`merge_t8_names.py` combines the game tables, the public dictionary, the accepted names and the label corrections.
It writes `name_conflicts.csv`: every hash with more than one candidate name, each candidate with its sources,
whether it re-hashes, how many move keys agree, the judge's rank, and which name was picked and why. Keep that file:
it is what lets a later, better rule choose again. After regenerating, update the expected count in
`tests/GameStaticDataTests.cpp`.

## What is in this folder

| Files | Role |
|---|---|
| `t8names_paths.py`, `bootstrap_workspace.py` | locations; workspace construction |
| `tkdata.py`, `motbin_retail.py`, `motbin_retail_full.py` | game archive reader and move-list dumps |
| `kamuiHash.py`, `kamui_search.c`, `recover.py`, `move_key.py` | the hash, the key, shared helpers |
| `keyed_*.py`, `keyed_*.c`, `short_exhaustive*`, `exhaustive_10_12*`, `tail_solver*`, `prefix_*.c` | searches |
| `name_judge.py`, `name_judge_data.py`, `name_judge_model*.json`, `judge_collisions.py` | the judge |
| `build_name_corpora.py`, `name_corpora.py`, `audit_name_corpora.py` | typed corpus |
| `keyed_report_tables.py`, `accept_move_key_confirmed.py`, `merge_t8_names.py` | results, acceptance, merge |
| the remaining modules | imported by the above (character models, prefix enumeration, older search methods) |

## Limits and what was verified

- Verified on Steam build 26155785 (53 move lists). A newer build may move record offsets; `motbin_retail.py` prints
  how many moves it could not decode and how many keys agree, which shows at once whether the layout still fits.
- Verified from an empty workspace, starting from already extracted `.motbin` files: the table dumps (53 lists,
  119,752 moves, no decoding error), the corpus build, the key check, the judge, the complete keyed search of
  length 5 with its report (20 names confirmed by their key, no decoy hit), the results table, the acceptance dry
  run and the merge. Not verified from an empty workspace: extraction straight from `tkdata.bin` and the long
  searches (11 to 24); both ran in the original research location, and the long searches use the same paths and
  compiler setting as the short one.
- Not included: the scripts that trained the judge model and the experiment history. The shipped model can be used
  as is; a refitted `name_judge_model.json` placed in `recovery/method_lab/` takes precedence.
- The search programs are tuned for throughput, not memory safety: run them only on inputs these scripts generate.
