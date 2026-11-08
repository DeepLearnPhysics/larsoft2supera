"""Gate shared selection and LArCV projection coverage separately from dependencies."""
import gzip
import json
from pathlib import Path
import sys


def main(directory):
    wanted = ('/process/SelectSpacePoints.cxx', '/io/larcv2/SpacePoint.cxx')
    summaries = {}
    for report in directory.glob('*.gcov.json.gz'):
        with gzip.open(report, 'rt') as stream:
            files = json.load(stream)['files']
        for entry in files:
            if not entry['file'].endswith(wanted):
                continue
            name = next(suffix for suffix in wanted if entry['file'].endswith(suffix))
            if name in summaries:
                raise RuntimeError('Ambiguous duplicate coverage report: ' + name)
            lines = entry['lines']
            functions = entry['functions']
            summary = {
                'line_covered': sum(line['count'] > 0 for line in lines),
                'line_total': len(lines),
                'function_covered': sum(function['execution_count'] > 0 for function in functions),
                'function_total': len(functions),
            }
            summaries[name] = summary
    (directory / 'summary.json').write_text(json.dumps(summaries, indent=2) + '\n')
    if set(summaries) != set(wanted):
        raise RuntimeError('Missing selection or projection coverage')
    for name, summary in summaries.items():
        if not summary['line_total'] or summary['line_covered'] / summary['line_total'] < .9:
            raise RuntimeError(name + ': line coverage below 90%')
        if not summary['function_total'] or summary['function_covered'] != summary['function_total']:
            raise RuntimeError(name + ': function coverage below 100%')
    print(json.dumps(summaries, indent=2))


if __name__ == '__main__':
    main(Path(sys.argv[1]))
