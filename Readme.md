
Facebook
/
zstd
Público
Zstandard: algoritmo de compresión rápido en tiempo real

www.zstd.net
Licencia
 Desconocido, licencias GPL-2.0 encontradas
 19,1k estrellas 1,8k horquillas 
Código
Cuestiones
109
Solicitudes de extracción
21
Comportamiento
Proyectos
wiki
Seguridad
Perspectivas
facebook/zstd
Última confirmación
@Cyan4973
cian4973
…
1 hour ago
Estadísticas de Git
archivos
LÉAME.md
Zestándar

Zstandard , o zstdsu versión abreviada, es un algoritmo de compresión rápido sin pérdidas, que se enfoca en escenarios de compresión en tiempo real a nivel de zlib y mejores relaciones de compresión. Está respaldado por una etapa de entropía muy rápida, proporcionada por la biblioteca Huff0 y FSE .

El formato de Zstandard es estable y está documentado en RFC8878 . Ya están disponibles múltiples implementaciones independientes. Este repositorio representa la implementación de referencia, proporcionada como una biblioteca C con licencia dual BSD y GPLv2 de código abierto , y una utilidad de línea de comandos que produce y decodifica archivos , y . Si su proyecto requiere otro lenguaje de programación, se proporciona una lista de puertos y enlaces conocidos en la página de inicio de Zstandard ..zst.gz.xz.lz4

Estado de la rama de desarrollo:

Estado de construcción Estado de construcción Estado de construcción Estado de construcción Estado de fuzzing

Puntos de referencia
Como referencia, se probaron y compararon varios algoritmos de compresión rápida en un escritorio con Ubuntu 20.04 ( Linux 5.11.0-41-generic), con una CPU Core i7-9700K a 4,9 GHz, utilizando lzbench , un punto de referencia en memoria de código abierto de @inikep compilado con gcc 9.3. 0, sobre el corpus de compresión de Silesia .

nombre del compresor	Relación	Compresión	Descomprimir.
zstd 1.5.1 -1	2.887	530 MB/s	1700 MB/s
zlib 1.2.11 -1	2.743	95 MB/s	400 MB/s
brotli 1.0.9 -0	2.702	395 MB/s	450 MB/s
zstd 1.5.1 --rápido=1	2.437	600 MB/s	2150 MB/s
zstd 1.5.1 --rápido=3	2.239	670 MB/s	2250 MB/s
rápidolz 1.5.0 -1	2.238	540 MB/s	760 MB/s
zstd 1.5.1 --rápido=4	2.148	710 MB/s	2300 MB/s
lzo1x 2.10 -1	2.106	660 MB/s	845 MB/s
lz4 1.9.3	2.101	740 MB/s	4500 MB/s
lzf 3.6 -1	2.077	410 MB/s	830 MB/s
rápido 1.1.9	2.073	550 MB/s	1750 MB/s
Los niveles de compresión negativos, especificados con --fast=#, ofrecen una mayor velocidad de compresión y descompresión a costa de la relación de compresión (en comparación con el nivel 1).

Zstd también puede ofrecer relaciones de compresión más fuertes a costa de la velocidad de compresión. La compensación entre velocidad y compresión se puede configurar en pequeños incrementos. La velocidad de descompresión se conserva y permanece aproximadamente igual en todas las configuraciones, una propiedad compartida por la mayoría de los algoritmos de compresión LZ, como zlib o lzma.

Las siguientes pruebas se ejecutaron en un servidor que ejecuta Linux Debian ( Linux version 4.14.0-3-amd64) con una CPU Core i7-6700K @ 4.0GHz, utilizando lzbench , un punto de referencia en memoria de código abierto de @inikep compilado con gcc 7.3.0, en el corpus de compresión de Silesia .

Velocidad de compresión frente a relación	Velocidad de descompresión
Velocidad de compresión frente a relación	Velocidad de descompresión
Algunos otros algoritmos pueden producir relaciones de compresión más altas a velocidades más lentas, quedando fuera del gráfico. Para ver una imagen más grande, incluidos los modos lentos, haga clic en este enlace .

El caso de la compresión de Small Data
Los gráficos anteriores proporcionan resultados aplicables a escenarios típicos de archivos y secuencias (varios MB). Los datos pequeños vienen con diferentes perspectivas.

Cuanto menor sea la cantidad de datos a comprimir, más difícil será comprimir. Este problema es común a todos los algoritmos de compresión y la razón es que los algoritmos de compresión aprenden de los datos pasados ​​cómo comprimir los datos futuros. Pero al comienzo de un nuevo conjunto de datos, no hay un "pasado" sobre el cual construir.

Para resolver esta situación, Zstd ofrece un modo de entrenamiento , que se puede usar para ajustar el algoritmo para un tipo de datos seleccionado. El entrenamiento de Zstandard se logra proporcionándole algunas muestras (un archivo por muestra). El resultado de este entrenamiento se almacena en un archivo llamado "diccionario", que debe cargarse antes de la compresión y descompresión. Con este diccionario, la relación de compresión que se puede lograr con datos pequeños mejora drásticamente.

El siguiente ejemplo utiliza el github-users conjunto de muestras , creado a partir de la API pública de github . Consiste en aproximadamente 10 000 registros que pesan alrededor de 1 KB cada uno.

Índice de compresión	Velocidad de compresión	Velocidad de descompresión
Índice de compresión	Velocidad de compresión	Velocidad de descompresión
Estas ganancias de compresión se logran al mismo tiempo que proporcionan velocidades de compresión y descompresión más rápidas.

El entrenamiento funciona si existe alguna correlación en una familia de pequeñas muestras de datos. Cuanto más específico de datos es un diccionario, más eficiente es (no existe un diccionario universal ). Por lo tanto, implementar un diccionario por tipo de datos proporcionará los mayores beneficios. Las ganancias del diccionario son principalmente efectivas en los primeros KB. Luego, el algoritmo de compresión utilizará gradualmente el contenido previamente decodificado para comprimir mejor el resto del archivo.

Compresión de diccionario Cómo:
crea el diccionario

zstd --train FullPathToTrainingSet/* -o dictionaryName

Comprimir con diccionario

zstd -D dictionaryName FILE

Descomprimir con diccionario

zstd -D dictionaryName --decompress FILE.zst

Instrucciones de construcción
makees el sistema de compilación mantenido oficialmente de este proyecto. Todos los demás sistemas de compilación son "compatibles" y mantenidos por terceros, pueden presentar pequeñas diferencias en las opciones avanzadas. Cuando su sistema lo permita, prefiera usar maketo build zstdy libzstd.

Makefile
Si su sistema es compatible con el estándar make(o gmake), la invocación makeen el directorio raíz generará zstdcli en el directorio raíz. También se creará libzstden lib/.

Otras opciones disponibles incluyen:

make install: crear e instalar zstd cli, biblioteca y páginas man
make check: crear y ejecutar zstd, probar su comportamiento en la plataforma local
MakefileSigue las convenciones GNU Standard Makefile , lo que permite la instalación por etapas, indicadores estándar, variables de directorio y variables de comando.

Para casos de uso avanzado, se documentan marcas de compilación especializadas que controlan la generación binaria en lib/README.mdla libzstdbiblioteca y en programs/README.mdla zstdCLI.

hacer
Se cmakeproporciona un generador de proyectos dentro de build/cmake. Puede generar Makefiles u otros scripts de compilación para crear bibliotecas zstdbinarias, libzstddinámicas y estáticas.

De forma predeterminada, CMAKE_BUILD_TYPEse establece en Release.

Mesón
Se proporciona un proyecto Meson dentro de build/meson. Siga las instrucciones de compilación en ese directorio.

También puede consultar el .travis.ymlarchivo para ver un ejemplo sobre cómo se usa Meson para construir este proyecto.

Tenga en cuenta que el tipo de compilación predeterminado es release .

VCPKG
Puede compilar e instalar el administrador de dependencias zstd vcpkg :

git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
./vcpkg integrate install
./vcpkg install zstd
Los miembros del equipo de Microsoft y los colaboradores de la comunidad mantienen actualizado el puerto zstd en vcpkg. Si la versión no está actualizada, cree una publicación o una solicitud de extracción en el repositorio de vcpkg.

Estudio Visual (Windows)
Al entrar en el builddirectorio, encontrará posibilidades adicionales:

Proyectos para Visual Studio 2005, 2008 y 2010.
El proyecto VS2010 es compatible con VS2012, VS2013, VS2015 y VS2017.
Scripts de compilación automatizados para el compilador Visual de @KrzysFR , en build/VS_scripts, que compilarán zstdcli y libzstdbiblioteca sin necesidad de abrir la solución de Visual Studio.
Dólar
Puede compilar el binario zstd a través de buck ejecutando: buck build programs:zstddesde la raíz del repositorio. El binario de salida estará en buck-out/gen/programs/.

Pruebas
Puede ejecutar pruebas de humo locales rápidas ejecutando el playTest.shscript desde el src/testsdirectorio. Se necesitan dos variables env $ZSTD_BINpara $DATAGEN_BINque el script de prueba localice el binario zstd y datagen. Para obtener información sobre las pruebas de IC, consulte TESTING.md

Estado
Zstandard se implementa actualmente en Facebook. Se usa continuamente para comprimir grandes cantidades de datos en múltiples formatos y casos de uso. Zstandard se considera seguro para entornos de producción.

Licencia
Zstandard tiene doble licencia bajo BSD y GPLv2 .

contribuyendo
La devsucursal es aquella donde se fusionan todas las cotizaciones antes de llegar a release. Si planea proponer un parche, comprométase en la devrama o en su propia rama de funciones. releaseNo se permiten compromisos directos . Para obtener más información, lea CONTRIBUYENDO .

Lanzamientos 64
Z estándar v1.5.2
El último
el 20 de enero
+ 63 lanzamientos
Paquetes
