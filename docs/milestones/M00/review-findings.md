# Independent review history — M00

First candidate: `d8483925be97fe4256e4679d782b82b4ef581adf`.
Independent reviewer: `/root/m00_independent_review`.
Verdict: **REQUEST_CHANGES**. No approval was recorded and no merge occurred.
M00-12 and M00-15 were reopened before fixes; all other M00 scope remained intact.

1. P2 — evidence gate checked only file existence/containment, so `.git/config`,
   ignored or untracked reports could escape reviewed-candidate protection.
   Resolution: evidence must be a regular tracked blob in HEAD, match its Git
   filtered content hash and exist through a link/junction-free relative path.
   Reject Git internals, ignored/untracked files and symlink tree modes. Regression
   tests also mutate previously committed evidence to prove rejection.
2. P2 — truthiness checks accepted object/blank result text, object reviewer IDs,
   null check entries and omitted limitations. Resolution: validate top-level JSON
   objects, nonblank string fields, exact Boolean independent attestation,
   nonempty string check/test-result lists and explicit string-list limitations;
   malformed records produce BLOCKED diagnostics. Focused tests cover these cases.

Thirteen governance tests passed after these fixes. Full source-only validation,
clean committed eligibility and current-candidate CI must pass before re-review.
The independent reviewer must assess the new exact SHA; this resolution report
does not itself constitute approval.
