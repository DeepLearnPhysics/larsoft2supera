"""Read-only parser validation and timing; no reconstruction, labeling, or LArCV output."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess


def configuration(*, fixture=False, mode='valid', with_hits=True, verify=True, products=None, association='', other=False):
    source = 'EmptyEvent firstRun: 1 firstSubRun: 1 firstEvent: 1' if fixture else 'RootInput'
    inputs = ['fixture:points'] if fixture else ['cluster3DCryoE', 'cluster3DCryoW']
    if products is None:
        products = ['fixture:hits', 'fixture:other'] if fixture else []
    producer = f'producers: {{ fixture: {{module_type: SpacePointFixture Mode: "{mode}"}} }} produce: [fixture] trigger_paths: [produce]' if fixture else ''
    return f'''process_name: SpacePointRead
source: {{ module_type: {source} }}
services: {{ scheduler: {{ num_threads: 1 num_schedules: 1 }}
 TimeTracker: {{ printSummary: true dbOutput: {{ filename: "TimingReport.db" }} }}
 MemoryTracker: {{ dbOutput: {{ filename: "MemoryReport.db" }} }} }}
physics: {{
 {producer}
 analyzers: {{ check: {{ module_type: SpacePointCheck Inputs: {json.dumps(inputs)}
 HitProducts: {json.dumps(products)} WithHits: {str(with_hits).lower()}
 Verify: {str(verify).lower()} Fixture: {str(fixture).lower()}
 Association: "{association}" Report: "timings.csv"
 OtherProducts: {str(other).lower()} ProductReport: "products.csv"
 }} }}
 output: [check] end_paths: [output]
}}
'''


def run(directory, name, config, source=None, error=None):
    case = directory / name
    case.mkdir()
    (case / 'job.fcl').write_text(config)
    command = ['lar', '-c', str(case / 'job.fcl'), '-n', '10' if source else '3']
    if source:
        command += ['-s', str(source)]
    with (case / 'run.log').open('w') as output:
        result = subprocess.run(command, cwd=case, stdout=output, stderr=subprocess.STDOUT)
    if error:
        if not result.returncode or error not in (case / 'run.log').read_text():
            raise RuntimeError(f'{name}: expected failure containing {error}')
        return []
    if result.returncode:
        raise RuntimeError(f'{name}: see run.log')
    with (case / 'timings.csv').open() as stream:
        return list(csv.DictReader(stream))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--repeats', type=int, default=3)
    args = parser.parse_args()
    if args.repeats < 0:
        parser.error("--repeats must be nonnegative")
    digest = hashlib.sha256()
    with args.input.open("rb") as stream:
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            digest.update(block)
    expected = "604b888f408a0c561a9d2df3b673bbc781989f23bc5b6a26d4a1c7f5e6da71f4"
    if digest.hexdigest() != expected:
        raise RuntimeError("Input differs from the pinned cached sample")
    directory = args.output.resolve()
    directory.mkdir(parents=True)
    run(directory, 'fixture', configuration(fixture=True))
    run(directory, 'points-only-missing-table', configuration(fixture=True, with_hits=False, association='absent'))
    for name, config, error in [
        ('bad-point-key', configuration(fixture=True, mode='bad-point-key'), 'SpacePoint key exceeds'),
        ('null-point', configuration(fixture=True, mode='null-point'), 'Null or foreign SpacePoint reference'),
        ('bad-key', configuration(fixture=True, mode='bad-key'), 'Hit key exceeds'),
        ('null', configuration(fixture=True, mode='null'), 'Null or unconfigured'),
        ('foreign', configuration(fixture=True, products=['fixture:hits']), 'Null or unconfigured'),
        ('duplicate-product', configuration(fixture=True, products=['fixture:hits', 'fixture:hits']), 'Duplicate SpacePoint hit product'),
        ('missing-table', configuration(fixture=True, association='absent'), 'ProductNotFound'),
        ('missing-hit-product', configuration(fixture=True, products=['absent']), 'ProductNotFound'),
    ]:
        run(directory, name, config, error=error)
    validation = run(directory, 'overlay-verified', configuration(other=True), args.input.resolve())
    totals = {key: sum(int(row[key]) for row in validation) for key in ['points','links','unique_hits']}
    if len(validation) != 20 or totals != {'points':8839871, 'links':20490298, 'unique_hits':383132}:
        raise RuntimeError(f'Cached sample inventory differs: {totals}')
    for repetition in range(args.repeats):
        run(directory, f'points-only-{repetition}', configuration(with_hits=False, verify=False), args.input.resolve())
        run(directory, f'associated-{repetition}', configuration(verify=False, other=True), args.input.resolve())
    (directory / 'summary.json').write_text(json.dumps({'passed': True, 'input_sha256':digest.hexdigest(), 'verified_inventory':totals,
        'expected_failures':8, 'performance_repetitions':args.repeats,
        'note':'Verification is outside timed regions; no LArCV output or voxel selection.'}, indent=2)+'\n')


if __name__ == '__main__':
    main()
