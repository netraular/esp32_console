
Estoy realizando la gestión de la mascota para el proyecto. Haz que en la vista de la mascota se muestre el tiempo restante hasta la siguiente fase y los puntos de cuidado actuales (ya que actualmente siempre se muestran 0 puntos) (Puede ser porqué está en fase huevo? aquí también debería poder tener puntos de cuidado).
Por último, se tendrán sprites para los huevos (que serán siempre los mismos sprites para cualquier tipo de mascota y serán aleatorios) y también se tendrán sprites para las mascotas (estos serán específicos para cada especie y para cada fase de cada especie).
Los sprites se obtendrán de la tarjeta micro sd y tendrán el siguiente tamaño: Huevo (16x16px), mascota(32x32px).
Por ahora no tengo todos los sprites generados por lo que quiero que uses el mismo archivo egg.png (16x16) para mostrar el huevo y baby.png, teen.png y adult.png para mostrar la mascota.
Decide donde guardar las imagenes en la micro sd y haz que se pueda definir en el código las propiedades de los sprites de los huevos y las mascotas y definir como y de donde obtenerlos.
Genera toda la lógica relacionada con la lectura, obtención y muestra de estos sprites.

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