"""Prove that optical comparison detects small value and structural changes."""
import json
import math

import ROOT

from compare_optical import compare


def check_comparator(directory):
    directory.mkdir()

    def write(name, value=1.0, event=1, count=1, west=True, auxiliary=False, pe=1.):
        path = directory / (name + ".root")
        file = ROOT.TFile(str(path), "RECREATE")
        for label in (["cryoE", "cryoW"] if west else ["cryoE"]):
            tree = ROOT.TTree(f"opflash_{label}_tree", "")
            record = ROOT.larcv.EventFlash()
            tree.Branch(f"opflash_{label}_branch", record)
            record.set_id(1, 1, event)
            flash = ROOT.larcv.Flash()
            flash.id(0)
            flash.time(value)
            record.append(flash)
            for _ in range(count):
                tree.Fill()
            tree.Write()
        if auxiliary:
            tree = ROOT.TTree("crthit_crthit_tree", "")
            record = ROOT.larcv.EventCRTHit()
            record.set_id(1, 1, event)
            tree.Branch("crthit_crthit_branch", record)
            hit = ROOT.larcv.CRTHit()
            hit.id(0)
            # The legacy constructor leaves scalar fields uninitialized.
            for field in ("ts0_s", "ts0_s_corr", "ts0_ns", "ts0_ns_corr", "ts1_ns",
                          "plane", "x_pos", "x_err", "y_pos", "y_err", "z_pos", "z_err"):
                getattr(hit, field)(0)
            hit.peshit(pe)
            record.append(hit)
            tree.Fill()
            tree.Write()
        file.Close()
        return path

    original = write("original")
    ids = ["1:1:1"]
    checks = {
        "identical": compare(original, write("same"), ids)["passed"],
        "one_double_ulp": not compare(original, write("ulp", math.nextafter(1., 2.)), ids)["passed"],
        "event_id": not compare(original, write("id", event=2), ids)["passed"],
    }
    auxiliary_original = write("auxiliary-original", auxiliary=True)
    checks["auxiliary_identical"] = compare(auxiliary_original, write("auxiliary-same", auxiliary=True), ids, auxiliary=True)["passed"]
    checks["auxiliary_float_ulp"] = not compare(auxiliary_original, write("auxiliary-ulp", auxiliary=True, pe=1. + 2.**-23), ids, auxiliary=True)["passed"]
    try:
        compare(auxiliary_original, original, ids, auxiliary=True)
    except Exception:
        checks["missing_auxiliary"] = True
    else:
        checks["missing_auxiliary"] = False
    for name, path in (("count", write("count", count=2)), ("missing_product", write("missing", west=False))):
        try:
            compare(original, path, ids)
        except Exception:
            checks[name] = True
        else:
            checks[name] = False
    (directory / "checks.json").write_text(json.dumps(checks, indent=2) + "\n")
    if not all(checks.values()):
        raise RuntimeError(f"Comparator self-check failed: {checks}")
