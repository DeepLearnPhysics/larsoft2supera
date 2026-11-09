"""Explain legacy differences using persisted coordinates and its measured geometry cut."""
import argparse
import collections
import json
import math
from pathlib import Path
import struct

import ROOT
from compare_deposits import tensor
from compare_transport import load_larcv


def audit(new_file, reference, bounds):
    load_larcv()
    old = tensor(reference, 'sed')
    new = tensor(new_file, 'deposit_energy')
    file = ROOT.TFile.Open(str(new_file))
    tree = file.Get('sparse3d_deposit_energy_tree')
    tree.GetEntry(0)
    meta = tree.sparse3d_deposit_energy_branch.meta()
    archive = ROOT.TFile.Open(str(new_file) + '.deposits.root')
    deposits = collections.defaultdict(lambda: collections.defaultdict(list))
    cropped = collections.defaultdict(lambda: collections.defaultdict(list))
    unresolved = collections.defaultdict(lambda: dict(records=0, energy_mev=0.))
    excluded = []
    for row in archive.Get('deposits'):
        if int(row.kind) != 0:
            continue
        event = (int(row.run), int(row.subrun), int(row.event))
        energy = float(row.energy_mev)
        xyz = [float(row.x), float(row.y), float(row.z)]
        voxel = int(meta.id(*xyz))
        deposits[event][voxel].append(energy)
        if int(row.attribution) == 3:
            item = unresolved[(event, int(row.raw_track))]
            item['records'] += 1
            item['energy_mev'] += energy
        inside = all(bounds['minimum'][a] <= xyz[a] <= bounds['maximum'][a] for a in range(3))
        if inside:
            cropped[event][voxel].append(energy)
        else:
            excluded.append(dict(event=list(event), source_index=int(row.source_index),
                                 track=int(row.raw_track), xyz_cm=xyz, energy_mev=energy, voxel=voxel))
    errors = []
    for event, (_, expected) in old.items():
        if set(cropped[event]) != set(expected):
            raise RuntimeError('Geometry cut does not explain the legacy voxel inventory')
        actual = new[event][1]
        if set(deposits[event]) != set(actual):
            raise RuntimeError('New voxel projection lost or added an input voxel')
        for voxel, values in deposits[event].items():
            rounded = struct.unpack('f', struct.pack('f', math.fsum(values)))[0]
            if actual[voxel] != rounded:
                raise RuntimeError('New tensor differs from independently rounded source energy')
        for voxel, values in cropped[event].items():
            exact = math.fsum(values)
            error = abs(expected[voxel] - exact)
            # At most n additions within clusters and n across clusters, all nonnegative.
            # IEEE float unit roundoff is 2^-24; this is an analytic accumulation bound.
            n_u = 2 * len(values) * 2.**-24
            bound = n_u / (1 - n_u) * exact
            if n_u >= 1 or error > bound:
                raise RuntimeError('Legacy energy difference exceeds float accumulation bound')
            errors.append(error)
    file.Close()
    archive.Close()
    return dict(geometry_cut_explains_all_voxel_inventory_differences=True,
                new_tensor_equals_independently_rounded_source=True,
                legacy_errors_within_float_accumulation_bound=True,
                maximum_legacy_rounding_error_mev=max(errors, default=0.),
                excluded_records=excluded,
                excluded_energy_mev=math.fsum(x['energy_mev'] for x in excluded),
                unresolved=[dict(event=list(event), track=track, **item)
                            for (event, track), item in sorted(unresolved.items())])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('new_file', type=Path)
    parser.add_argument('reference', type=Path)
    parser.add_argument('--bounds', required=True, type=Path)
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    result = audit(args.new_file, args.reference, json.loads(args.bounds.read_text()))
    args.report.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: len(v) if isinstance(v, list) else v for k, v in result.items()}, indent=2))
