set -e
make
pushd java
./inplaceModifyJfxrt.sh
popd
sudo cp libglass_monocle.so /usr/lib/jvm/java-8-oracle/jre/lib/arm/libglass_monocle.so
sudo cp /tmp/jfxrt.jar /usr/lib/jvm/java-8-oracle/jre/lib/ext/jfxrt.jar
echo Done all
