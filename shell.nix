with import <nixpkgs> {};

stdenv.mkDerivation {
    name = "curses";

    buildInputs = [ cmake ];

    shellHook = ''
      ./configure
    '';

}
