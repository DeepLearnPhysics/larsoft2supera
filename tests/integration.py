"""Run in a configured ICARUS release with installed writer and test plugins visible.

The output directory must be new. No reconstruction is scheduled. Each reference
is compared exactly; the fixture additionally exercises empty collections in art.
"""
import argparse
import json
from pathlib import Path
import subprocess

from compare_optical import compare, load_comparator
from comparator_checks import check_comparator
import ROOT


def run_case(directory, name, config, source=None, events=3, error=None):
    case = directory / name
    case.mkdir()
    configuration = case / "job.fcl"
    configuration.write_text(config)
    command = ["lar", "-c", str(configuration), "-n", str(events)]
    if source:
        command += ["-s", str(source)]
    with (case / "run.log").open("w") as log:
        result = subprocess.run(command, cwd=case, stdout=log, stderr=subprocess.STDOUT)
    if error:
        if result.returncode == 0 or error not in (case / "run.log").read_text():
            raise RuntimeError(f"{name}: expected failure containing {error}")
    elif result.returncode:
        raise RuntimeError(f"{name}: lar failed; see {case / 'run.log'}")
    return case / "optical.root"


def fixture_config(collections, output='"optical.root"'):
    return '''process_name: OpticalTest
source: { module_type: EmptyEvent firstRun: 1 firstSubRun: 1 firstEvent: 1 }
services: { scheduler: { num_threads: 1 num_schedules: 1 } }
physics: {
 producers: { fixture: { module_type: OpticalFixture } }
 analyzers: { optical: { module_type: OpticalWriter OutputFile: OUTPUT Collections: COLLECTIONS } }
 produce: [fixture]
 output: [optical]
 trigger_paths: [produce]
 end_paths: [output]
}
'''.replace("OUTPUT", output).replace("COLLECTIONS", collections)


def inspect_fixture(path):
    file = ROOT.TFile.Open(str(path))
    if not file or file.IsZombie():
        raise RuntimeError("Cannot open fixture output")
    try:
        for label, counts in (("cryoE", [2, 0, 1]), ("cryoW", [0, 1, 0])):
            tree = file.Get(f"opflash_{label}_tree")
            if not tree or tree.GetEntries() != 3:
                raise RuntimeError("Wrong fixture entry count")
            for index, count in enumerate(counts):
                if tree.GetEntry(index) <= 0:
                    raise RuntimeError("Cannot read fixture entry")
                record = getattr(tree, f"opflash_{label}_branch")
                if (record.run(), record.subrun(), record.event()) != (1, 1, index + 1):
                    raise RuntimeError("Incorrect event identity")
                flashes = record.as_vector()
                if len(flashes) != count:
                    raise RuntimeError("Stale or missing fixture flash")
                for key, flash in enumerate(flashes):
                    if flash.id() != key or flash.time() != -12.5:
                        raise RuntimeError("Incorrect flash identity or value")
    finally:
        file.Close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--reference", required=True, action="append", type=Path)
    parser.add_argument("--expected-ids", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    for path in [args.input, args.expected_ids, *args.reference]:
        if not path.is_file():
            raise FileNotFoundError(path)
    directory = args.output.resolve()
    directory.mkdir(parents=True, exist_ok=False)
    ids = json.loads(args.expected_ids.read_text())
    if not ids or len(ids) != len(set(ids)):
        raise ValueError("Expected IDs must be nonempty and unique")
    config = (Path(__file__).resolve().parent.parent / "fcl/optical_icarus.fcl").read_text()
    output = run_case(directory, "overlay", config, args.input.resolve(), len(ids))
    load_comparator()
    check_comparator(directory / "comparator-checks")
    reports = []
    for index, reference in enumerate(args.reference):
        report = compare(reference.resolve(), output, ids)
        (directory / f"comparison-{index}.json").write_text(json.dumps(report, indent=2) + "\n")
        if not report["passed"]:
            raise RuntimeError("Reference mismatch")
        reports.append({key: value for key, value in report.items() if key != "records"})
    collections = '[{InputTag:"fixture:east" OutputLabel:"cryoE"},' \
                  '{InputTag:"fixture:west" OutputLabel:"cryoW"}]'
    inspect_fixture(run_case(directory, "empty-collections", fixture_config(collections)))
    failures = {
        "missing-product": (collections.replace("fixture:east", "absent"), "ProductNotFound"),
        "wrong-process": (collections.replace("fixture:east", "fixture:east:Wrong"), "ProductNotFound"),
        "duplicate-output": (collections.replace("cryoW", "cryoE"), "outputs must be unique"),
        "empty-input": (collections.replace("fixture:east", ""), "must be nonempty"),
        "empty-label": (collections.replace("cryoE", ""), "must be nonempty"),
        "no-collections": ("[]", "must not be empty"),
    }
    for name, (value, error) in failures.items():
        run_case(directory, name, fixture_config(value), error=error)
    run_case(directory, "empty-output", fixture_config(collections, '""'),
             error="must not be empty")
    # The comparator must reject a different event sequence/count.
    try:
        compare(args.reference[0].resolve(), directory / "empty-collections/optical.root", ids)
    except Exception:
        pass
    else:
        raise RuntimeError("Comparator accepted an intentionally wrong event count")
    summary = {"passed": True, "reference_comparisons": reports,
               "empty_collection_sequence": "passed", "expected_failures": len(failures) + 1,
               "comparator_wrong_count_rejected": True}
    (directory / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
