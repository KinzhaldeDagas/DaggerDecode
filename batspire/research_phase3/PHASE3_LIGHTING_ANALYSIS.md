# Phase 3 Lighting/Material Inventory

- BS6 files scanned: **45**
- `AMBI` chunk count: **2452**
- `BRIT` chunk count: **1499**
- `FLAD` chunk count: **2272**
- `FLAS` chunk count: **45**
- `LITD` chunk count: **1499**
- `LITS` chunk count: **45**
- `RAWD` chunk count: **5784**

## Scalar probe ranges (signed int32, exploratory)
| chunk | samples | min | max |
|---|---:|---:|---:|
| `AMBI` | 2452 | 0 | 60000 |
| `BRIT` | 1499 | 11 | 1023 |
| `FLAD` | 213431 | -19331 | 2003659878 |
| `FLAS` | 217975 | -19331 | 2003659878 |
| `LITD` | 30010 | -19443 | 1498694729 |
| `LITS` | 33008 | -19443 | 1498694729 |
| `RAWD` | 27056 | -17150 | 1936280870 |

## Notes
1. This is a structure-first inventory pass for Phase 3 and does not yet decode semantic fields.
2. Use `lighting_payload_lengths.csv` to prioritize reverse-engineering the most common payload shapes.
3. Next step: wire decoded light terms into renderer debug overlays for parity iteration.
