# Wisp Arena

## Host the multiplayer server with Coolify

The repository includes a root `Dockerfile` for the Bun websocket server.
Create a Dockerfile-based service in Coolify from this repo and expose port
`9001`.

Useful environment variables:

- `PORT=9001` changes the port the server listens on.
- `HOST=0.0.0.0` is the container default and should usually stay unchanged.

Point clients at your deployed websocket URL, for example
`wss://YOUR_DOMAIN` when Coolify terminates TLS for the service.

## Windows client installer

The Windows client is built with CMake and vcpkg. The release package is a zip
containing `wisp-arena.exe`, required DLLs, and `server-url.txt`. You can also
build a Windows installer with Inno Setup. The installer contains the already
built game; players do not need Visual Studio, CMake, or vcpkg.

### Build locally on Windows

1. Install Visual Studio Build Tools with the C++ workload.
2. Install CMake, Git, vcpkg, and Inno Setup 6.
3. Set `VCPKG_ROOT` to your vcpkg checkout.
4. Build and package:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
.\scripts\package-windows.ps1 -ServerUrl "ws://YOUR_SERVER:9001" -Installer
```

The portable zip is written to `dist\wisp-arena-windows.zip`, and the installer
is written to `dist\wisp-arena-setup.exe`.

### Build with GitHub Actions

Push the repo to GitHub and run the `Windows Client` workflow. Use the
`server_url` input when starting the workflow manually to bake your server
address into `server-url.txt`.

Friends can download the artifact, run `wisp-arena-setup.exe`, and launch the
game from the installer, Start Menu, or desktop shortcut. If the server changes,
they can edit `server-url.txt` in the install folder.

## Run the multiplayer test setup

1. Install the development packages for `raylib` and `ixwebsocket`.
2. Make sure `mise` is available.
3. Run:

```bash
make run
```

That starts the server and opens two client windows by default.

## Useful overrides

- `CLIENT_COUNT=3 make run` starts three clients.
- `SERVER_URL=ws://localhost:9001 make run` overrides the client target URL.
