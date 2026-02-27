#!/usr/bin/env python3
import argparse
import csv
import json
from collections import Counter
from pathlib import Path

IN_BINDINGS = Path('batspire/research_phase2/texture_tag_bindings.csv')
IN_FAMILY = Path('batspire/research_phase2/texture_tag_bindings_family_candidates.csv')
OUT_COVERAGE = Path('batspire/research_phase2/phase2_binding_coverage.json')


def load_csv(path: Path):
    with path.open(newline='') as f:
        return list(csv.DictReader(f))


def pick_unique_example(example_field: str) -> str:
    examples = [x.strip() for x in example_field.split('|') if x.strip()]
    if not examples:
        return ''
    return examples[0]


def recompute_coverage(rows):
    total_faces = sum(int(r['face_count']) for r in rows)
    nonzero_faces = sum(
        int(r['face_count']) for r in rows if r['texture_tag_hex'] != '000000000000'
    )
    bound_faces = sum(
        int(r['face_count'])
        for r in rows
        if r['status'].startswith('bound_') and r['bound_bsi_stem'].strip()
    )
    bound_nonzero_faces = sum(
        int(r['face_count'])
        for r in rows
        if r['texture_tag_hex'] != '000000000000'
        and r['status'].startswith('bound_')
        and r['bound_bsi_stem'].strip()
    )

    return {
        'total_tags': len(rows),
        'total_faces': total_faces,
        'nonzero_faces': nonzero_faces,
        'bound_faces': bound_faces,
        'bound_nonzero_faces': bound_nonzero_faces,
        'bound_face_ratio': 0.0 if total_faces == 0 else (bound_faces / total_faces),
        'bound_face_ratio_nonzero': 0.0 if nonzero_faces == 0 else (bound_nonzero_faces / nonzero_faces),
        'status_counts': dict(Counter(r['status'] for r in rows)),
    }


def main():
    parser = argparse.ArgumentParser(description='Apply conservative unique-family Phase 2 bindings.')
    parser.add_argument('--min-face-count', type=int, default=1)
    parser.add_argument('--allow-families', default='wal,sheet,basket,treb')
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()

    allow_families = {f.strip().lower() for f in args.allow_families.split(',') if f.strip()}

    bindings = load_csv(IN_BINDINGS)
    families = {r['texture_tag_hex']: r for r in load_csv(IN_FAMILY)}

    changed = 0
    changed_faces = 0
    for row in bindings:
        if row['texture_tag_hex'] == '000000000000':
            continue
        if row['status'] != 'unbound':
            continue

        fam = families.get(row['texture_tag_hex'])
        if not fam:
            continue

        dominant_family = fam['dominant_model_family'].strip().lower()
        family_count = int(fam['bsi_family_count'])
        face_count = int(row['face_count'])

        if dominant_family not in allow_families:
            continue
        if family_count != 1:
            continue
        if face_count < args.min_face_count:
            continue

        stem = pick_unique_example(fam['bsi_family_examples'])
        if not stem:
            continue

        row['status'] = 'bound_family_unique'
        row['bound_bsi_stem'] = stem
        row['confidence'] = 'medium'
        row['notes'] = (
            f"auto-bound via unique family candidate ({dominant_family}, min_face_count={args.min_face_count})"
        )
        changed += 1
        changed_faces += face_count

    coverage = recompute_coverage(bindings)
    if not args.dry_run:
        with IN_BINDINGS.open('w', newline='') as f:
            fieldnames = [
                'texture_tag_hex',
                'status',
                'bound_bsi_stem',
                'confidence',
                'face_count',
                'model_count',
                'candidate_bsi_stems',
                'notes',
            ]
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(bindings)
        OUT_COVERAGE.write_text(json.dumps(coverage, indent=2, sort_keys=True) + '\n')

    print(f'updated bindings: {changed}')
    print(f'updated faces: {changed_faces}')
    print(f"bound_nonzero_faces={coverage['bound_nonzero_faces']} / {coverage['nonzero_faces']}")
    print(f"bound_face_ratio_nonzero={coverage['bound_face_ratio_nonzero']:.6f}")
    if args.dry_run:
        print('dry-run enabled: files were not written')
    else:
        print('wrote', IN_BINDINGS)
        print('wrote', OUT_COVERAGE)


if __name__ == '__main__':
    main()
