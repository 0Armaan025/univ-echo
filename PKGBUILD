pkgname=univ-echo
pkgver=0.1.5
_srcdir="$srcdir/$pkgname-$pkgver"
pkgrel=1
pkgdesc="Terminal-based universal memory and command search"
arch=('x86_64')
url="https://github.com/0Armaan025/univ-echo"
license=('MIT')
depends=('sqlite' 'curl' 'ncurses')
makedepends=('cmake' 'pkgconf' 'nlohmann-json')
source=("$pkgname-$pkgver.tar.gz::https://github.com/0Armaan025/univ-echo/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
  cmake -S "$srcdir/$pkgname-$pkgver" \
    -B "$srcdir/$pkgname-$pkgver/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr

  cmake --build "$srcdir/$pkgname-$pkgver/build" --parallel
}

package() {
  DESTDIR="$pkgdir" \
    cmake --install "$srcdir/$pkgname-$pkgver/build"
}
