# Agent instructions

## Prefer MCP, APIs, and CLIs over computer use

For service access and routine reads or writes, prefer an available MCP tool, a
supported HTTP API, or the service's CLI over browser or desktop interaction.
Check the project's runbooks, existing tools, and documented endpoints first.
The presence of browser tools is not a reason to choose computer use. A missing
MCP tool or browser login does not prove that an API or CLI is unavailable.

Use computer interaction when the task needs a visual check, a browser test, a
UI-only operation, or authentication that the available programmatic routes
cannot perform. Keep that interaction narrow, then return to MCP/API/CLI calls
for the rest. If no documented route exists, check the app's existing HTTP
requests before assuming repeated clicks are necessary; do not bypass access
controls. State the specific limitation when computer use is required.

This preference does not grant new access, widen credentials or permissions,
replace a more specific service route, or remove required visual verification.

This repository contains generic, publishable ESPHome hardware support. Never add
household names, network addresses, secrets, or private device identifiers.
Use original code derived from hardware datasheets and mainline ESPHome APIs.
Do not adopt M5Stack or other Chinese-origin software dependencies.

Every PR requires independent adversarial review by another agent that did not
author the changes. Review the final head for correctness, regressions, failure
cases, security, and test coverage. Record base/head SHAs, reviewer, findings,
resolutions, and checks on the PR. Resolve material findings before merge and
repeat review after substantive fixes. Passing CI and self-review do not replace
this review. If no reviewer is available, report the gate instead of merging.

Verify with an ESPHome 2026.10.0 dashboard build and actual board readback. Keep
one production device configuration per physical board in the deployment.
Commit, push, open a PR, satisfy review/check gates, and merge before completion.
