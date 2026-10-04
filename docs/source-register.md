# Source register

Every source cited in `evidence-register.md`. A claim may be marked CONFIRMED only when its source
is listed here with enough detail for someone else to find the same page.

Original documents, firmware and ROM data are **not** stored in this repository (plan §1, §16).
Record bibliographic details and page numbers only.

| ID | Source | Type | Location / reference | Catalogued | Notes |
|----|--------|------|----------------------|------------|-------|
| SRC-000 | A5 implementation plan | Project specification | `docs/A5_implementation_plan.md` | 2026-10-04 | Secondary. States the architecture in §3 as documented, but does not name the documents or pages. |
| SRC-001 | Owner's manual for the red A5 Guitar | Owner manual | *Not yet catalogued* | — | Needed for the parameter table, program layout and output description. |
| SRC-002 | Service manual / schematic | Service documentation | *Not yet catalogued* | — | Needed for the chip list, sample rate and memory size. |

## How to add a source

1. Give it the next `SRC-` number.
2. Record edition, date, language and page count where known, so a reader can tell two printings apart.
3. In `evidence-register.md`, cite it as `SRC-nnn p. x` against each claim it supports.
