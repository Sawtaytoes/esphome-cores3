# Agent instructions

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
