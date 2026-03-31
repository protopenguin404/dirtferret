{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };
  outputs = {
    self,
    nixpkgs,
  } @ inputs: {
    devShells.x86_64-linux.default = let
      system = "x86_64-linux";
      pkgs = import nixpkgs {
        inherit system;
        config.allowUnfree = true;
      };
      cef = pkgs.cef-binary;
    in
      pkgs.mkShell {
        buildInputs = with pkgs; [
          cmake
          ninja
          pkg-config
          libx11
          libxcomposite
          libxdamage
          libxext
          libxfixes
          libxrandr
          libxcb
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
          claude-code
          claude-mergetool
          claude-code-router
        ];

        CEF_STORE = "${cef}";

        shellHook = ''
          PROJ="$PWD"
          WS="$PROJ/workspace"
          CEF_STORE="${cef}"

          _assemble_workspace() {
            # Remove stale workspace if present (from a crashed session, etc.)
            rm -rf "$WS"
            mkdir -p "$WS"

            # Ensure persistent build dir exists at repo root.
            mkdir -p "$PROJ/build"

            # FIRST: inject our source, build config, and build cache (real project files).
            ln -s "$PROJ/src" "$WS/src"
            ln -s "$PROJ/CMakeLists.txt" "$WS/CMakeLists.txt"
            ln -s "$PROJ/build" "$WS/build"

            # SECOND: overlay CEF library dirs from the nix store (read-only symlinks).
            for item in include libcef_dll cmake Release Resources; do
              ln -s "$CEF_STORE/$item" "$WS/$item"
            done

            echo "[workspace] Ready at $WS (symlinked)"
          }

          _cleanup_workspace() {
            if [ -d "$WS" ]; then
              rm -rf "$WS"
              echo "[workspace] Cleaned up."
            fi
          }

          _assemble_workspace
          trap _cleanup_workspace EXIT

          # Build helper: configure + compile in one shot.
          build-cef() {
            (
              cd "$WS/build"
              cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..
              ninja cef-terminal
            )
            echo "[build] Binary at: $WS/build/src/Release/cef-terminal"
          }

          run() {
            cd $PROJ/workspace/build/src/Release && ./cef-terminal --no-sandbox
          }

          export -f build-cef
          export -f run

        '';
      };
  };
}
