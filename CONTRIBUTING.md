# Team workflow

Use ESP-IDF 6.0.2 with the `esp32` target. Each person needs their own ESP-IDF
installation. Local paths and serial-port settings belong in the ignored
`.vscode/settings.json`; generated build files stay out of Git.

## Making a change

1. Pull the latest `main` before editing.
2. Create a branch for the section being changed.
3. Keep changes within that section where possible. Agree on changes to
   `project_config.hpp` and `system_types.hpp` with the group.
4. Run the host tests and build the firmware.
5. Push the branch and ask another group member to review it before merging.

```sh
sh tests/run_host_tests.sh
idf.py build
```

Keep Simulation as the shared default. Only change hardware-verification flags
after the checks in [board bring-up](docs/board_bringup.md) have been completed.
Record physical test results separately from computer tests.

Review Git conflicts rather than force-pushing over another person's work.
Do not commit passwords, access tokens, personal settings or build output.
