"""Verify immutable fixture identities before running integration tests."""
import hashlib
import json
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2])
manifest = json.loads((Path(__file__).resolve().parent.parent /
                       "docs/reference-baseline.json").read_text())
roles = {"augmented_cluster3d_cache", "canonical_larcv_reference", "staged_larcv_reference"}
for artifact in manifest["artifacts"]:
    if artifact["role"] not in roles:
        continue
    path = root / artifact["lab_relative_path"]
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    if path.stat().st_size != artifact["bytes"] or digest.hexdigest() != artifact["sha256"]:
        raise RuntimeError(f"Baseline checksum mismatch: {path}")
output.write_text(json.dumps(manifest["event_ids"], indent=2) + "\n")
print("PASS: cached input and both reference file checksums")
