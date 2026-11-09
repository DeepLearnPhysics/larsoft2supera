"""Compare the new inventory with source audit counts and legacy labeling records.

This reports representation differences; it does not require a labeling subset
and a persisted transport inventory to have equal lengths or group indices.
"""
import argparse
import collections
import json
import os
from pathlib import Path

import ROOT


def load_larcv():
    include = Path(os.environ['LARCV2_INC'])
    for directory in sorted({include} | {p.parent for p in include.rglob('*.h')}):
        ROOT.gInterpreter.AddIncludePath(str(directory))
    if ROOT.gSystem.Load(str(Path(os.environ['LARCV2_LIB']) / 'liblarcv.so')) < 0:
        raise RuntimeError('Cannot load LArCV2')


def particles(path, label):
    file = ROOT.TFile.Open(str(path))
    tree = file.Get('particle_' + label + '_tree')
    if not tree:
        raise RuntimeError('Missing particle tree: ' + label)
    result = {}
    for entry in tree:
        record = getattr(entry, 'particle_' + label + '_branch')
        event = (int(record.run()), int(record.subrun()), int(record.event()))
        for p in record.as_vector():
            key = event + (int(p.track_id()),)
            if key in result:
                raise RuntimeError('Repeated source track in output')
            result[key] = dict(pdg=int(p.pdg_code()), energy=float(p.energy_init()),
                               parent=int(p.parent_track_id()), ancestor=int(p.ancestor_track_id()),
                               parent_time=float(p.parent_position().t()),
                               ancestor_time=float(p.ancestor_position().t()))
    file.Close()
    return result


def compare(new_file, legacy_file):
    load_larcv()
    new = particles(new_file, 'transport')
    old = particles(legacy_file, 'pcluster')
    archive = ROOT.TFile.Open(str(new_file) + '.transport.root')
    tree = archive.Get('transport')
    states = collections.Counter()
    raw = {}
    primaries = set()
    unrepresentable = []
    for row in tree:
        event = (int(row.run), int(row.subrun), int(row.event))
        key = event + (int(row.track_id),)
        states[int(row.ancestry)] += 1
        trajectory = list(row.trajectory)
        raw[key] = dict(primary=bool(row.primary), parent=int(row.direct_parent),
                        ancestor=int(row.primary_ancestor), steps=len(trajectory) // 8)
        if row.primary:
            primaries.add(key)
        if int(row.unrepresentable_trajectory_coordinates):
            unrepresentable.append(dict(event=list(event), track=int(row.track_id),
                                        coordinates=int(row.unrepresentable_trajectory_coordinates),
                                        max_abs_coordinate=max(abs(trajectory[i])
                                                               for i in range(len(trajectory))
                                                               if i % 8 < 4)))
        if key not in new or new[key]['pdg'] != int(row.pdg):
            raise RuntimeError('LArCV inventory does not preserve archived identity/PDG')
        if trajectory and new[key]['energy'] != trajectory[7] * 1000.:
            raise RuntimeError('LArCV initial energy differs from archived input')
    if len(new) != len(raw):
        raise RuntimeError('Archive and LArCV inventories differ')
    missing_legacy = sorted(set(old) - set(new))
    if missing_legacy:
        raise RuntimeError('A legacy source particle is absent from the new inventory')
    report = dict(new_particles=len(new), legacy_particles=len(old),
                  source_primaries=len(primaries), legacy_primaries=len(primaries & set(old)),
                  restored_primaries=[list(key) for key in sorted(primaries - set(old))],
                  ancestry_states=dict(states),
                  unresolved_primary_affiliation=sum(p['ancestor'] < 0 for p in raw.values()),
                  missing_direct_parents=sum(p['parent'] < 0 and not p['primary'] for p in raw.values()),
                  common_pdg_differences=sum(new[k]['pdg'] != old[k]['pdg'] for k in old),
                  common_initial_energy_differences=sum(new[k]['energy'] != old[k]['energy'] for k in old),
                  common_ancestor_differences=[list(k) for k in old if new[k]['ancestor'] != old[k]['ancestor']],
                  legacy_invalid_parent_times=sum(p['parent_time'] == -9223372036854.7754 for p in old.values()),
                  new_overflowed_parent_times=sum(p['parent_time'] == -9223372036854.7754 for p in new.values()),
                  unrepresentable_coordinates=unrepresentable,
                  stored_trajectory_points=sum(p['steps'] for p in raw.values()))
    archive.Close()
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('new_file', type=Path)
    parser.add_argument('legacy_file', type=Path)
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    report = compare(args.new_file, args.legacy_file)
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
