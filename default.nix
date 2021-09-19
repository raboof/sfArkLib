{ pkgs ? import (
    builtins.fetchTarball {
      # nixpkgs pin @ NixOS 21.05
      url = "https://github.com/NixOS/nixpkgs/archive/d4590d21006387dcb190c516724cb1e41c0f8fdf.tar.gz";
      sha256 = "17q39hlx1x87xf2rdygyimj8whdbx33nzszf4rxkc6b85wz0l38n";
    }
  ) {}
}:

pkgs.stdenv.mkDerivation {
  name = "sfArkLib";
  src = builtins.filterSource (
    path: type: baseNameOf path != ".git" && type != "symlink"
  ) ./.;

  buildInputs = [ pkgs.zlib ];

  installFlags = [ "PREFIX=$(out)" ];

  meta = {
    description = "Library for decompressing sfArk soundfonts";
    platforms = pkgs.lib.platforms.linux;
  };
}
