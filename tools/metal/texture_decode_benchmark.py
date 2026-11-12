#!/usr/bin/env python3
"""Read-only extraction of anonymized SL texture inputs for local codec research.

Never uploads inputs. Copies cache bytes into an ignored artifact directory;
the original cache is not changed. Codec executables use the generated TSVs.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import struct
import uuid


def image_info(data):
    """Parse the mandatory raw J2K SIZ marker, without decoding pixels."""
    if len(data) < 42 or data[:4] != b"\xff\x4f\xff\x51":
        raise ValueError("not a raw J2K codestream with SIZ")
    xs, ys, xo, yo = struct.unpack_from(">4I", data, 8)
    components = struct.unpack_from(">H", data, 40)[0]
    if xs <= xo or ys <= yo or not 1 <= components <= 4:
        raise ValueError("unsupported dimensions/component count")
    if len(data) < 42 + 3 * components:
        raise ValueError("short SIZ")
    for c in range(components):
        if data[42 + 3*c:45 + 3*c] != b"\x07\x01\x01":
            raise ValueError("not unsigned 8-bit, full-resolution components")
    return xs-xo, ys-yo, components


def select_balanced(items, limit):
    """Deterministic size/component strata, rather than just the smallest files."""
    groups = collections.defaultdict(list)
    for item in items:
        group = (max(item["width"], item["height"]).bit_length(), item["components"])
        groups[group].append(item)
    for values in groups.values():
        values.sort(key=lambda item: item["sha256"])
    result = []
    while groups and len(result) < limit:
        for key in sorted(list(groups)):
            result.append(groups[key].pop(0))
            if not groups[key]:
                del groups[key]
            if len(result) == limit:
                break
    return result


def max_reduce(data):
    """Constrain every codec to a legal reduction from main COD/COC markers."""
    offset, levels = 2, []
    components = struct.unpack_from(">H", data, 40)[0]
    while offset + 4 <= len(data):
        marker = data[offset:offset+2]
        if marker == b"\xff\x90":  # first SOT: main-header defaults only
            break
        size = struct.unpack_from(">H", data, offset+2)[0]
        if size < 2 or offset+2+size > len(data):
            break
        if marker == b"\xff\x52" and size >= 8:
            levels.append(data[offset+9])
        elif marker == b"\xff\x53":
            decomp_offset = offset + 5 + (2 if components > 256 else 1)
            if decomp_offset < offset+2+size:
                levels.append(data[decomp_offset])
        offset += 2+size
    if not levels:
        raise ValueError("no complete main COD marker")
    return min(levels)


def extract(cache, output, limit, partial_limit):
    entries_path, headers_path = cache/"texture.entries", cache/"texture.cache"
    entries, headers = entries_path.read_bytes(), headers_path.read_bytes()
    if len(entries) < 44:
        raise ValueError("short entries header")
    version, address, encoder, count = struct.unpack_from("<fI32sI", entries)
    if address != 64 or len(entries) != 44 + count*28 or len(headers) < count*600:
        raise ValueError("cache layout differs from current macOS LLTextureCache")
    counts, candidates = collections.Counter(), {"complete": [], "partial": []}
    for index in range(count):
        ident, total, body_size, _ = struct.unpack_from("<16siiI", entries, 44+index*28)
        if total <= 0 or body_size < 0:
            counts["inactive"] += 1
            continue
        ident = str(uuid.UUID(bytes=ident))
        data = headers[index*600:(index+1)*600]
        if body_size:
            body_path = cache/ident[0]/(ident+".texture")
            try:
                body = body_path.read_bytes()
            except FileNotFoundError:
                counts["missing_body"] += 1
                continue
            if len(body) != body_size:
                counts["body_size_changed"] += 1
                continue
            data += body
        data = data[:total]
        try:
            width, height, components = image_info(data)
            reduce_limit = max_reduce(data)
        except ValueError:
            counts["unsupported_header"] += 1
            continue
        category = "complete" if len(data) == total and data.endswith(b"\xff\xd9") else "partial"
        counts[category] += 1
        candidates[category].append(dict(width=width, height=height, components=components,
                                         sha256=hashlib.sha256(data).hexdigest(), data=data,
                                         declared_bytes=total, bytes=len(data), max_reduce=reduce_limit))
    output.mkdir(parents=True, exist_ok=True)
    corpus = output/"corpus"
    corpus.mkdir(exist_ok=True)
    selected = []
    for category, amount in (("complete", limit), ("partial", partial_limit)):
        manifests = {"full": [], "progressive": []}
        for i, item in enumerate(select_balanced(candidates[category], amount)):
            case_id = f"{category}_{i:04d}"
            path = (corpus/(case_id+".j2c")).resolve()
            path.write_bytes(item.pop("data"))
            item.update(id=case_id, category=category)
            selected.append(item)
            manifests["full"].append(f"{case_id}_r0\t{path}\t0\n")
            for reduce in sorted({min(r, item["max_reduce"]) for r in (3, 2, 1, 0)}, reverse=True):
                manifests["progressive"].append(f"{case_id}_r{reduce}\t{path}\t{reduce}\n")
        for kind, lines in manifests.items():
            (output/f"{category}-{kind}.tsv").write_text("".join(lines))
    summary = dict(schema_version=1, cache_version=version, cache_address_bits=address,
                   encoder=encoder.split(b"\0")[0].decode(), entries=count,
                   source_counts=dict(counts), selection="balanced dimensions/components; SHA256 order",
                   source_entries_sha256=hashlib.sha256(entries).hexdigest(),
                   source_headers_sha256=hashlib.sha256(headers).hexdigest(), selected=selected)
    (output/"corpus.json").write_text(json.dumps(summary, indent=2)+"\n")
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--limit", type=int, default=192)
    parser.add_argument("--partial-limit", type=int, default=48)
    args = parser.parse_args()
    if args.limit <= 0 or args.partial_limit < 0:
        parser.error("invalid corpus sizes")
    summary = extract(args.cache, args.output, args.limit, args.partial_limit)
    print(json.dumps(dict(source_counts=summary["source_counts"], selected=len(summary["selected"]))))


if __name__ == "__main__":
    main()
