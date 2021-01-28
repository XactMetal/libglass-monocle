set -e
echo "Store a copy of jfxrt.jar under /tmp for inplace modification"
rm -R bin || true
mkdir bin

unzip /tmp/jfxrt.jar -d bin/
javac -cp bin/ com/sun/javafx/binding/BindingHelperObserver.java
javac -cp bin/ com/sun/javafx/binding/ExpressionHelperBase.java
javac -cp bin/ javafx/beans/property/BooleanPropertyBase.java
javac -cp bin/ javafx/beans/property/DoublePropertyBase.java
javac -cp bin/ javafx/beans/property/FloatPropertyBase.java
javac -cp bin/ javafx/beans/property/IntegerPropertyBase.java
javac -cp bin/ javafx/beans/property/ListPropertyBase.java
javac -cp bin/ javafx/beans/property/LongPropertyBase.java
javac -cp bin/ javafx/beans/property/MapPropertyBase.java
javac -cp bin/ javafx/beans/property/ObjectPropertyBase.java
javac -cp bin/ javafx/beans/property/SetPropertyBase.java
javac -cp bin/ javafx/beans/property/StringPropertyBase.java

jar -uf /tmp/jfxrt.jar com/sun/javafx/binding/*.class
jar -uf /tmp/jfxrt.jar javafx/beans/property/*.class

javac -cp bin/ com/sun/glass/ui/monocle/MX6Cursor.java
jar -uf /tmp/jfxrt.jar com/sun/glass/ui/monocle/MX6Cursor.class
javac -cp bin/ com/sun/glass/ui/monocle/EGL.java
jar -uf /tmp/jfxrt.jar com/sun/glass/ui/monocle/EGL.class
cp com/sun/glass/ui/monocle/EGL.class bin/com/sun/glass/ui/monocle/EGL.class
javac -cp bin/ com/sun/glass/ui/monocle/AcceleratedScreen.java
jar -uf /tmp/jfxrt.jar com/sun/glass/ui/monocle/AcceleratedScreen.class
javac -cp bin/ com/sun/glass/ui/monocle/LinuxStatefulMultiTouchProcessor.java
jar -uf /tmp/jfxrt.jar com/sun/glass/ui/monocle/LinuxStatefulMultiTouchProcessor.class
javac -cp bin/ com/sun/glass/ui/monocle/NativePlatformFactory.java
jar -uf /tmp/jfxrt.jar com/sun/glass/ui/monocle/NativePlatformFactory.class

echo "Done"
rm -R bin/
echo "Cleaned up"
