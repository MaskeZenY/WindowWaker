#!/usr/bin/env python3
import os
import sys
import shutil

def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    bin_dir = os.path.join(repo_root, "build", "native-win")
    out_dir = os.path.join(repo_root, "build", "release")
    pkg_dir = os.path.join(out_dir, "SwitchWaker-Windows-x64")

    exe_path = os.path.join(bin_dir, "switchwaker.exe")
    if not os.path.exists(exe_path):
        print(f"Error: {exe_path} not found. Please build the project first.")
        sys.exit(1)

    os.makedirs(pkg_dir, exist_ok=True)

    required_files = [
        "switchwaker.exe",
        "dxcompiler.dll",
        "dxil.dll",
        "libpng16.dll",
        "nod.dll",
        "SDL3.dll",
        "webgpu_dawn.dll",
        "zlib1.dll",
    ]

    for fname in required_files:
        src = os.path.join(bin_dir, fname)
        if os.path.exists(src):
            shutil.copy2(src, os.path.join(pkg_dir, fname))
            print(f"Copied {fname}")
        else:
            print(f"Warning: {src} not found!")

    # Include default user settings
    user_dir = os.path.join(pkg_dir, "user")
    os.makedirs(user_dir, exist_ok=True)
    default_ini = (
        "COS_TARGET_FPS=120\n"
        "COS_FB_SCALE=3.0\n"
        "COS_ASPECT=16:9\n"
        "COS_FPS_OVERLAY=1\n"
        "COS_FULLSCREEN=0\n"
        "COS_ANISO=1\n"
    )
    with open(os.path.join(user_dir, "settings.ini"), "w", encoding="utf-8") as f:
        f.write(default_ini)

    # Include quick launcher
    run_cmd = (
        "@echo off\n"
        "start \"\" \"%~dp0switchwaker.exe\"\n"
    )
    with open(os.path.join(pkg_dir, "run.cmd"), "w", encoding="utf-8") as f:
        f.write(run_cmd)

    # Create zip archive
    zip_base = os.path.join(out_dir, "SwitchWaker-Windows-x64")
    zip_path = shutil.make_archive(zip_base, "zip", pkg_dir)
    zip_size_mb = os.path.getsize(zip_path) / (1024 * 1024)
    print(f"\nSuccessfully created release package: {zip_path} ({zip_size_mb:.2f} MB)")

if __name__ == "__main__":
    main()
