export LOCAL_PREFIX="$(dirname "$(readlink -f "$0")")/../local-prefix"
mkdir -p "$LOCAL_PREFIX"
export PATH="$LOCAL_PREFIX/bin:$HOME/.local/bin:${PATH:+:}$PATH"
python_lib="$(python -c "from distutils.sysconfig import get_python_lib; print(get_python_lib())")"
export PYTHONPATH="$HOME/.local/lib/$(basename "$(dirname "$python_lib")")"

# Make sure that we properly define dependencies in make
# and can parallel build.
export MAKEFLAGS=("-j" "2")

# Make sure that we use the right compiler
if [ "${MYCC:0:3}" = "gcc" ]; then
  export CC=gcc${MYCC:3}
  export CXX=g++${MYCC:3}
elif [ "${MYCC:0:5}" = "clang" ]; then
  export CC=clang${MYCC:5}
  export CXX=clang++${MYCC:5}
else
  export CC=not-a-compiler
  export CXX=not-a-compiler
fi
