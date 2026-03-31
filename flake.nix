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
    in
      pkgs.mkShell {
        buildInputs = with pkgs; [
          cef-binary
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
          at-spi2-core # atk + atspi
          cups
          libxkbcommon
          mesa # gbm
          libglvnd # libGL.so for GPU process
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

        # Tell the linker where to find libcef.so at build time
        CEF_ROOT = "${pkgs.cef-binary}";

        shellHook = ''
          echo "CEF devshell ready. CEF_ROOT=$CEF_ROOT"
        '';
      };
  };
}
