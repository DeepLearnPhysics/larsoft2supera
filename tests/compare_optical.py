"""Exact comparison of ICARUS optical and optional CRT products, excluding file metadata.

Every product is read with its compiled dictionary and independently streamed
into a fresh ROOT buffer. Compare bytes without tolerance or reordering. This
includes event IDs, producer names, and every serialized field of the selected products.
Unknown keys/classes, missing products, wrong counts, and invalid IDs fail closed.
"""
import argparse
import json
import os
from pathlib import Path
import ROOT


def load_comparator():
    include = Path(os.environ['LARCV2_INC'])
    # The shipped dictionary references header basenames, not installed paths.
    for directory in sorted({include} | {p.parent for p in include.rglob('*.h')}):
        ROOT.gInterpreter.AddIncludePath(str(directory))
    library = str(Path(os.environ['LARCV2_LIB']) / 'liblarcv.so')
    if ROOT.gSystem.Load(library) < 0:
        raise RuntimeError('Cannot load compiled LArCV dictionary')
    source = Path(__file__).resolve().parent / 'CompareOptical.hxx'
    if not ROOT.gInterpreter.Declare(source.read_text()):
        raise RuntimeError('Cannot compile comparator')


def compare(left, right, expected_ids, auxiliary=False):
    rows = ROOT.supera_audit.compare(str(left), str(right), len(expected_ids), auxiliary)
    records = []
    for row in rows:
        record = {key: str(getattr(row, key)) for key in
                  ('tree', 'branch', 'type', 'left_id', 'right_id')}
        record.update({key: int(getattr(row, key)) for key in
                       ('entry', 'left_bytes', 'right_bytes', 'first_difference')})
        record['equal'] = bool(row.equal)
        record['expected_id'] = expected_ids[row.entry]
        record['ids_valid'] = record['left_id'] == record['right_id'] == record['expected_id']
        records.append(record)
    return {'passed': all(r['equal'] and r['ids_valid'] for r in records),
            'left': str(left), 'right': str(right), 'events': len(expected_ids),
            'trees': len({r['tree'] for r in records}), 'product_events': len(records),
            'bytes_compared': sum(r['left_bytes'] for r in records), 'records': records}


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('left')
    p.add_argument('right')
    p.add_argument('--expected-ids', required=True, help='JSON list of run:subrun:event strings')
    p.add_argument('--report', required=True)
    args = p.parse_args()
    expected_ids = json.loads(Path(args.expected_ids).read_text())
    if not expected_ids or len(expected_ids) != len(set(expected_ids)):
        raise ValueError('Expected identities must be nonempty and unique')
    load_comparator()
    result = compare(args.left, args.right, expected_ids)
    Path(args.report).write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'records'}, indent=2))
    raise SystemExit(0 if result['passed'] else 1)
