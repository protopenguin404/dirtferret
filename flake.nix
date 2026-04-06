{
  description = "dirtferret — terminal web browser on CEF";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };

  outputs = {
    nixpkgs,
    self,
  }: let
    system = "x86_64-linux";
    pkgs = import nixpkgs {
      inherit system;
    };
    cef = pkgs.callPackage ./nix/cef-binary.nix {};
  in {
    packages.${system} = {
      default = cef;
      cef-binary = cef;
    };

    devShells.${system}.default = pkgs.mkShell {
      nativeBuildInputs = with pkgs; [
        # Build tools
        cmake
        ninja
        gcc
        pkg-config

        # Cap'n Proto compiler + libraries
        capnproto

        # Rust toolchain
        rustc
        cargo
        rustfmt
        clippy

        # Testing
        gtest
      ];

      buildInputs = with pkgs; [
        # CEF runtime dependencies
        libx11
        libxcomposite
        libxdamage
        libxext
        libxfixes
        libxrandr
        libxcb
        libxshmfence
        nss
        nspr
        at-spi2-core
        cups
        libxkbcommon
        mesa
        libglvnd
        glib
        dbus
        expat
        cairo
        pango
        alsa-lib
        gtk3
        libdrm
        libgbm
        udev
        systemdLibs

        # LuaJIT
        luajit

        # Cap'n Proto runtime
        capnproto
      ];

      shellHook = ''
        export CEF_ROOT="${cef}"

        build-core() {
          mkdir -p build && cd build
          cmake -G Ninja \
            -DCMAKE_BUILD_TYPE=Release \
            -DCEF_ROOT="$CEF_ROOT" \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
            ..
          ninja "$@"
          cd ..
        }

        build-tui() {
          cd tui && cargo build "$@" && cd ..
        }

        build-all() {
          build-core && build-tui
        }

        run-tests() {
          build-core test-schema test-shm test-input
          cd build && ctest --output-on-failure && cd ..
          cd tui && cargo test && cd ..
        }

        echo "dirtferret dev environment"
        echo "  CEF_ROOT=$CEF_ROOT"
        echo ""
        echo "Commands:"
        echo "  build-core [target]  — build C++ core (default: all)"
        echo "  build-tui [flags]    — build Rust TUI"
        echo "  build-all            — build everything"
        echo "  run-tests            — run all tests"
      '';
    };
  };
}
