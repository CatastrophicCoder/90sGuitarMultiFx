# Source register

Every source cited in `evidence-register.md`. A claim may be marked CONFIRMED only when its source
is listed here with enough detail for someone else to find the same page.

Original documents, firmware and ROM data are **not** stored in this repository (plan §1, §16).
Record bibliographic details and page numbers only.

| ID | Source | Type | Location / reference | Catalogued | Notes |
|----|--------|------|----------------------|------------|-------|
| SRC-000 | Implementation plan | Project specification | `docs/implementation-plan.md` | 2026-10-04 | Secondary. States the architecture in §3 as documented, but does not name the documents or pages. |
| SRC-001 | Owner's manual of the original unit, guitar model (English, edition mark "E4", 1992, print code "0402 FGH", 14 printed pages + covers; PDF 17 pages, 602 717 bytes, SHA-256 `9800f0b9…6d60dd7`) | Owner manual | Manufacturer's support download (one personal copy). Local copy outside the repository. | 2026-10-05 | Cited by printed page number; PDF page = printed page + 2. Scanned, no text layer. Contains controls, connections, program layout, chain diagram, full effect parameter list and specifications. The factory preset values (a separate "Effect Parameter List" sheet) are **not** in this PDF. |
| SRC-002 | Service manual of the original unit, covering its guitar, multi-effect and bass models (English, 1991; 35 PDF pages, 2 560 760 bytes, SHA-256 `33eae2c4…cd68684de28d`) | Service manual | Third-party scan. Local copy outside the repository; only cited, never copied. | 2026-10-05 | Cited by **PDF page** (printed numbers differ). Specifications, block diagrams, schematics, circuit description, pin tables, test mode, parts lists. Does not describe how effects are computed. |
| SRC-003 | Photograph of the original unit's front panel, top view (2048 × 1421 px, 291 645 bytes) | Reference photograph | Supplied by the owner 2026-10-05; photographer and licence unknown. Local copy outside the repository, never committed. | 2026-10-05 | Visual reference only (layout, label text, colours under unknown lighting). Agrees with the front-panel drawing in SRC-001 p. 2. Nothing is traced or cut from it. |

## How to add a source

1. Give it the next `SRC-` number.
2. Record edition, date, language and page count where known, so a reader can tell two printings apart.
3. In `evidence-register.md`, cite it as `SRC-nnn p. x` against each claim it supports.
