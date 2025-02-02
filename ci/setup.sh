#!/bin/bash
# shellcheck disable=2086,2103,2164,2317

cd "$(dirname "$0")"

_ver=$(head -1 debian/changelog | grep -Eo '[0-9]+(\.[0-9]+){2,}-[0-9]+')
pkgver=$(echo "${_ver}" | awk -F - '{print $1}')

# avoid command failure
exit_check() { [ "$1" = 0 ] || exit "$1"; }
trap 'exit_check $?' EXIT

# Update packages
apt update && apt upgrade -y

# Install debhelper
yes | apt install -y debhelper cryptsetup pkg-kde-tools pkexec os-prober rsync git wget || :

# Clone original
wget https://github.com/calamares/calamares/archive/refs/tags/v${pkgver}.tar.gz -O - | tar -xzf -
cp -rn calamares-${pkgver}/* ..
cp -rf debian ..

rm -rf debian calamares-${pkgver}

# Install dependencies
yes | apt install libkf6config-dev libkf6config-dev-bin libkf6configwidgets-dev libkf6coreaddons-dev libkf6crash-dev libkf6i18n-dev libkf6iconthemes-dev libkf6kio-dev libkf6notifyconfig-dev libkf6package-dev libkf6parts-dev libkf6service-dev libkf6solid-dev libpolkit-qt6-1-dev qt6-base-private-dev qt6-declarative-dev qt6-declarative-dev-tools qt6-declarative-private-dev qt6-quick3d-dev-tools qt6-quick3dphysics-dev-tools qt6-shadertools-dev qt6-svg-dev qt6-svg-private-dev qt6-tools-dev qt6-tools-dev-tools qt6-tools-private-dev qt6-wayland-dev-tools qt6-webengine-dev qt6-webengine-dev-tools qt6-webengine-private-dev cmake extra-cmake-modules libkpmcore-dev libparted-dev libpwquality-dev libboost-python-dev libyaml-cpp-dev qml6-module-qtquick-layouts qml6-module-qtquick-window qml6-module-qtquick

cd ..
