{
  description = "CEF terminal browser — build environment";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };

  outputs = {
    self,
    nixpkgs,
  } @ inputs: let
    system = "x86_64-linux";
    pkgs = import nixpkgs {
      inherit system;
      config.allowUnfree = true;
    };

    # Chromium/CEF build dependencies (from install-build-deps.py).
    chromiumDeps = with pkgs; [
      # ── Core build tooling ──
      cmake
      ninja
      pkg-config
      autoconf
      binutils
      bison
      flex
      gperf
      patch
      perl
      python3
      ruby
      git

      # ── Compression / archiving ──
      bzip2
      p7zip
      xz
      zip
      zstd
      zlib

      # ── System / IPC libs ──
      dbus
      elfutils
      fakeroot
      libcap
      libffi
      linux-pam
      lksctp-tools
      openssl
      sqlite
      systemd
      util-linux

      # ── Graphics / display ──
      cairo
      fontconfig
      freetype
      libdrm
      libglvnd
      libGLU
      libpng
      libjpeg
      libva
      mesa
      pango
      pixman
      vulkan-loader
      wayland
      gtk3

      # ── X11 / Xorg ──
      libx11
      libxcb
      libxcomposite
      libxdamage
      libxext
      libxfixes
      libxrandr
      libxkbcommon
      libxshmfence
      libxscrnsaver
      libxt
      libxtst
      libxau
      libxcursor
      libxdmcp
      libxi
      libxinerama
      libxrender
      xcb-util-cursor
      xorg-server

      # ── Input / accessibility / misc device ──
      at-spi2-core
      bluez
      brltty
      libevdev
      libinput
      ncurses
      speechd

      # ── Audio / network / crypto ──
      alsa-lib
      cups
      curl
      glib
      krb5
      nss
      nspr
      pciutils
      pulseaudio

      # ── Misc CLI tools (used by chromium build scripts) ──
      fd
      file
      fuse
      libxslt
      lsb-release
      procps
      ripgrep
      wdiff
      expat
    ];

    # Our project's own dependencies (on top of Chromium's).
    projectDeps = with pkgs; [
      lua5_4
      gtest

    ];
  in {
    devShells.${system}.default =
      # buildFHSEnv creates a bubblewrap sandbox with standard FHS layout
      # (/lib64/ld-linux-x86-64.so.2, /usr/lib, etc). Required on NixOS
      # because depot_tools downloads prebuilt binaries (cipd, gn, clang,
      # reclient) that are dynamically linked against FHS paths.
      #
      # On non-NixOS distros this is unnecessary — deps come from the
      # system package manager and sync.sh / build.sh run directly.
      (pkgs.buildFHSEnv {
        name = "cef-env";

        targetPkgs = pkgs: chromiumDeps ++ projectDeps;

        profile = ''
          export PATH="$PWD/depot_tools:$PATH"
          export CEF_BUILD_ROOT="$PWD"
        '';

        runScript = "bash";
      })
      .env;
  };
}
