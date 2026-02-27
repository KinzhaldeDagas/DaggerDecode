# Phase 1 CSV Analysis (batspire/research_phase1)

## BS6 diagnostics highlights
- Files: **45**
- Total `OBJD` instances: **7428**
- Model resolution: **7265 resolved / 163 unresolved** (**97.81%** resolved)
- Files with unresolved references: **5**

### Most complex BS6 files by chunk count
| File | chunk_total | objd_count | unresolved_models |
|---|---:|---:|---:|
| FIXIT.BS6 | 10637 | 1030 | 0 |
| L6.BS6 | 9786 | 872 | 0 |
| L4_OLD.BS6 | 7485 | 533 | 0 |
| L1.BS6 | 6341 | 458 | 0 |
| L3.BS6 | 6191 | 501 | 0 |
| L4.BS6 | 5887 | 482 | 0 |
| L7.BS6 | 5489 | 489 | 0 |
| L5.BS6 | 5350 | 518 | 0 |

### BS6 files with unresolved model references
| File | objd_count | resolved_models | unresolved_models |
|---|---:|---:|---:|
| L8OLD.BS6 | 168 | 42 | 126 |
| L12.BS6 | 313 | 282 | 31 |
| JULIAN.BS6 | 62 | 58 | 4 |
| BACKUP.BS6 | 29 | 28 | 1 |
| DMSC.BS6 | 29 | 28 | 1 |

## B3D diagnostics highlights
- Files: **2395**
- Median faces/model: **24**; p95: **240**
- Median points/model: **26**; p95: **192**

### Unique texture-tag cardinality distribution (top)
| unique_texture_tags | model_count |
|---:|---:|
| 1 | 1853 |
| 2 | 64 |
| 4 | 53 |
| 5 | 52 |
| 6 | 42 |
| 3 | 33 |
| 9 | 30 |
| 7 | 28 |

### Largest meshes by face count
| File | point_count | plane_count | face_count | unique_texture_tags |
|---|---:|---:|---:|---:|
| L6TELE02.3D | 631 | 1050 | 1050 | 1 |
| LION1.3D | 367 | 718 | 718 | 1 |
| BARACKS.3D | 672 | 697 | 697 | 1 |
| L6INKEP.3D | 641 | 696 | 696 | 1 |
| L6KEEP1.3D | 635 | 694 | 694 | 1 |
| ISL_SW.3D | 372 | 676 | 676 | 1 |
| LND_F1.3D | 350 | 624 | 624 | 1 |
| SHIP.3D | 362 | 570 | 570 | 1 |
| 61.3D | 296 | 553 | 553 | 1 |
| ISL_NW.3D | 301 | 541 | 541 | 1 |

## RAWD research table summary
| rawd_length | count | hypothesis |
|---:|---:|---|
| 4 | 1683 | unknown |
| 8 | 759 | likely compact params (id/value pairs?) |
| 16 | 983 | unknown |
| 24 | 878 | vector-ish triplets/transform adjunct |
| 28 | 48 | unknown |
| 30 | 55 | unknown |
| 32 | 719 | material/control payload candidate |
| 52 | 659 | unknown |

## Suggested next parser hardening actions
1. Add explicit allow-list handling for unresolved-model BS6 files to avoid noisy diagnostics.
2. Promote rawd_length cohorts (8/24/32/52) into dedicated decode experiments with fixture-based asserts.
3. Add thresholds in CI using `golden_parity_signatures.json` to fail on signature drift.