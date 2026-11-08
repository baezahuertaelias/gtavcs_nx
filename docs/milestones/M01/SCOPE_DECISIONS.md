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

The user subsequently instructed: **"Defer the Pro Controller test until the
game loads the main menu too, then finish M01 with the tests I already
provided."** This extends the explicit scope change: M01 accepts the supplied
Joy-Con functional tests as working, and Pro Controller acceptance is retained
as unchecked M06-14 at the same genuine-game/main-menu trigger.

The user's earlier instruction was to mark the supplied Joy-Con test working,
stop further testing for now, and resolve any remaining problems later. The raw
log's SDL connected=1 observation remains disclosed, and M06-14 explicitly owns
formal physical connection-state verification and resolution. It is not claimed
fixed or independently proved in M01. This preserves the deferred work instead
of erasing it from the plan or recording a false hardware pass.

M01-08 records accepted Joy-Con functional evidence plus this authorized
deferral. M01-12 now closes the supplied evidence/CI and in-scope failure fixes;
it does not require the explicitly deferred tests before M01 review. The
missing-root/crash/display and suspend-reset failures have actual fix/regression
evidence in VERIFICATION.md. Pro/docking and formal connection-state work remain
required before subsequent gameplay acceptance at M06's main-menu trigger.

The user's instruction to finish M01 authorizes its required source/governance
checks, independent reviewer and merge workflow. No additional hardware tests
will be requested or run. The deferrals themselves do not constitute independent
milestone approval; that exact-candidate review is still required.
