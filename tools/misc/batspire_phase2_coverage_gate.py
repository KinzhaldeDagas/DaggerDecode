#!/usr/bin/env python3
import argparse
import json
from pathlib import Path


def load_json(path: Path):
    return json.loads(path.read_text())


def main():
    parser = argparse.ArgumentParser(description='Phase 2 binding coverage gate checker.')
    parser.add_argument('--current', default='batspire/research_phase2/phase2_binding_coverage.json')
    parser.add_argument('--baseline', help='Optional baseline coverage JSON to compare against.')
    parser.add_argument('--min-nonzero-ratio', type=float, default=0.01)
    args = parser.parse_args()

    current = load_json(Path(args.current))
    ratio = float(current.get('bound_face_ratio_nonzero', 0.0))

    failures = []
    if ratio < args.min_nonzero_ratio:
        failures.append(
            f'bound_face_ratio_nonzero {ratio:.6f} < min {args.min_nonzero_ratio:.6f}'
        )

    if args.baseline:
        baseline = load_json(Path(args.baseline))
        base_ratio = float(baseline.get('bound_face_ratio_nonzero', 0.0))
        if ratio < base_ratio:
            failures.append(
                f'bound_face_ratio_nonzero regressed: {ratio:.6f} < baseline {base_ratio:.6f}'
            )

    if failures:
        print('Phase 2 coverage gate: FAIL')
        for msg in failures:
            print('-', msg)
        raise SystemExit(1)

    print('Phase 2 coverage gate: PASS')
    print(f'- bound_face_ratio_nonzero={ratio:.6f}')
    print(f"- bound_nonzero_faces={current.get('bound_nonzero_faces', 0)}")
    print(f"- nonzero_faces={current.get('nonzero_faces', 0)}")


if __name__ == '__main__':
    main()
