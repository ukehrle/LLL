{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-22.11";

    flint.url = "github:ukehrle/flint2/lll-rank-deficient";
    flint.flake = false;
  };

  outputs = { self, nixpkgs, flint }:
    let
      system = "x86_64-linux";
      pkgs = (import nixpkgs { inherit system; }).pkgs;
    in {
      devShell.${system} = pkgs.mkShell {
        nativeBuildInputs = with pkgs;
          [
            gdb
            ((pkgs.flint.overrideAttrs (old: {
              src = flint;
              preConfigure = ''
                configureFlagsArray=("CFLAGS=-O3 -mtune=native -march=native -funroll-loops -mpopcnt")
              '';
            #   preConfigure = ''
            #     configureFlagsArray=("CFLAGS=-g -Og" "CXXFLAGS=-g -Og")
            #   '';
            #   dontStrip = true;
            # })).override { stdenv = pkgs.clangStdenv; })
            })))
          ];
      };
    };
}
