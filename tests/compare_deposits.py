"""Account for raw contributions and compare aggregate energy voxels with legacy SED."""
import argparse
import collections
import json
from pathlib import Path

import ROOT
from compare_transport import load_larcv


def tensor(path, label):
    file = ROOT.TFile.Open(str(path))
    tree = file.Get('sparse3d_' + label + '_tree')
    if not tree:
        raise RuntimeError('Missing sparse energy tree: ' + label)
    result = {}
    for row in tree:
        record = getattr(row, 'sparse3d_' + label + '_branch')
        event = (int(record.run()), int(record.subrun()), int(record.event()))
        meta = record.meta()
        result[event] = (str(meta.dump()), {int(v.id()): float(v.value()) for v in record.as_vector()})
    file.Close()
    return result


def compare(new_file, reference):
    load_larcv()
    new = tensor(new_file, 'deposit_energy')
    old = tensor(reference, 'sed')
    if set(new) != set(old):
        raise RuntimeError('Event inventories differ')
    differences = []
    for event in new:
        metadata, actual = new[event]
        expected_meta, expected = old[event]
        if metadata != expected_meta:
            raise RuntimeError('Energy voxel metadata differs')
        common = set(actual) & set(expected)
        maximum = max((abs(actual[v] - expected[v]) for v in common), default=0.)
        differences.append(dict(event=list(event), new_voxels=len(actual), legacy_voxels=len(expected),
                                new_only=len(set(actual) - set(expected)), legacy_only=len(set(expected) - set(actual)),
                                exact_values=sum(actual[v] == expected[v] for v in common),
                                maximum_abs_difference_mev=maximum,
                                new_energy_mev=sum(actual.values()), legacy_energy_mev=sum(expected.values())))
    archive = ROOT.TFile.Open(str(new_file) + '.deposits.root')
    accounting = collections.defaultdict(lambda: dict(records=0, counts=[0] * 6, energy_mev=[0.] * 6,
                                                     invalid_energy=0, invalid_position=0, outside_grid=0))
    for row in archive.Get('events'):
        key = str(row.collection)
        item = accounting[key]
        item['records'] += int(row.count)
        for i in range(6):
            item['counts'][i] += int(row.counts[i])
            item['energy_mev'][i] += float(row.energies[i])
        for field in ('invalid_energy', 'invalid_position', 'outside_grid'):
            item[field] += int(getattr(row, field))
    for item in accounting.values():
        if sum(item['counts']) != item['records']:
            raise RuntimeError('Deposit accounting lost records')
    archive.Close()
    return dict(attribution_order=['exact', 'reassigned', 'ancestor', 'unresolved', 'conflict', 'evidence_ancestor'],
                accounting=dict(accounting), voxel_comparison=differences)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('new_file', type=Path)
    parser.add_argument('reference', type=Path)
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    report = compare(args.new_file, args.reference)
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
