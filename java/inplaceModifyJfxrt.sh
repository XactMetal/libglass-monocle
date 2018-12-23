set -e
echo "Store a copy of jfxrt.jar under /tmp for inplace modification"
rm -R bin || true
mkdir bin

unzip /tmp/jfxrt.jar -d bin/
javac -cp bin/ com/sun/glass/ui/monocle/EGL.java
jar -uf /tmp/jfxrt.jar com/sun/glass/ui/monocle/EGL.class
cp com/sun/glass/ui/monocle/EGL.class bin/com/sun/glass/ui/monocle/EGL.class
javac -cp bin/ com/sun/glass/ui/monocle/AcceleratedScreen.java
jar -uf /tmp/jfxrt.jar com/sun/glass/ui/monocle/AcceleratedScreen.class

echo "Done"
rm -R bin/
echo "Cleaned up"
