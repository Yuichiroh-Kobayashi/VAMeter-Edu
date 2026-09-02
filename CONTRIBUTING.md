# Contributing to VAMeter-Edu

日本語: [CONTRIBUTING_ja.md](CONTRIBUTING_ja.md)

## Welcome

Bug reports, documentation improvements, and focused pull requests are welcome. VAMeter-Edu is educational firmware for real classroom use, so clarity, safe failure, data preservation, and explainable measurement behavior are important throughout the project.

## Before opening an Issue

- Search existing Issues first. Add useful context to an existing Issue when it describes the same durable problem or outcome.
- Prefer one Issue for one durable problem or outcome, rather than a new Issue for each debugging attempt.
- Check which repository owns the behavior before filing, when practical.

## Where should I report it?

- [VAMeter-Edu](https://github.com/Yuichiroh-Kobayashi/VAMeter-Edu): firmware, device UI, measurement behavior, VAMeter hardware integration, AssetPool integration, and deployment.
- [Device-to-Browser-Viewer](https://github.com/Yuichiroh-Kobayashi/Device-to-Browser-Viewer): browser UI, graph rendering, browser lifecycle, and browser compatibility.
- [Device-to-Browser-Data-Streaming](https://github.com/Yuichiroh-Kobayashi/Device-to-Browser-Data-Streaming): the generic D2B protocol, schemas, reusable validators and test vectors, and conformance behavior.

If you are unsure which repository owns a problem, open the Issue where you encountered it and describe the context. Maintainers can redirect or cross-link it.

## Writing a useful Issue

Please include, where relevant:

- the expected behavior and the behavior you observed;
- practical reproduction steps;
- the device model and connection setup;
- the firmware version or commit;
- the browser, browser version, and operating system;
- logs, screenshots, or a minimal data sample with private information removed;
- whether the behavior is consistent or intermittent.

For measurement reports, describe the circuit, measurement mode, and units without exceeding the hardware's documented operating limits. Do not include credentials, private network details, or personal information.

## Pull requests

Keep pull requests focused and reviewable. Explain what changed and why, link an Issue when applicable, and state:

- the tests and checks performed;
- any physical hardware or browser checks performed;
- checks that were not performed;
- known limitations or follow-up work.

Avoid mixing firmware behavior, protocol changes, UI redesign, dependency updates, and unrelated cleanup in one pull request. Preserve existing behavior unless the proposed change explicitly updates the relevant documented behavior or interface.

## Testing and physical validation

Use the smallest test set that covers the change. Host tests and desktop simulation are valuable, but they do not establish behavior that depends on actual VAMeter hardware, storage, USB, Wi-Fi, electrical measurement, or a real browser/device combination.

Do not describe a change as physically validated unless it was tested on the stated hardware and environment. Maintainers may perform additional hardware, browser, resource, and release qualification before merging or publishing a release.

Serial observation that crosses WSL and Windows has additional device-identity, observer-lifecycle, and evidence-handling considerations. See the [WSL/Windows serial observation guide](docs/development/wsl-windows-serial-observation.md).

## Documentation

Documentation changes are welcome. Keep current product behavior separate from future proposals and historical evidence. Prefer public engineering language, define specialized terms, avoid private machine paths or operator data, and keep English and Japanese entry points aligned when both describe the same behavior.

## Cross-repository changes

Some features require coordinated changes across the firmware, Viewer, and generic D2B protocol repositories. Open or link an Issue in each affected repository, identify which repository owns each part, and describe compatibility or rollout ordering. A merged change in one repository does not by itself establish end-to-end compatibility or release readiness.

## License

VAMeter-Edu is distributed under the [MIT License](LICENSE). By submitting a contribution, you agree that it may be distributed under the project's license.
