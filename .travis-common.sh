export LOCAL_PREFIX="$(dirname "$(readlink -f "$0")")/local-prefix"
mkdir -p "$LOCAL_PREFIX"
export PATH="$LOCAL_PREFIX/bin${PATH:+:}$PATH"

# Make sure that we properly define dependencies in make
# and can parallel build.
export MAKEFLAGS=("-j" "2")

# Make sure that we use the right compiler
if [ "$CC" = "gcc" ] || [ "$CXX" = "g++" ]; then
  export CC=gcc
  export CXX=g++
else
  export CC=clang
  export CXX=clang++
fi
