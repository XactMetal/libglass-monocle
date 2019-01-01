set -e
make
pushd java
./inplaceModifyJfxrt.sh
cp /home/xact/8u-dev-rt/modules/graphics/src/main/native-glass/monocle/libglass_monocle.so /usr/lib/jvm/java-8-oracle/jre/lib/arm/libglass_monocle.so
cp /tmp/jfxrt.jar /usr/lib/jvm/java-8-oracle/jre/lib/ext/jfxrt.jar
echo Done all
