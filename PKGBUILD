pkgname=pg_news
pkgver=0.0.1
pkgrel=1
pkgdesc="News"
arch=('x86_64')
license=('GPL')
depends=('postgresql' 'curl' 'libxml2')
options=('!makeflags')
source=()
options=(debug !strip)

package() {
  cd "$startdir"
  make DESTDIR="${pkgdir}" CFLAGS="-O3 -g -Wall $(pkg-config --libs --cflags libcurl) $(xml2-config --cflags) $(xml2-config --libs)" install
}