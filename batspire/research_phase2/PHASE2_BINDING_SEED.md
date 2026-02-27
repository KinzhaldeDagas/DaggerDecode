# Phase 2 Binding Seed

This artifact seeds `texture_tag_bindings.csv` for manual/iterative tag->BSI mapping.

- total tags: **4625**
- total faces represented: **134754**
- bound faces: **0** (ratio **0.00%**)
- status counts: `{'fallback_unknown': 1, 'unbound': 4624}`

## Next actions
1. Manually confirm high-frequency non-zero tags and assign `bound_bsi_stem`.
2. Re-run this script + add an assert script to require monotonically increasing bound coverage.
3. Use per-level context to disambiguate tags reused across many models.