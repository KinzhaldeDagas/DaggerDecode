# Phase 2 Binding Analysis — Action Plan

- Total faces in inventory: **134754**
- Non-zero-tag faces: **10452** (7.76%)
- Non-zero faces with family candidates: **3444** (32.95% of non-zero)
- Non-zero faces with no family candidates: **7008** (67.05% of non-zero)

## Family buckets by non-zero face coverage
| family | face_count |
|---|---:|
| `7` | 3393 |
| `l` | 3103 |
| `bsp` | 1405 |
| `a` | 240 |
| `wal` | 189 |
| `earrow` | 161 |
| `sc` | 157 |
| `dng` | 136 |
| `tower` | 108 |
| `t` | 102 |
| `qcove` | 71 |
| `ejav` | 61 |
| `ebrdswrd` | 60 |
| `emace` | 50 |
| `isle` | 49 |
| `nnslabs` | 44 |
| `land` | 44 |
| `sheet` | 43 |
| `twr` | 37 |
| `sigill` | 36 |

## Priority execution plan (top-100 non-zero tags)
1. Manually bind top 20 tags from `texture_tag_binding_priority_top100.csv`.
2. Recompute `phase2_binding_coverage.json` and require positive delta in bound_face_ratio.
3. For tags with zero family candidates, add level-context sampling before manual assignment.


## Current progress checkpoint
- Applied unique-family auto-binding pass for high-signal families (`wal`, `sheet`, `basket`, `treb`) at `face_count >= 1` for unique-family candidates.
- Newly bound tags: **134**
- Newly bound non-zero faces: **289 / 10452** (**2.77%**)

## Suggested working cadence
1. Work in 10-tag batches from `texture_tag_binding_priority_top100.csv`.
2. After each batch, regenerate analysis outputs and verify that newly-bound tags increased
   non-zero tagged face coverage.
3. Keep a short rationale note per manual binding (source stem, uncertainty, and fallback) to
   make future corrections cheap.

## Regeneration command
```bash
python tools/misc/batspire_phase2_binding_analysis.py
python tools/misc/batspire_phase2_coverage_gate.py --min-nonzero-ratio 0.02
```

## Exit criteria for this pass
- At least **50/100** priority tags manually reviewed.
- At least **+10%** relative increase in `bound_face_ratio_nonzero` versus current baseline.
- No regressions in CSV row counts for `texture_tag_bindings.csv`.

## Artifacts
- `texture_tag_binding_priority_top100.csv`
- `PHASE2_BINDING_ACTION_PLAN.md`
