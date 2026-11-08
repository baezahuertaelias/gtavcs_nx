# M00 foundation verification

Scope: repository/governance, static discovery, existing scaffold preservation.
No target build, console boot, gameplay, audio playback or performance acceptance
belongs to M00. Those remain unchecked tasks in M01–M11.

## Repository settings observed

Repository: https://github.com/baezahuertaelias/gtavcs_nx

`gh api repos/baezahuertaelias/gtavcs_nx` reports private=true,
default_branch=main, allow_merge_commit=true, allow_squash_merge=false,
allow_rebase_merge=false and delete_branch_on_merge=true. Root main was seeded
with bootstrap commit `6587d65`, then work continued on
`milestone/m00-foundation`. Public source is a registered submodule pinned to
`ecf150e53f87e4656dfe81eacd922839d952b282`; its working tree is unchanged.
Git identity is configured locally to the authenticated account's noreply
identity, avoiding the preexisting unrelated global author default.

Attempted main branch protection with strict required statuses `foundation` and
`milestone-review`, admin enforcement and force-push/deletion disabled. GitHub
returned HTTP 403: "Upgrade to GitHub Pro or make this repository public to enable
this feature." Repository remained private; no paid subscription or visibility
change was made. Server-side protection is unavailable, not claimed as enabled.
Agent workflow and local/CI gates remain mandatory but owners can technically
bypass them; record checks are not cryptographic attestations of independent work.

## Discovery observed

`tools/discover_port.py --iso <local-user-disc>` completed using pinned tree-sitter
packages. It emitted metadata only. Summary: 348 scoped source files; 4,687 parsed
definitions; 7,488 declarator nodes; 10,383 unique guest addresses from 11,216 map
rows; 667 macro call entries; 23 VU uploads; 341 disc files and 28 directories.
Source SHA-256 values normalize CRLF to LF for cross-platform reproducibility.

Parser gap nodes: 667. Most are macro-related; they are retained with exact
locations, not marked as semantic success. Asset archive internals, semantic
call graph, target behavior and raw-sector consumption remain explicitly unknown
and assigned tasks. The inspected local image's size differs from upstream's
full-disc documentation; local metadata is not substituted with upstream LBAs.

Read actual startup sequence, platform adapter, RHI Device contract, recompiler
configuration/function maps and disc reader. Their ownership and required work
are mapped in PORT_DISCOVERY and PORTING_CHECKLIST. REA skill was read; ordinary
source analysis is its source-first route, so no unnecessary binary/provider
session was launched. Escalation to targeted REA is documented for later blockers.

## Local checks observed

- `python tools/validate_project.py`: PASS, 142 unique tasks in 12 milestones,
  metadata counts, all source-path/hash matches, pinned submodule and tracked
  artifact policy. Re-run after staging and final commit.
- `python -m unittest discover -s tools -p 'test_*.py' -v`: thirteen tests passed,
  covering incomplete tasks, missing/outside evidence, duplicate/foreign IDs,
  eligible complete evidence, stale source approval, review-only post-candidate
  changes and nonancestor candidate rejection. Added after independent review:
  committed-file evidence enforcement, ignored/untracked/Git-internal/symlink
  rejection, evidence mutation and malformed evidence/review schema cases.
- Initial scaffold's existing host CMake guard proof is in docs/VALIDATION.md;
  no compiler/target success was inferred from that deliberate rejection.
- Generated GLAD and root C/CMake scaffold were preserved. Upstream git status
  remained empty. Git-tracked data excludes ISO, ELF, assets, saves, generated game
  code and host build directories. No generated game bytes were created.

## Remote CI and final candidate

Foundation workflow checks source inventory/paths/hash and governance tests on
Ubuntu/Python 3.12 with the public pinned submodule. It requires no private disc.
The initial remote `foundation` job passed for commit
`eacb73af5e9cadc7dd030a33c632a1923ec9112e` in
[run 42505381952](https://github.com/baezahuertaelias/gtavcs_nx/actions/runs/42505381952).
The final task-completion candidate must also pass foundation CI and the clean
eligibility gate before reviewer creation; its exact SHA/run is recorded in the
independent review record. No reviewer is created on the strength of a stale run.
`milestone-review` is a PR-only gate and must remain blocked until actual review.

## Known environment limits

Docker Linux engine was unavailable and WSL inventory only showed docker-desktop
in earlier checks. No local devkitPro target toolchain was found at standard
C:/devkitPro paths. M01 explicitly requires restoration and target/hardware
verification. M00 review must assess discovery/planning/gates, not certify a game
binary it cannot run. No reviewer has been created during incomplete M00 tasks.
