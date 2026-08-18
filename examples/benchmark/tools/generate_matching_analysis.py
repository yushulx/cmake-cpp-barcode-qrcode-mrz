#!/usr/bin/env python3
"""Summarize remaining mismatch pairs after payload normalization."""
import argparse
import json
from collections import Counter
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--results", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    pairs = {}
    placeholders = Counter()
    buffer = ""
    with args.results.open(encoding="utf-8") as handle:
      for line in handle:
        buffer += line
        try:
            row = json.loads(buffer)
        except json.JSONDecodeError:
            continue
        buffer = ""
        decoder = row["decoder"]
        counts = pairs.setdefault(decoder, Counter())
        for match in row.get("matches", []):
            outcome = match.get("outcome")
            if outcome not in {"wrong_text", "wrong_format"}:
                continue
            truth = row["ground_truth"][match["truth_index"]]
            pred = row["predictions"][match["prediction_index"]]
            counts[f"{outcome}:{truth.get('format')}->{pred.get('format')}"] += 1
        for truth in row.get("ground_truth", []):
            if truth.get("text") == "^":
                placeholders[decoder] += 1
    analysis = {
        "leading_zero_only_mismatches_after_existing_upc_ean_normalization": {},
        "code39_extended_alias_correct_matches": {},
        "unreliable_placeholder_ground_truth": dict(placeholders),
        "top_wrong_pairs": {name: dict(counter.most_common(10)) for name, counter in pairs.items()},
        "notes": [
            "UPC-A already normalizes a leading zero when compared with EAN-13",
            "CODE_39 start/stop asterisks are stripped before scoring",
            "CODE_128/GS1_128 leading {GS}, {FNC1}, and ASCII GS markers are stripped before scoring",
            "HTML entities, trailing newlines, and a leading \\000001 escape are normalized before scoring",
            "Ground truth payload ^ is excluded from scoring as an unreliable placeholder",
            "CODE39EXTENDED is treated as CODE_39 by the canonical format normalizer",
        ],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(analysis, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
