# Working on the project

Repository: [jokenedward/esp32_electronic_load](https://github.com/jokenedward/esp32_electronic_load)

## Setup

1. Accept the repository invitation, if required.
2. Clone the repository through VS Code's **Git: Clone** command.
3. Open the `esp32_electronic_load` folder.
4. Set up ESP-IDF 6.0.2 and build with `idf.py build`. The target is `esp32`.
5. Run `sh tests/run_host_tests.sh` on Mac/Linux with a C++ compiler.
   Windows users can use a suitable WSL/Linux environment for the host tests.

Each person should use their own GitHub account and Git author details.
GitHub provides a no-reply email address in its email settings if needed.

Local ESP-IDF paths and serial-port settings belong in the ignored
`.vscode/settings.json`. Build output is also ignored. Build locally to
generate `build/compile_commands.json`; do not copy another person's build folder.

## Making changes

Agree on the area being worked on before editing, especially shared files such
as `project_config.hpp`.

1. Pull the latest `main`.
2. Create a branch for the change, for example `gui-menu-fix`.
3. Make the change and run the relevant tests.
4. Review the changed files, stage them and commit with a short description.
5. Push the branch and open a pull request.
6. Get a teammate to review it before merging into `main`.

Keep each pull request focused on one change. Include what was tested and any
hardware assumptions. This is the team's recommended workflow; branch protection
has not been configured.

If Git reports a conflict, compare both changes before resolving it. Avoid
force-pushing or replacing the project with an older ZIP.

## Before merging

- Check that `idf.py build` succeeds.
- Run the host tests when available.
- Keep Simulation as the shared default, with hardware flags false.
- Check pin, resistor and calibration changes with the hardware team.
- Keep passwords, tokens, build output and personal settings out of commits.

The safety checks in [board bring-up](docs/board_bringup.md) still apply.
A passing build is not permission to energize the power stage.

## Repository access

The owner can invite teammates through **Settings → Collaborators → Add people**.
A private repository link does not grant access on its own; invitations need to
be accepted.

Reference: [VS Code Git guide](https://code.visualstudio.com/docs/sourcecontrol/repos-remotes)
and [GitHub collaborator guide](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/repository-access-and-collaboration/inviting-collaborators-to-a-personal-repository).
