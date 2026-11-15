# Moveset reference sheets

Local snapshots of the two user-supplied Google Sheets. **T8 means retail Tekken 8**, as identified by the user. This archive is separate from the CNT/CBT binary-format and runtime research. The exact retail patch/build remains unspecified.

The manifest records retrieval time in UTC, source links, tab IDs, observed page revisions, sizes and SHA-256 hashes. A page revision is observed separately from the XLSX request; it is not a pinned export revision.

## Sources

- [Tekken 7 MovesetEditor Data](https://docs.google.com/spreadsheets/d/1xOvUozLiC-pwflK53JtZs_-Qnd3jQ5KyQspNooEtPAI/edit?gid=121831918): 8 tabs, 3,127 populated cells, 68 notes, 242 formulas.
- [Tekken 8 Moveset Editing Data — retail](https://docs.google.com/spreadsheets/d/1DBkC-HfqD0KWQNeOTKjJWmPxdbEuCcGZxkPxQpsLkOY/edit?gid=0): 10 tabs, 3,906 populated cells, 73 notes, 269 formulas.

## Files

- `t7/source.xlsx` and `t8-retail/source.xlsx`: original exports saved byte-for-byte, retaining formatting and other workbook features.
- `t7/workbook.json` and `t8-retail/workbook.json`: all nonempty cells with original addresses, typed cached values, raw value text, formulas and their attributes, exported cell notes, rich text, style IDs and merged ranges. Style IDs refer to the same file's `styles` XML tree.
- `*/tabs/GID.csv`: UTF-8 CSV per tab, in original row/column order, using cached formula results. Interior blank cells/rows remain blank. Formatting-only trailing cells are omitted. Notes and formulas are in JSON/XLSX.
- [label-catalog.json](label-catalog.json): 1,280 requirement/property rows indexed by game, category and numeric ID. Retains source ID spelling, labels, parameter/value ranges, descriptions, notes, style IDs and source row links. These are unreviewed reference entries, not installed application labels.
- [manifest.json](manifest.json): tab inventory and hashes for all 23 data files.

Shared formulas retain the original OOXML representation: an anchor has formula text, and dependent cells refer to its `si` identifier. Formulas are not executed or recalculated.

## Tabs

| Category | T7 CSV | Retail T8 CSV |
| --- | --- | --- |
| Requirements | [Requirement](t7/tabs/121831918.csv) | [Requirements](t8-retail/tabs/0.csv) |
| Extra properties | [Extra Properties](t7/tabs/0.csv) | [Extra Move Properties](t8-retail/tabs/1974294367.csv) |
| Sound effects | [Sound Effects](t7/tabs/1071911039.csv) | [Sound Effects](t8-retail/tabs/2099031418.csv) |
| Cancel extra data | [Cancel Extra Data](t7/tabs/1876612704.csv) | [Cancel Extra Data](t8-retail/tabs/638509589.csv) |
| Cancel options | [Cancel Option](t7/tabs/719076746.csv) | [Cancel Options](t8-retail/tabs/1123472459.csv) |
| Hit levels | [Hit Levels](t7/tabs/1267385287.csv) | [Hit Levels](t8-retail/tabs/1292684689.csv) |
| Vulnerability | [Vulnerability](t7/tabs/414625078.csv) | [Vulnerability](t8-retail/tabs/422187284.csv) |
| Commands | [Command](t7/tabs/1079598677.csv) | [Command](t8-retail/tabs/334355488.csv) |
| Generic move aliases | — | [Generic Move IDs](t8-retail/tabs/1275930420.csv) |
| Limb IDs | — | [Limb IDs](t8-retail/tabs/2078729588.csv) |

Original tab names, exact row locations and per-tab source links are in the manifest and workbook JSON.

## Using the references

The catalog contains 207 T7 requirement rows, 316 T7 property rows, 433 retail T8 requirement rows and 324 retail T8 property rows. Duplicate IDs and parameter-specific rows remain separate. Eight indexed rows have no label; missing source content stays missing. Rows whose ID is not one unambiguous decimal/hex integer remain in the complete workbook JSON/CSV rather than being guessed into the catalog.

For editor labels, review each entry's notes, value ranges and formatting before copying it into a resource. Source spelling, uncertainty and questionable descriptions are preserved.

For T7 to retail T8 conversions, use these as version-separated research inputs. Equal IDs or matching labels alone do not establish equivalent parameter encodings or behavior. No conversion map is generated or enabled by this scrape. CNT/CBT layouts, offsets and IDs require independent validation.

## Refresh

Run from the repository root with Python 3.9 or later; only the standard library is required:

~~~powershell
python tools/scrape_moveset_reference_sheets.py
~~~

The command downloads both complete workbooks and their public tab metadata, validates the tab inventories, prepares JSON/CSV, then installs files through temporary siblings. Downloads/parsing must finish for both sources before replacement starts. All-file publication is not one filesystem transaction; the manifest is written last and its hashes identify the completed snapshot. Refresh into a separate `--output` directory to compare versions before replacing this archive.

## Verification

An independent openpyxl reader agreed with all 7,033 populated cells, 511 formulas (including shared formulas), 141 notes and merged ranges. All 18 CSVs were compared with their extracted matrices, and every data-file hash was checked. Both requirement terminator entries (T7 881 and retail T8 1100) retain their source labels and row links. This checks extraction fidelity, not game-engine semantics.
