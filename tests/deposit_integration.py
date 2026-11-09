"""Native deposit round-trip and explained legacy comparison on the frozen ICARUS fixture."""
import argparse
import csv
import json
import os
from pathlib import Path
import statistics
import subprocess

from compare_deposits import compare
from audit_deposit_differences import audit


def run(command, output, log, env):
    with (output / log).open('w') as stream:
        subprocess.run(command, cwd=output, env=env, stdout=stream, stderr=subprocess.STDOUT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lab', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    lab = args.lab.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    env = os.environ.copy()
    env['FHICL_FILE_PATH'] = str(lab / 'config') + ':' + env.get('FHICL_FILE_PATH', '')
    source = lab / 'data/icarus-overlay-cluster3d-v10_27_00/overlay_cluster3d_cache.root'
    profile = Path(__file__).resolve().parents[1] / 'fcl/deposit_icarus.fcl'
    run(['lar', '-c', str(profile), '-n', '10', '-s', str(source)], output, 'run.log', env)

    # Geometry is used only to audit the legacy acceptance, not by the deposit reader.
    config = output / 'check.fcl'
    config.write_text('''#include "supera_from_cache.fcl"
process_name: DepositSourceCheck
physics.producers: {}
physics.reco: []
physics.trigger_paths: []
physics.analyzers: { check: {
 module_type: DepositCheck AuditGeometry: true Archive: ''' +
                      json.dumps(str(output / 'deposits.root.deposits.root')) + '''
} }
physics.outana: [check]
physics.end_paths: [outana]
''')
    run(['lar', '-c', str(config), '-n', '10', '-s', str(source)], output, 'source-check.log', env)
    bounds = json.loads((output / 'legacy-active-bounds.json').read_text())
    references = [lab / 'results/staged-full-01/supera_reference.root',
                  lab / 'results/standard-full-01/larcv_mc_20261107_001608_861108.root']
    for index, reference in enumerate(references):
        report = compare(output / 'deposits.root', reference)
        assert report['accounting']['deposit_energy']['records'] == 175443
        assert report['accounting']['full_steps']['records'] == 220719
        assert report['accounting']['deposit_energy']['counts'][3] == 0
        assert report['accounting']['readout']['records'] == 352351
        assert all(item['invalid_energy'] == 0 and item['counts'][4] == 0
                   for item in report['accounting'].values())
        (output / f'comparison-{index}.json').write_text(json.dumps(report, indent=2) + '\n')
        explanation = audit(output / 'deposits.root', reference, bounds)
        (output / f'differences-{index}.json').write_text(json.dumps(explanation, indent=2) + '\n')
    with (output / 'deposit-timing.csv').open() as stream:
        rows = list(csv.DictReader(stream))
    timing = {name: {field: statistics.median(float(row[field]) for row in rows
                                             if row['collection'] == name)
                     for field in ('read_s', 'attribute_s')}
              for name in sorted({row['collection'] for row in rows})}
    with (output / 'transport-timing.csv').open() as stream:
        timing['all_output_write_s'] = statistics.median(float(row['write_s']) for row in csv.DictReader(stream))
    (output / 'timing-summary.json').write_text(json.dumps(timing, indent=2) + '\n')


if __name__ == '__main__':
    main()
