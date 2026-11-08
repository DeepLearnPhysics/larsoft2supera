"""Compare all twenty configured sparse tensors, every serialized field, without tolerance."""
import argparse
import json
from pathlib import Path
import subprocess

from compare_optical import compare, load_comparator
import ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lab', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--compare-only', action='store_true', help='Recheck an existing output without rerunning art')
    args = parser.parse_args()
    directory = args.output.resolve()
    directory.mkdir(parents=True, exist_ok=args.compare_only)
    tests = Path(__file__).resolve().parent
    ids_path = directory / 'event-ids.json'
    subprocess.run(['python3', str(tests / 'verify_baseline.py'), str(args.lab), str(ids_path)], check=True)
    ids = json.loads(ids_path.read_text())
    if not args.compare_only:
        config = (tests.parent / 'fcl/spacepoint_icarus.fcl').read_text()
        (directory / 'job.fcl').write_text(config)
        with (directory / 'run.log').open('w') as log:
            subprocess.run(['lar', '-c', str(directory / 'job.fcl'), '-n', str(len(ids)), '-s',
                            str(args.lab / 'data/icarus-overlay-cluster3d-v10_27_00/overlay_cluster3d_cache.root')],
                           cwd=directory, stdout=log, stderr=subprocess.STDOUT, check=True)
    load_comparator()
    # Independently prove that every comparison dimension can detect a changed file.
    if not ROOT.gInterpreter.Declare((tests / 'SpacePointComparatorFixture.hxx').read_text()):
        raise RuntimeError('Cannot compile comparator fixtures')
    fixture = directory / 'comparator-fixtures'
    fixture.mkdir(exist_ok=args.compare_only)
    baseline = fixture / 'baseline.root'
    ROOT.supera_sparse_fixture.write(str(baseline), '')
    if not compare(baseline, baseline, ['1:2:3'], spacepoints=True)['passed']:
        raise RuntimeError('Comparator rejects identical sparse data')
    for mutation in ['event', 'missing', 'extra', 'entries', 'bounds', 'counts', 'id', 'value']:
        changed = fixture / (mutation + '.root')
        ROOT.supera_sparse_fixture.write(str(changed), mutation)
        try:
            accepted = compare(baseline, changed, ['1:2:3'], spacepoints=True)['passed']
        except Exception:
            accepted = False
        if accepted:
            raise RuntimeError('Comparator missed mutation: ' + mutation)
    (fixture / 'summary.json').write_text(json.dumps({'passed': True, 'mutations_rejected': 8})+'\n')
    summaries = []
    for name, relative in [('staged', 'results/staged-full-01/supera_reference.root'),
                           ('standard', 'results/standard-full-01/larcv_mc_20261107_001608_861108.root')]:
        result = compare(args.lab / relative, directory / 'spacepoints.root', ids, spacepoints=True)
        (directory / (name + '-comparison.json')).write_text(json.dumps(result, indent=2)+'\n')
        if not result['passed']:
            raise RuntimeError(name + ': serialized output differs; inspect the comparison report')
        summaries.append({'reference':name, 'passed':True, 'product_events':result['product_events'],
                          'bytes_compared':result['bytes_compared']})
    (directory / 'summary.json').write_text(json.dumps(summaries, indent=2)+'\n')


if __name__ == '__main__':
    main()
