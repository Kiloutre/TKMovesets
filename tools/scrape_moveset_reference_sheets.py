"""Archive both public moveset reference sheets using Python's standard library.

Retains the XLSX bytes, cached values, formulas, notes, rich text and styles.
No source script or formula is executed. T8 refers to retail, not CNT/CBT.
"""
import argparse
import csv
from datetime import datetime, timezone
from decimal import Decimal
import hashlib
import io
import json
import os
from pathlib import Path
import posixpath
import re
import tempfile
import urllib.request
import xml.etree.ElementTree as ET
import zipfile

SOURCES = (
    {"game": "t7", "title": "Tekken 7 MovesetEditor Data",
     "spreadsheet_id": "1xOvUozLiC-pwflK53JtZs_-Qnd3jQ5KyQspNooEtPAI", "initial_gid": 121831918},
    {"game": "t8-retail", "title": "Tekken 8 Moveset Editing Data",
     "spreadsheet_id": "1DBkC-HfqD0KWQNeOTKjJWmPxdbEuCcGZxkPxQpsLkOY", "initial_gid": 0},
)
NS = {"s": "http://schemas.openxmlformats.org/spreadsheetml/2006/main",
      "r": "http://schemas.openxmlformats.org/officeDocument/2006/relationships"}
MAX_DOWNLOAD = 32 << 20
MAX_EXPANDED = 128 << 20


def download(url):
    request = urllib.request.Request(url, headers={"User-Agent": "TKMovesets2-reference-snapshot/1.0"})
    with urllib.request.urlopen(request, timeout=45) as response:
        content = response.read(MAX_DOWNLOAD + 1)
    if len(content) > MAX_DOWNLOAD:
        raise ValueError("Download exceeds 32 MiB")
    return content


def xml_text(element):
    if element is None:
        return ""
    return "".join(node.text or "" for node in element.iter(f"{{{NS['s']}}}t"))


def xml_record(element):
    result = {"tag": element.tag.rsplit("}", 1)[-1]}
    if element.attrib:
        result["attributes"] = dict(element.attrib)
    if element.text:
        result["text"] = element.text
    if len(element):
        result["children"] = [xml_record(child) for child in element]
    return result


def column_index(address):
    match = re.fullmatch(r"([A-Z]+)([1-9][0-9]*)", address)
    if not match:
        raise ValueError(f"Invalid cell address: {address}")
    column = 0
    for letter in match[1]:
        column = column * 26 + ord(letter) - ord("A") + 1
    return int(match[2]), column


def scalar(cell, strings):
    kind = cell.get("t", "n")
    raw = cell.findtext("s:v", namespaces=NS)
    if kind == "inlineStr":
        return xml_text(cell.find("s:is", NS)), None
    if raw is None:
        return None, None
    if kind == "s":
        return strings[int(raw)]["text"], raw
    if kind == "b":
        return raw == "1", raw
    if kind in ("str", "e", "d"):
        return raw, raw
    value = Decimal(raw)
    if not value.is_finite():
        raise ValueError(f"Non-finite numeric cell: {raw}")
    return int(value) if value == value.to_integral_value() else float(value), raw


def sheet_metadata(page):
    # Read JSON as data, never evaluate Google's script or cell formulas.
    marker = "var bootstrapData = "
    start = page.index(marker) + len(marker)
    bootstrap, _ = json.JSONDecoder().raw_decode(page[start:])
    metadata = {}
    for opcode, raw in bootstrap["changes"]["topsnapshot"]:
        if opcode == 21350203:
            row = json.loads(raw)
            name = row[3][0]["1"][0][2]
            metadata[name] = {"gid": int(row[2]), "order": row[0]}
    if not metadata:
        raise ValueError("Google page did not expose tab identifiers")
    return metadata, bootstrap["changes"].get("revision")


def relationships(archive, part):
    base = posixpath.dirname(part)
    rel_part = posixpath.join(base, "_rels", posixpath.basename(part) + ".rels")
    if rel_part not in archive.namelist():
        return {}
    result = {}
    for node in ET.fromstring(archive.read(rel_part)):
        external = node.get("TargetMode") == "External"
        target = node.attrib["Target"]
        if not external:
            target = target.lstrip("/") if target.startswith("/") else posixpath.normpath(posixpath.join(base, target))
            if target.startswith("../"):
                raise ValueError("Relationship escapes workbook")
        result[node.attrib["Id"]] = {"target": target, "external": external,
                                      "type": node.attrib["Type"].rsplit("/", 1)[-1]}
    return result


def parse_workbook(content, source, metadata):
    archive = zipfile.ZipFile(io.BytesIO(content))
    if sum(item.file_size for item in archive.infolist()) > MAX_EXPANDED:
        raise ValueError("Expanded workbook exceeds 128 MiB")
    strings = []
    if "xl/sharedStrings.xml" in archive.namelist():
        for node in ET.fromstring(archive.read("xl/sharedStrings.xml")):
            strings.append({"text": xml_text(node), "xml": xml_record(node)})
    book = ET.fromstring(archive.read("xl/workbook.xml"))
    links = relationships(archive, "xl/workbook.xml")
    sheets = []
    for tab in book.find("s:sheets", NS):
        name = tab.attrib["name"]
        info = metadata[name]
        part = links[tab.attrib[f"{{{NS['r']}}}id"]]["target"]
        root = ET.fromstring(archive.read(part))
        related = relationships(archive, part)
        annotations = {}
        for relation in related.values():
            if relation["type"] == "comments" and not relation["external"]:
                comments = ET.fromstring(archive.read(relation["target"]))
                authors = [node.text or "" for node in comments.findall("s:authors/s:author", NS)]
                for node in comments.findall("s:commentList/s:comment", NS):
                    author_id = int(node.get("authorId", "0"))
                    annotations[node.attrib["ref"]] = {
                        "text": xml_text(node.find("s:text", NS)),
                        "author": authors[author_id] if author_id < len(authors) else None,
                    }
        hyperlinks = {}
        for node in root.findall("s:hyperlinks/s:hyperlink", NS):
            link = dict(node.attrib)
            rid = link.pop(f"{{{NS['r']}}}id", None)
            if rid:
                link["target"] = related[rid]["target"]
            hyperlinks[node.attrib["ref"]] = link
        rows = []
        max_row = max_column = formulas = note_count = errors = populated = 0
        seen_annotations = set()
        for row in root.findall("s:sheetData/s:row", NS):
            cells = []
            for cell in row:
                address = cell.attrib["r"]
                row_number, col = column_index(address)
                value, raw = scalar(cell, strings)
                formula = cell.find("s:f", NS)
                record = {"address": address, "value": value, "type": cell.get("t", "n"), "style": int(cell.get("s", "0"))}
                if raw is not None:
                    record["raw_value"] = raw
                if formula is not None:
                    record["formula"] = {"text": formula.text or "", "attributes": dict(formula.attrib)}
                    formulas += 1
                if address in annotations:
                    record["note"] = annotations[address]
                    seen_annotations.add(address)
                    note_count += 1
                if address in hyperlinks:
                    record["hyperlink"] = hyperlinks[address]
                if cell.get("t") == "s" and raw is not None:
                    shared = strings[int(raw)]
                    if any(child["tag"] == "r" for child in shared["xml"].get("children", [])):
                        record["rich_text"] = shared["xml"]
                if cell.get("t") == "inlineStr":
                    record["rich_text"] = xml_record(cell.find("s:is", NS))
                if value is not None or formula is not None or "note" in record or "hyperlink" in record:
                    cells.append(record)
                    max_row, max_column = max(max_row, row_number), max(max_column, col)
                    populated += 1
                    errors += cell.get("t") == "e"
            if cells:
                rows.append({"row": int(row.attrib["r"]), "cells": cells})
        if seen_annotations != annotations.keys():
            raise ValueError(f"Unrepresented annotated cells in {name}")
        sheet_url = f"https://docs.google.com/spreadsheets/d/{source['spreadsheet_id']}/edit#gid={info['gid']}"
        sheets.append({"name": name, "gid": info["gid"], "url": sheet_url, "state": tab.get("state", "visible"),
                       "row_count": max_row, "column_count": max_column, "populated_cells": populated,
                       "formula_count": formulas, "note_count": note_count, "error_count": errors,
                       "merged_ranges": [node.attrib["ref"] for node in root.findall("s:mergeCells/s:mergeCell", NS)],
                       "rows": rows})
    if set(metadata) != {sheet["name"] for sheet in sheets}:
        raise ValueError("HTML tab inventory differs from XLSX export; retry the snapshot")
    styles = xml_record(ET.fromstring(archive.read("xl/styles.xml")))
    return {"schema_version": 1, "game": source["game"], "build": "unspecified", "styles": styles, "sheets": sheets}


def csv_content(sheet):
    matrix = [[""] * sheet["column_count"] for _ in range(sheet["row_count"])]
    for row in sheet["rows"]:
        for cell in row["cells"]:
            r, c = column_index(cell["address"])
            value = cell["value"]
            matrix[r - 1][c - 1] = "" if value is None else value
    output = io.StringIO(newline="")
    csv.writer(output, lineterminator="\n").writerows(matrix)
    return output.getvalue()


def numeric_id(value):
    if isinstance(value, bool) or value is None:
        return None
    if isinstance(value, int):
        return value
    if isinstance(value, str) and re.fullmatch(r"(?:0[xX][0-9a-fA-F]+|[0-9]+)", value.strip()):
        return int(value.strip(), 16 if value.strip().lower().startswith("0x") else 10)
    return None


def label_entries(workbook):
    entries = []
    for sheet in workbook["sheets"]:
        if sheet["name"] in ("Requirement", "Requirements"):
            category = "requirement"
        elif sheet["name"] in ("Extra Properties", "Extra Move Properties"):
            category = "extra_property"
        else:
            continue
        for row in sheet["rows"]:
            if row["row"] == 1:
                continue
            cells = {re.sub(r"[0-9]+$", "", cell["address"]): cell for cell in row["cells"]}
            value = cells.get("A", {}).get("value")
            identifier = numeric_id(value)
            if identifier is None:
                continue
            entries.append({"game": workbook["game"], "category": category, "id": identifier,
                            "id_source_value": value, "parameter_or_value_range": cells.get("B", {}).get("value"),
                            "label": cells.get("C", {}).get("value"), "description": cells.get("D", {}).get("value"),
                            "related_requirements": cells.get("E", {}).get("value"),
                            "source": {"sheet": sheet["name"], "gid": sheet["gid"], "row": row["row"],
                                       "url": sheet["url"] + f"&range=A{row['row']}:E{row['row']}"},
                            "notes": {col: cell["note"] for col, cell in cells.items() if "note" in cell},
                            "style_ids": {col: cell["style"] for col, cell in cells.items()}})
    return entries


def json_bytes(value):
    return (json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False) + "\n").encode("utf-8")


def install(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as output:
            output.write(content)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def snapshot(destination, cache=None):
    fetched = datetime.now(timezone.utc).isoformat(timespec="seconds")
    manifest = {"schema_version": 1, "retrieved_at_utc": fetched, "sources": []}
    all_entries = []
    pending = {}
    for source in SOURCES:
        sid = source["spreadsheet_id"]
        url = f"https://docs.google.com/spreadsheets/d/{sid}/edit?gid={source['initial_gid']}"
        export_url = f"https://docs.google.com/spreadsheets/d/{sid}/export?format=xlsx"
        if cache:
            content = (cache / f"{sid}.xlsx").read_bytes()
            page = (cache / f"{sid}.html").read_text(encoding="utf-8")
        else:
            content = download(export_url)
            page = download(url).decode("utf-8")
        metadata, revision = sheet_metadata(page)
        workbook = parse_workbook(content, source, metadata)
        provenance = {**source, "source_url": url, "export_url": export_url, "retrieved_at_utc": fetched,
                      "xlsx_sha256": hashlib.sha256(content).hexdigest(),
                      "page_revision_observed": revision,
                      "acquisition": "cached_download" if cache else "public_https_export"}
        workbook["provenance"] = provenance
        game = source["game"]
        pending[f"{game}/source.xlsx"] = content
        pending[f"{game}/workbook.json"] = json_bytes(workbook)
        tabs = []
        for sheet in workbook["sheets"]:
            filename = f"{game}/tabs/{sheet['gid']}.csv"
            data = csv_content(sheet).encode("utf-8")
            pending[filename] = data
            tabs.append({key: value for key, value in sheet.items() if key != "rows"} |
                        {"csv": filename, "csv_sha256": hashlib.sha256(data).hexdigest()})
        manifest["sources"].append(provenance | {"tabs": tabs})
        all_entries.extend(label_entries(workbook))
    pending["label-catalog.json"] = json_bytes({"schema_version": 1, "status": "unreviewed_source_labels",
                                               "entries": all_entries})
    manifest["label_catalog_entries"] = len(all_entries)
    manifest["files"] = {name: {"sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data)}
                         for name, data in sorted(pending.items())}
    pending["manifest.json"] = json_bytes(manifest)
    # Download and parse both sources before replacing any snapshot file.
    for name, data in pending.items():
        install(destination / name, data)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] / "research/moveset-reference-sheets")
    parser.add_argument("--from-cache", type=Path, help="Read prior ID.xlsx and ID.html downloads instead of the network")
    args = parser.parse_args()
    manifest = snapshot(args.output, args.from_cache)
    for source in manifest["sources"]:
        print(f"{source['game']}: {len(source['tabs'])} tabs, "
              f"{sum(tab['populated_cells'] for tab in source['tabs'])} populated cells, "
              f"{sum(tab['note_count'] for tab in source['tabs'])} notes, "
              f"{sum(tab['formula_count'] for tab in source['tabs'])} formulas")
    print(f"{manifest['label_catalog_entries']} requirement/property rows indexed; output: {args.output.resolve()}")


if __name__ == "__main__":
    main()
