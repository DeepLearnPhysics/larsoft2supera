"""Run the ICARUS transport profile, independent source check, and legacy comparison."""
import argparse
import csv
import json
from pathlib import Path
import statistics
import subprocess

from compare_transport import compare


def run(command, directory, log):
    with (directory / log).open('w') as stream:
        subprocess.run(command, cwd=directory, stdout=stream, stderr=subprocess.STDOUT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True, type=Path)
    parser.add_argument('--reference', required=True, action='append', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    source = args.input.resolve()
    config = Path(__file__).resolve().parents[1] / 'fcl/transport_icarus.fcl'
    run(['lar', '-c', str(config), '-n', '10', '-s', str(source)], output, 'run.log')

    archive = output / 'transport.root.transport.root'
    checker = output / 'source-check.fcl'
    checker.write_text('''process_name: TransportSourceCheck
source: { module_type: RootInput }
services: { scheduler: { num_threads: 1 num_schedules: 1 } }
physics: {
  analyzers: { check: { module_type: TransportCheck Archive: ''' + json.dumps(str(archive)) + ''' } }
  output: [check]
  end_paths: [output]
}
''')
    run(['lar', '-c', str(checker), '-n', '10', '-s', str(source)], output, 'source-check.log')
    for index, reference in enumerate(args.reference):
        report = compare(output / 'transport.root', reference.resolve())
        # Frozen cosmic-muon fixture expectations; not claims about neutrino completeness.
        assert report['new_particles'] == 1493 and report['source_primaries'] == 30
        assert report['unresolved_primary_affiliation'] == 0
        assert report['missing_direct_parents'] == 12
        assert report['new_overflowed_parent_times'] == 0
        (output / f'comparison-{index}.json').write_text(json.dumps(report, indent=2) + '\n')
    with (output / 'transport-timing.csv').open() as stream:
        rows = list(csv.DictReader(stream))
    timing = {name: statistics.median(float(row[name]) for row in rows)
              for name in ('read_s', 'resolve_s', 'write_s')}
    (output / 'timing-summary.json').write_text(json.dumps(timing, indent=2) + '\n')


if __name__ == '__main__':
    main()
