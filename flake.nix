{
  description = "Rivet — a native Linux launcher for online-fixed games";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];

      forAllSystems = f:
        nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});

      runtimeTools = pkgs: with pkgs; [
        aria2
        ffmpeg
        gnutar
        icoextract
        p7zip
        procps
        winetricks
        xdg-user-dirs
        xdg-utils
      ];
    in
    {
      packages = forAllSystems (pkgs:
        let
          rivet = pkgs.stdenv.mkDerivation {
            pname = "rivet";
            version = "1.0.0";
            src = self;

            nativeBuildInputs = with pkgs; [
              cmake
              pkg-config
              qt6.wrapQtAppsHook
            ];

            buildInputs = with pkgs.qt6; [
              qtbase
              qtsvg
            ];

            cmakeFlags = [ "-DRIVET_BUILD_TESTS=OFF" ];

            # Keep the runtime helpers on PATH for the launcher's subprocesses.
            # Steam is intentionally not included: use the native Steam package
            # from Nixpkgs or your system configuration.
            qtWrapperArgs = [
              "--prefix PATH : ${pkgs.lib.makeBinPath (runtimeTools pkgs)}"
            ];

            meta = with pkgs.lib; {
              description = "Native Linux launcher for OnlineFix-style game fixes";
              homepage = "https://github.com/ne1i/rivet-launcher";
              license = licenses.agpl3Only;
              platforms = [ "x86_64-linux" "aarch64-linux" ];
              mainProgram = "rivet";
            };
          };
        in
        {
          inherit rivet;
          default = rivet;
        });

      apps = nixpkgs.lib.genAttrs systems (system: {
        default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/rivet";
        };
      });

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          packages = with pkgs; [
            cmake
            pkg-config
            qt6.qtbase
            qt6.qtsvg
            gdb
            nixfmt-rfc-style
          ];
        };
      });

      formatter = forAllSystems (pkgs: pkgs.nixfmt-rfc-style);
    };
}
