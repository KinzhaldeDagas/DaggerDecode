# Texture Tag Binding Analysis (Phase 2)

- total tags: **4625**
- total faces: **134754**
- non-zero-tag faces: **10452**
- bound faces: **289** (**0.21%** of total)
- bound non-zero-tag faces: **289** (**2.77%** of non-zero)
- unbound faces: **10163**
- fallback-zero-tag faces: **124302**
- faces with non-empty BSI family candidates: **127746** (**94.80%**)

## Key finding
Direct model-stem to BSI-stem matching is mostly sparse; family-level heuristics provide candidate clusters but not deterministic bindings.

## Top tags with family candidates
| tag | face_count | dominant_family | bsi_family_count | bsi_examples |
|---|---:|---|---:|---|
| `000000000000` | 124302 | `1` | 45 | 1brg01 \| 1brg02 \| 1cnt01 \| 1cnt02 \| 1cnt03 \| 1cnt04 \| 1cnt05 \| 1cnt06 |
| `008005000000` | 54 | `l` | 7 | l2sky \| l5sky \| l6bgdr1 \| l6mir \| l6sky \| l6win \| l8sky |
| `010000000100` | 42 | `l` | 7 | l2sky \| l5sky \| l6bgdr1 \| l6mir \| l6sky \| l6win \| l8sky |
| `000080bf0000` | 37 | `l` | 7 | l2sky \| l5sky \| l6bgdr1 \| l6mir \| l6sky \| l6win \| l8sky |
| `040000010000` | 19 | `l` | 7 | l2sky \| l5sky \| l6bgdr1 \| l6mir \| l6sky \| l6win \| l8sky |
| `0080feff0000` | 18 | `l` | 7 | l2sky \| l5sky \| l6bgdr1 \| l6mir \| l6sky \| l6win \| l8sky |
| `000000000200` | 17 | `l` | 7 | l2sky \| l5sky \| l6bgdr1 \| l6mir \| l6sky \| l6win \| l8sky |
| `0400feffffff` | 16 | `l` | 7 | l2sky \| l5sky \| l6bgdr1 \| l6mir \| l6sky \| l6win \| l8sky |
| `feffffff0000` | 15 | `l` | 7 | l2sky \| l5sky \| l6bgdr1 \| l6mir \| l6sky \| l6win \| l8sky |
| `000000006901` | 15 | `l` | 7 | l2sky \| l5sky \| l6bgdr1 \| l6mir \| l6sky \| l6win \| l8sky |
| `0080fbff0000` | 15 | `l` | 7 | l2sky \| l5sky \| l6bgdr1 \| l6mir \| l6sky \| l6win \| l8sky |
| `000000005400` | 14 | `l` | 7 | l2sky \| l5sky \| l6bgdr1 \| l6mir \| l6sky \| l6win \| l8sky |
| `4f0000005000` | 14 | `l` | 7 | l2sky \| l5sky \| l6bgdr1 \| l6mir \| l6sky \| l6win \| l8sky |
| `550000005600` | 14 | `l` | 7 | l2sky \| l5sky \| l6bgdr1 \| l6mir \| l6sky \| l6win \| l8sky |
| `08002a010000` | 14 | `l` | 7 | l2sky \| l5sky \| l6bgdr1 \| l6mir \| l6sky \| l6win \| l8sky |
| `0300feffffff` | 13 | `l` | 7 | l2sky \| l5sky \| l6bgdr1 \| l6mir \| l6sky \| l6win \| l8sky |
| `5b0000005c00` | 13 | `l` | 7 | l2sky \| l5sky \| l6bgdr1 \| l6mir \| l6sky \| l6win \| l8sky |
| `000000007800` | 13 | `l` | 7 | l2sky \| l5sky \| l6bgdr1 \| l6mir \| l6sky \| l6win \| l8sky |
| `008001000000` | 13 | `l` | 7 | l2sky \| l5sky \| l6bgdr1 \| l6mir \| l6sky \| l6win \| l8sky |
| `180054010000` | 13 | `l` | 7 | l2sky \| l5sky \| l6bgdr1 \| l6mir \| l6sky \| l6win \| l8sky |

## Recommended hardening steps
1. Add explicit per-level context key (`level + tag`) to binding table to reduce global collisions.
2. Add manual-reviewed bindings for highest non-zero face-count tags first.
3. Add coverage gate script to require increasing bound_face_ratio over time.