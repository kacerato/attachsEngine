#!/usr/bin/env bash
# Requires Bash, curl, tar, sha256sum, a complete Perl, GNU make and Android NDK.
# On Git for Windows, MSYS2_ENV_CONV_EXCL preserves Perl's POSIX module paths.
set -euo pipefail
export MSYS2_ENV_CONV_EXCL="${MSYS2_ENV_CONV_EXCL:+$MSYS2_ENV_CONV_EXCL;}PERL5LIB"
: "${ANDROID_NDK_ROOT:?Set ANDROID_NDK_ROOT to the Android NDK directory}"
root="$(cd "$(dirname "$0")/.." && pwd)"
version=3.5.8
sha=a8f84a39918ec6415ce765d9b429d313ba97b8143169c172e734b9514464f5b2
work="$root/build/openssl-android"
mkdir -p "$work"
archive="$work/openssl-$version.tar.gz"
if [[ ! -f "$archive" ]]; then
  curl --fail --location --output "$archive" "https://github.com/openssl/openssl/releases/download/openssl-$version/openssl-$version.tar.gz"
fi
echo "$sha  $archive" | sha256sum --check --status
if [[ ! -f "$work/openssl-$version/Configure" ]]; then tar -xzf "$archive" -C "$work"; fi
case "$(uname -s)" in
  MINGW*|MSYS*) host=windows-x86_64; make_bin="${MAKE:-mingw32-make}" ;;
  Linux*) host=linux-x86_64; make_bin="${MAKE:-make}" ;;
  Darwin*) host=darwin-x86_64; make_bin="${MAKE:-make}" ;;
  *) echo 'Unsupported build host' >&2; exit 1 ;;
esac
export PATH="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/$host/bin:$PATH"
cd "$work/openssl-$version"
# Private, APK-compatible SONAMEs. No patch to OpenSSL source or algorithms.
cat > Configurations/99-astra.conf <<'PERL'
my %targets = (
    "android-astra-arm64" => {
        inherit_from => [ "android-arm64" ],
        shared_extension => ".so.astra.so",
    },
);
PERL
perl Configure android-astra-arm64 -D__ANDROID_API__=26 shared no-tests no-apps no-docs no-module no-engine --prefix=/astra-openssl
"$make_bin" -j"${JOBS:-8}" SHELL=sh "CC=bash '$root/tools/android-clang-response.sh'" build_libs
destination="$root/native/third_party/openssl/arm64-v8a"
mkdir -p "$destination"
for library in libcrypto.so.astra.so libssl.so.astra.so; do
  cp "$library" "$destination/$library"
  llvm-strip --strip-unneeded "$destination/$library"
  llvm-readelf --dynamic "$destination/$library"
done
cp LICENSE.txt "$root/native/third_party/openssl/LICENSE.txt"
cd "$destination"
sha256sum lib*.so > SHA256SUMS
