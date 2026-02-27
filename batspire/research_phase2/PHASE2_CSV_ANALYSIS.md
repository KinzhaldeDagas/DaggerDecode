# Phase 2 CSV Analysis (Texture Mapping Readiness)

## Inventory recap
- Distinct texture tags: **4625**
- Total faces represented in tag inventory: **134754**
- BSI candidate files indexed: **2599**
- Distinct TEXI directories in BS6: **1**

## Concentration observations
- Top 10 tags account for **125421 / 134754** faces (**93.07%**).
- Dominant null-like tag `000000000000` appears on **124302** faces across **2188** models.
- Non-zero tags count: **4624** (likely useful for concrete binding experiments).

## Top texture tags (from CSV)
| rank | tag | face_count | model_count | sample_models |
|---:|---|---:|---:|---|
| 1 | `000000000000` | 124302 | 2188 | 1-3.3D | 1.3D | 10.3D | 11.3D | 12-2.3D | 12.3D | 13.3D | 14.3D |
| 2 | `0000000000ff` | 261 | 38 | 7PYLON1.3D | 7PYLON2.3D | A06.3D | BSP_LVA0.3D | BSP_WAL8.3D | CUIRASS.3D | EDAGGER.3D | ELOSWRD.3D |
| 3 | `000000000001` | 217 | 42 | 7BRID1.3D | 7BRID2.3D | 7BRID3.3D | 7BRID4.3D | 7BRID5.3D | 7FORM1.3D | 7MECH.3D | 7PYLON1.3D |
| 4 | `00ffffff0000` | 108 | 29 | A06.3D | BSP_CAT2.3D | BSP_CL16.3D | BSP_WAL8.3D | CUIRASS.3D | ELOSWRD.3D | EMACE.3D | ESPEAR.3D |
| 5 | `000100000000` | 102 | 33 | A06.3D | BSP_LVA0.3D | CUIRASS.3D | EDAGGER.3D | ELOSWRD.3D | EMACE.3D | ESPEAR.3D | ESTAFF.3D |
| 6 | `070000000800` | 97 | 95 | 7BIS1.3D | 7BIS2.3D | 7BIS3.3D | 7BOLT.3D | 7BOULDR.3D | 7BROCKO.3D | 7CLIFF.3D | 7FORM2.3D |
| 7 | `0d0000000e00` | 93 | 91 | 7BIS1.3D | 7BIS2.3D | 7BIS3.3D | 7BOLT.3D | 7BOULDR.3D | 7BROCKO.3D | 7CLIFF.3D | 7FORM2.3D |
| 8 | `130000001400` | 84 | 82 | 7BIS1.3D | 7BIS2.3D | 7BIS3.3D | 7BOLT.3D | 7BOULDR.3D | 7BROCKO.3D | 7CLIFF.3D | 7FORM2.3D |
| 9 | `0000803f0000` | 79 | 45 | 7BAKUP.3D | 7DAGTOP2.3D | 7LAVA14.3D | 7LAVA17.3D | 7LAVA5.3D | BSP_BTN1.3D | BSP_CL06.3D | BSP_CL11.3D |
| 10 | `190000001a00` | 78 | 76 | 7BIS1.3D | 7BIS2.3D | 7BIS3.3D | 7BOLT.3D | 7BROCKO.3D | 7CLIFF.3D | 7FORM2.3D | 7FORM3.3D |

## Tag prefix heuristic (first 2 bytes hex)
| prefix | tag_count |
|---|---:|
| `0000` | 974 |
| `3c00` | 62 |
| `0400` | 55 |
| `0100` | 53 |
| `0800` | 51 |
| `0c00` | 51 |
| `0080` | 45 |
| `1000` | 41 |
| `0300` | 41 |
| `1400` | 40 |
| `ffff` | 37 |
| `4800` | 33 |

## TEXI directory signals
| file | texi_dir_count | texi_dirs |
|---|---:|---|
| 1ROOM.BS6 | 1 | e:\projects\batspire\art\tex_cels\hicolor |
| BACKUP.BS6 | 1 | e:\projects\batspire\art\tex_cels\hicolor |
| BARCKIN.BS6 | 1 | e:\projects\batspire\art\tex_cels\hicolor |
| BARNIN.BS6 | 1 | e:\projects\batspire\art\tex_cels\hicolor |
| BELLIN.BS6 | 1 | e:\projects\batspire\art\tex_cels\hicolor |
| BLKSMIN.BS6 | 1 | e:\projects\batspire\art\tex_cels\hicolor |
| CATCOM.BS6 | 1 | e:\projects\batspire\art\tex_cels\hicolor |
| DMKK.BS6 | 1 | e:\projects\batspire\art\tex_cels\hicolor |
| DMNN.BS6 | 1 | e:\projects\batspire\art\tex_cels\hicolor |
| DMSC.BS6 | 1 | e:\projects\batspire\art\tex_cels\hicolor |

## BSI filename inventory signals
| extension | count |
|---|---:|
| `.bsi` | 2592 |
| `.txt` | 2 |
| `.exe` | 1 |
| `` | 1 |
| `.err` | 1 |
| `.bat` | 1 |
| `.lst` | 1 |

## Recommended Phase 2 hardening steps
1. Treat `000000000000` as fallback/unknown mapping candidate and prioritize non-zero tags for initial deterministic mapping.
2. Implement an explicit tag-binding table artifact (`texture_tag_bindings.csv`) keyed by 6-byte tag and optional model/level context.
3. Normalize BS6 TEXI DIRN paths (DOS root stripping, lowercase, slash normalization) and resolve against local extracted texture roots.
4. Add a validation script that reports bind-coverage (% faces with concrete texture assets) to track Phase 2 exit criteria (>95%).

## Binding seed status
- Seed table generated: `texture_tag_bindings.csv`
- Coverage snapshot: `phase2_binding_coverage.json` (currently 0% bound by design; baseline for monotonic improvement).
