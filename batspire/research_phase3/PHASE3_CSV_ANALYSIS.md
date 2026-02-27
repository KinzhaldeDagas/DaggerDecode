# Phase 3 Lighting/Material CSV Analysis

- BS6 files analyzed: **45**
- Total lighting chunks (excl. RAWD): **7812**
- Total RAWD chunks: **5784**

## Correlation probes (file-level counts)
| pair | r |
|---|---:|
| `litd_vs_brit` | 1.0000 |
| `litd_vs_ambi` | 0.7866 |
| `flad_vs_litd` | 0.7866 |
| `rawd_vs_litd` | 0.8221 |
| `rawd_vs_ambi` | 0.9200 |

## Highest lighting-density files
| file | lighting_total | rawd_count | rawd_to_lighting_ratio |
|---|---:|---:|---:|
| `L4_OLD.BS6` | 718 | 500 | 0.696 |
| `L6.BS6` | 640 | 708 | 1.106 |
| `FIXIT.BS6` | 632 | 682 | 1.079 |
| `L2.BS6` | 606 | 331 | 0.546 |
| `L3.BS6` | 548 | 394 | 0.719 |
| `L1.BS6` | 530 | 517 | 0.975 |
| `L7.BS6` | 446 | 306 | 0.686 |
| `L4.BS6` | 444 | 441 | 0.993 |
| `L12.BS6` | 372 | 190 | 0.511 |
| `L5.BS6` | 366 | 297 | 0.811 |
| `DMNN.BS6` | 230 | 39 | 0.170 |
| `L8.BS6` | 230 | 365 | 1.587 |
| `DMKK.BS6` | 202 | 139 | 0.688 |
| `CATCOM.BS6` | 160 | 29 | 0.181 |
| `L11.BS6` | 128 | 15 | 0.117 |

## Payload concentration highlights
| chunk | distinct_lens | top_len | top_share | top3_share |
|---|---:|---:|---:|---:|
| `AMBI` | 1 | 4 | 1.000 | 1.000 |
| `BRIT` | 1 | 4 | 1.000 | 1.000 |
| `FLAD` | 27 | 336 | 0.632 | 0.828 |
| `FLAS` | 42 | 3472 | 0.044 | 0.133 |
| `LITD` | 2 | 80 | 0.999 | 1.000 |
| `LITS` | 23 | 352 | 0.133 | 0.311 |
| `RAWD` | 8 | 4 | 0.291 | 0.613 |

## Phase 3 decode order (recommended)
1. **AMBI (len=4)** and **BRIT (len=4)** first: single-shape payloads and direct brightness/ambient candidates.
2. **FLAD** next: dominant payload lens likely map to repeated light definition struct variants.
3. **LITD/LITS** after FLAD schema confidence improves; use count alignment with BRIT as a validation signal.
4. **RAWD** last for material/flag interpretation; prioritize levels with highest RAWD:lighting ratio for contrastive diffing.
