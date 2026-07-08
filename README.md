# purple // loader

A minimalist, modern **C++ desktop login UI** with a purple/black theme, built
with [Dear ImGui](https://github.com/ocornut/imgui). It authenticates against
[KeyAuth](https://keyauth.win) and integrates [Supabase](https://supabase.com)
for service-status + login-event auditing.

## Screens

1. **Login** – account (username/password) or license-key auth, optional 2FA code.
2. **Dashboard** – shows service **status**, **license/plan**, **time before
   expiry**, and account details, with a **CMD console** on the left and a
   **Run Launcher** button beneath it.

## How it works

- **KeyAuth** – a clean, cross-platform reimplementation of the KeyAuth `1.3`
  protocol (`src/keyauth.*`). Requests are form-encoded POSTs; every response is
  verified against KeyAuth's Ed25519 public key using the
  `x-signature-ed25519` / `x-signature-timestamp` headers (via libsodium).
  Unlike the official SDK it never calls `exit()` – failures surface in the UI.
- **Supabase** – a thin PostgREST client (`src/supabase.*`) that records login
  events and fetches the latest status message. Both are optional.
- **Demo mode** – when KeyAuth is not configured (or `app.demo_mode` is `true`),
  any credentials are accepted and the dashboard is filled with sample data so
  you can preview the UI immediately.

## Configuration

Copy `config.example.json` to `config.json` (next to the binary or passed with
`--config`) and fill in your values:

```json
{
  "app":      { "title": "PURPLE // LOADER", "demo_mode": false },
  "keyauth":  { "name": "<app name>", "ownerid": "<10-char ownerid>", "version": "1.0" },
  "supabase": { "url": "https://xxxx.supabase.co", "anon_key": "<anon key>" },
  "launcher": { "command": "./my_launcher", "label": "RUN LAUNCHER" }
}
```

> `config.json` is git-ignored because it holds secrets (KeyAuth ownerid,
> Supabase anon key). Only commit `config.example.json`.

### Supabase tables (optional)

```sql
create table app_status (
  id bigint generated always as identity primary key,
  message text not null,
  created_at timestamptz default now()
);

create table login_events (
  id bigint generated always as identity primary key,
  username text,
  ip text,
  success boolean,
  created_at timestamptz default now()
);
```

## Build

Dependencies: a C++17 compiler, CMake ≥ 3.16, and dev packages for
`glfw3`, `libcurl`, `libsodium`, and OpenGL. Dear ImGui and nlohmann/json are
vendored under `external/`.

### Linux

```bash
sudo apt-get install -y cmake pkg-config libglfw3-dev libgl1-mesa-dev \
    libcurl4-openssl-dev libsodium-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/purple_loader
```

### Windows / macOS

Install the same dependencies (e.g. via [vcpkg](https://vcpkg.io):
`vcpkg install glfw3 curl libsodium`) and configure CMake with the vcpkg
toolchain file. The code is cross-platform (HWID, fonts, and the launcher are
handled per-OS).

## Project layout

```
src/
  main.cpp      GLFW + OpenGL3 + ImGui bootstrap, font loading
  app.*         screens (login + dashboard), console, launcher, auth threads
  theme.*       purple/black ImGui style
  config.*      JSON config loading
  keyauth.*     KeyAuth 1.3 client (Ed25519 verified)
  supabase.*    Supabase PostgREST client
  http.*        libcurl GET/POST helper
  hwid.*        cross-platform hardware id
external/       vendored Dear ImGui + nlohmann/json
```
