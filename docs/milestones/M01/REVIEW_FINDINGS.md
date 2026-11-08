# Independent review findings — M01

Reviewer: `/root/m01_independent_review` (fresh independent reviewer; read-only).
Candidate: `fd900967627e0dba1ef72a8ee28293b3f61df2ee`.
Verdict: REQUEST_CHANGES.

One P3 finding: VERIFICATION.md described the saved crash-report baseline as
"130-file" at candidate lines 223 and 250. The retained baseline JSON contains
130 entries: 129 regular report files and one dumps directory. The no-new-report
comparison is valid, but the count description was inaccurate.

M01-12 was reopened for the finding. The baseline JSON was independently counted
locally without contacting the Switch; all "130-file" descriptions were corrected
to "130-entry (129 report files plus the dumps directory)". Runtime/build code,
hardware logs and their hashes are unchanged. M01-12 can close after this evidence
correction; the new candidate still requires eligibility, CI and independent
re-review. This record is not an approval.

The reviewer independently passed foundation validation, all 13 governance tests,
eligibility, full diff/pin review, fresh-clone/incremental/failure/clean build
proof, exact NRO hardware/CI hash comparison, target ELF/NRO/NACP/path inspection,
audio-backend symbol inspection and retained hardware-log hash checks. Scope
deferrals were represented accurately as unchecked M06-13/M06-14 work. No other
findings were reported.

Limitations: retained logs/user confirmations supply hardware observations;
no new Switch/manual tests were performed. Exact CFW/Sphaira versions remain
unspecified. This is a platform stub, not gameplay or audible-game acceptance.
