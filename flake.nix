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
          lua5_4
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

          # Hand off to fish. Cleanup runs after fish exits (bash resumes here).
          fish -C "
            set -gx PROJ $PROJ
            set -gx WS $WS
            set -gx CEF_DIR $CEF_STORE

            function build-cef; $PROJ/scripts/build-cef.sh; end
            function run; $PROJ/scripts/run.sh; end
          "

          # Fish exited — clean up workspace and exit bash too.
          _cleanup_workspace
          exit
        '';
      };
  };
}
