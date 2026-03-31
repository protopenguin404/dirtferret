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
          rsync
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
          set -euo pipefail
          PROJ="$PWD"
          WS="$PROJ/workspace"
          CEF_STORE="${cef}"

          # Assemble workspace from immutable nix store CEF + mutable project sources.
          _assemble_workspace() {
            echo "[workspace] Assembling from $CEF_STORE ..."

            # Stage in a tmpdir to make the swap atomic-ish.
            TMPWS=$(mktemp -d /tmp/.cef-ws-XXXXXX)
            trap 'rm -rf "$TMPWS"' ERR

            # 1. Copy CEF distribution (immutable base) — writable copies.
            rsync -a "$CEF_STORE/" "$TMPWS/"
            chmod -R u+w "$TMPWS"

            # 2. Overlay our source tree.
            rsync -a "$PROJ/src/" "$TMPWS/src/"

            # 3. Overlay our top-level CMakeLists.txt (has our app wired in).
            cp "$PROJ/CMakeLists.txt" "$TMPWS/CMakeLists.txt"

            # 4. Preserve build cache if it exists.
            if [ -d "$WS/build" ]; then
              rsync -a "$WS/build/" "$TMPWS/build/"
            fi

            # 5. Swap into place.
            rm -rf "$WS"
            mv "$TMPWS" "$WS"

            echo "[workspace] Ready at $WS"
          }

          _assemble_workspace

          # Convenience: cd into workspace on shell open.
          echo "[workspace] To build:"
          echo "  cd workspace/build && cmake -G Ninja -DCMAKE_BUILD_TYPE=Release .. && ninja cef-terminal"
          echo ""
          echo "  Or just run: build-cef"

          # Helper function to configure + build in one shot.
          build-cef() {
            mkdir -p "$WS/build"
            cd "$WS/build"
            cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
            ninja cef-terminal
            echo "[build] Binary at: $WS/build/src/Release/cef-terminal"
          }

          # Helper to sync source changes into the workspace without full rebuild.
          sync-src() {
            rsync -a "$PROJ/src/" "$WS/src/"
            echo "[sync] Source synced to workspace."
          }

          export -f build-cef sync-src
        '';
      };
  };
}
