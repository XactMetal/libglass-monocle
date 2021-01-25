This repo contains a modified version of libglass-monocle from OpenJFX 8
to allow running framebuffer graphics on hosts with the etnaviv driver
or proprietary imx8 vivante driver.

Two files must be patched, jfxrt.jar and libglass_monocle.so

### JFXRT

To patch jfxrt.jar, *move* it out of its original directory and into /tmp,
then run

    java/inplaceModifyJfxrt.sh

Following successful completion, jfxrt.jar may be moved back to its original location.



### Toradex Apalis imx8

To build on Toradex bullseye docker container the following dependencies must be installed

    apt install autoconf automake build-essential pkg-config libglib2.0-dev libegl-vivante1-dev libgbm-vivante1-dev libglesv2-vivante1-dev 

Additionally libdrm-dev is also required - it causes a dependency conflict and must be installed manually as follows:

    cd /
    wget http://feeds.toradex.com/debian/pool/main/libd/libdrm/libdrm-dev_2.4.99-1+toradex1_arm64.deb
    ar x libdrm-dev_2.4.99-1+toradex1_arm64.deb
    tar xf data.tar.xz

Then libglass_monocle.so may be built using 'make'


### License and Trademarks

This patch is licensed under the GNU General Public License v2.0 with the Classpath exception, the same license used by Oracle for the OpenJFX project. See the files [LICENSE](LICENSE) and [ADDITIONAL_LICENSE_INFO](ADDITIONAL_LICENSE_INFO) for details.

Java, JavaFX, and OpenJDK are trademarks or registered trademarks of Oracle and/or its affiliates. See the file [TRADEMARK](TRADEMARK) for details.
