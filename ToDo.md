
Por ahora solo quiero añadir las funcionalidades para el crecimiento de la mascota (pero teniendo en cuenta las funcionalidades a implementar futuras).
Realiza la implementación de la mascota en una vista donde se simule con un cuadrado y el nombre del estado de la mascota la "evolución" en la que está (huevo, bebé, niño y adulto). Y además querré tener un personaje principal que suba de nivel y pueda decorar como personaje del jugador y tenga dinero y niveles.
Por ahora crea la estructura de datos para el jugador y la mascota pero solo implementa la visión y gestión de la mascota.
Quiero que la mascota evolucione durante 2 semanas empezando siempre un lunes y que, empieze siendo 1 huevo el lunes, luego del martes al jueves sea un bebé y el viernes evolucione a niño (teen). Luego, si el domingo se ha cuidado la mascota (por ejemplo haciendo habitos, pomodoros, diarios... que le den puntos de cuidado) evolucionará a adulto durante toda la siguiente semana. En caso de que no tenga suficientes puntos de cuidado empezaremos con un huevo de nuevo ya que perderemos la mascota actual.
Si conseguimos el adulto, al llegar al domingo de la siguiente semana se mirarán de nuevo los puntos de cuidado y si tiene suficientes lo guardaremos como conseguido en la colección. Si no tiene suficientes puntos de cuidado lo perderemos (pero quedará marcado como encontrado).
Crea toda la gestión de la mascota con, por ejemplo, 5 mascotas distintas con nombres placeholder distintos como "pet1_baby","pet1_teen","pet1_adult".

Añade la pantalla de colección con la información de todas las mascotas encontradas, las que queden por encontrar y mostrando un cuadrado con '?' si no se se ha encontrado nunca la mascota, un icono negro si se ha visto en modo adulto pero no se ha quedado y en color si se ha visto y se ha quedado la mascota.


Hacer que al pulsar un botón se muestre un log indicando que botón se ha pulsado.

Si tengo un error con la micro sd no quiero que no pueda volver a accederse a la micro sd hasta reiniciar el dispositivo. Quiero que se monte de nuevo si es necesario o se gestione de forma que pueda volver a intentarse.

Se debería esprar a hacer las inicializaciones de forma progresiva evitando que se avanze la pantalla de standby y mostrando "Loading..." hasta que todo esté funcional en vez de dejar que el usuario se mueva entre menus mientras se conecta al wifi?

Añadir tiempo a la vista standby
Arreglar envio audio por streaming.
Permitir reproducir otro tipo de archivos de audio (no solo wav)
Realizar ejemplo de vistas funcional led rgb.
Implementar "notificaciones" en la pantalla lock screen.
Implementar vista de ajustes (para volumen o wifi).
Implementar inicialización de la conexión a wifi mediante qr code o similar.
Cargar imagenes .png y .bmp