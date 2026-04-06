{
  lib,
  stdenv,
  fetchurl,
  glib,
  nss,
  nspr,
  atk,
  at-spi2-atk,
  libdrm,
  expat,
  libxkbcommon,
  libgbm,
  gtk3,
  pango,
  cairo,
  alsa-lib,
  dbus,
  at-spi2-core,
  cups,
  libGL,
  udev,
  systemdLibs,
  libxcb,
  libx11,
  libxcomposite,
  libxdamage,
  libxext,
  libxfixes,
  libxrandr,
  libxshmfence,
}: let
  version = "142.0.10";
  gitRevision = "29548e2";
  chromiumVersion = "142.0.7444.135";
  buildType = "Release";

  gl_rpath = lib.makeLibraryPath [stdenv.cc.cc];

  rpath = lib.makeLibraryPath [
    glib
    nss
    nspr
    atk
    at-spi2-atk
    libdrm
    expat
    libxkbcommon
    libgbm
    gtk3
    pango
    cairo
    alsa-lib
    dbus
    at-spi2-core
    cups
    libGL
    udev
    systemdLibs
    libxcb
    libx11
    libxcomposite
    libxdamage
    libxext
    libxfixes
    libxrandr
    libxshmfence
  ];
in
  stdenv.mkDerivation {
    pname = "cef-binary";
    inherit version;

    src = fetchurl {
      url = "https://cef-builds.spotifycdn.com/cef_binary_${version}+g${gitRevision}+chromium-${chromiumVersion}_linux64_minimal.tar.bz2";
      hash = "sha256-pFMHjj4MktjnX3g03sgLqgai4X/lF29Phmduf7a+KfM=";
    };

    dontStrip = true;
    dontPatchELF = true;

    installPhase = ''
      runHook preInstall

      # Optimize CEF build flags
      sed 's/-O0/-O2/' -i cmake/cef_variables.cmake

      # Patch ELF binaries for NixOS
      patchelf --set-rpath "${rpath}" --set-interpreter "${stdenv.cc.bintools.dynamicLinker}" ${buildType}/chrome-sandbox
      patchelf --add-needed libudev.so --set-rpath "${rpath}" ${buildType}/libcef.so
      patchelf --set-rpath "${gl_rpath}" ${buildType}/libEGL.so
      patchelf --add-needed libGL.so.1 --set-rpath "${gl_rpath}" ${buildType}/libGLESv2.so
      patchelf --set-rpath "${gl_rpath}" ${buildType}/libvk_swiftshader.so
      patchelf --set-rpath "${gl_rpath}" ${buildType}/libvulkan.so.1

      # Output the full CEF distribution for building
      mkdir -p $out

      # Build-time files (headers, cmake, wrapper source)
      cp -a include $out/
      cp -a cmake $out/
      cp -a libcef_dll $out/
      cp CMakeLists.txt $out/

      # Runtime files
      cp -a ${buildType} $out/Release
      cp -a Resources $out/

      runHook postInstall
    '';

    passthru = {
      inherit buildType;
    };

    meta = {
      description = "CEF binary distribution for dirtferret";
      homepage = "https://cef-builds.spotifycdn.com/index.html";
      license = lib.licenses.bsd3;
      platforms = ["x86_64-linux"];
    };
  }
