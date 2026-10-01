# Sharing and working together

This repository contains the ESP32 electronic-load firmware and its software
documentation. It does not contain the presentations, thermal models, generated
build files or personal ESP-IDF installation paths.

## First: publish the repository

The local repository is prepared. It is not online until someone publishes it.
Recommended name: `elec5552-electronic-load`. Keep it private for the project team.

The simplest route for the owner is VS Code:

1. Open the `esp32_electronic_load` folder itself, not its parent folder.
2. Open the Command Palette and choose **Publish to GitHub**.
3. Sign in using your own GitHub account.
4. Choose the repository name and **Private**.
5. Review the files and finish publishing.

If Git asks for your author name/email, use your own identity. For email privacy,
copy your exact GitHub-provided no-reply address from GitHub's email settings;
do not guess it or share a password/token in a chat.

Do not create a second repository if one has already been published.
[Official VS Code publishing instructions](https://code.visualstudio.com/docs/sourcecontrol/repos-remotes#_publish-to-github).

## Invite teammates

On the published GitHub repository, open **Settings → Collaborators → Add people**.
Invite your group members using their GitHub usernames. They must accept their
invitations before contributing. Sharing a private repository's link alone does
not give access. Each person should use their own account.
[Official GitHub collaborator instructions](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/repository-access-and-collaboration/inviting-collaborators-to-a-personal-repository).

## Each teammate's first setup

1. Accept the invitation.
2. Copy the repository's HTTPS URL from GitHub's **Code** menu.
3. In VS Code, use **Git: Clone**, paste that URL and open the cloned folder.
4. Install/configure ESP-IDF **6.0.2** using the ESP-IDF extension.
5. In an ESP-IDF terminal, run `idf.py build`. Target: `esp32`, not `esp32s3`.
6. On Mac/Linux with a C++ compiler, also run `sh tests/run_host_tests.sh`.
   Windows users can run these host tests in a suitable WSL/Linux environment.

Each teammate configures their own ESP-IDF installation and serial port.
The machine-specific `.vscode/settings.json` is deliberately excluded.
Do not upload the `build/` directory to fix missing headers; build locally so
ESP-IDF creates your own `build/compile_commands.json`.

## Simple everyday workflow

Agree who is working on menus, sensors or another area before starting.

1. Pull the latest `main`.
2. Create a branch for one change, such as `gui-menu-fix`.
3. Edit and test the code.
4. Commit only the intended files with a short explanation.
5. Push/publish your branch.
6. On GitHub, open a pull request into `main`.
7. Ask a teammate to review it, then merge. Everyone pulls the updated `main`.

A branch is your separate version of the code. A commit is a saved checkpoint.
A pull request lets the team review a change before it joins the shared version.
This is a recommended team workflow; branch protection has not been configured.

Use Git/VS Code to resolve conflicts carefully. Do not force-push over teammates'
work or replace the whole project with an old downloaded ZIP.

## Before sharing a change

- Run `idf.py build` and the host tests where available.
- Describe what changed and what was actually tested.
- Keep the shared default in Simulation with hardware flags false.
- Review pin assignments, resistor values and calibration changes with the
  hardware team; passing software tests does not verify a PCB.
- Do not connect or enable the load-power stage just because a build passes.
- Never commit passwords, tokens, keys or personal credentials.

Use [README.md](README.md) for the file map and
[board_bringup.md](docs/board_bringup.md) for the outstanding hardware checks.
