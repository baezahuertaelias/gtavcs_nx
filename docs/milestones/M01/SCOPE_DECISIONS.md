# User-approved M01 scope decision — 2026-11-08

The user requested that M01-10 be marked done, then clarified the intended action
as **"Defer the requirement"**, rather than confirming a hardware pass. The user
specified the trigger: **"M01-10 will be tested when game opens and it can load
the main menu of the game"**.

Accordingly, the original docking acceptance is retained as unchecked M06-13:
at first genuine game boot that loads the main menu, test repeated dock/undock,
drawable pixels/viewport, 1280x720 handheld and 1920x1080 docked output, and one
SDL-owned GL context/surface. Perform it when that trigger is reached, before
subsequent gameplay acceptance; do not wait for all other M06 tasks to finish.
The original validation requirements have not been replaced with an emulator,
scaffold, source inspection or invented hardware evidence.

M01-10 now records completion of this approved scheduling/scope change, not
completion of the physical test. M01's exit and evidence expectations are revised
to match. The task ledger's checked deferral record must not be described as
"docking verified". This follows the ledger rule permitting scope changes only
through the user's explicit decision and a revised plan.

The Pro Controller matrix and complete M01 hardware/failure evidence remain
required M01-08/M01-12 work. The user also requested no further testing for now;
no additional tests were run for this documentation change. No independent
milestone approval or merge is implied by the deferral.
