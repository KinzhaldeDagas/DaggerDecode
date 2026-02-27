# Phase 2 Texture Pipeline Research Inventory

## Corpus signals
- BS6 files scanned: **45**
- Distinct TEXI directory hints: **1**
- 3D models scanned: **2395**
- Distinct texture tags observed: **4625**

## Most common texture tags (face-weighted)
| tag | face_count | model_count |
|---|---:|---:|
| `000000000000` | 124302 | 2188 |
| `0000000000ff` | 261 | 38 |
| `000000000001` | 217 | 42 |
| `00ffffff0000` | 108 | 29 |
| `000100000000` | 102 | 33 |
| `070000000800` | 97 | 95 |
| `0d0000000e00` | 93 | 91 |
| `130000001400` | 84 | 82 |
| `0000803f0000` | 79 | 45 |
| `190000001a00` | 78 | 76 |
| `000000800000` | 74 | 48 |
| `1f0000002000` | 69 | 67 |
| `250000002600` | 59 | 57 |
| `0000e0ab0100` | 58 | 10 |
| `008005000000` | 54 | 5 |
| `010000000000` | 53 | 33 |
| `3c00ee040000` | 46 | 46 |
| `010000000100` | 42 | 19 |
| `3c00ee040200` | 40 | 40 |
| `2b0000002c00` | 38 | 36 |

## TEXI directory hints (top)
| texi_dir | bs6_count |
|---|---:|
| `e:\projects\batspire\art\tex_cels\hicolor` | 45 |

## Interpretation / hardening notes
1. Texture tags are highly reused across many models; mapping likely requires additional tables beyond the 6-byte tag alone.
2. BS6 TEXI `DIRN` strongly indicates texture-cell roots; normalize path casing and DOS roots during runtime resolution.
3. `bsi_extracted` inventory should be used as candidate texture namespace while reverse-engineering tag->asset mapping.

## Output files
- `bs6_texi_directories.csv`
- `texture_tag_inventory.csv`
- `bsi_filename_inventory.csv`