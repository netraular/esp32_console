En el habit history view, hacer que el streak de hábitos se muestre en la esquina superior derecha con un icono y el número de hábitos recurrentes. Además, en la parte inferior ahora se deberá mostrar las 3 primeras letras del nombre del mes que corresponda a la semana que contenga el día 8 del mes.

Estandarizar algun ejemplo de notificación y de pop-up. Se añadirán métodos de ayuda en la clase base como `show_error_popup("Mensaje")`. Esto nos permitirá mostrar diálogos y notificaciones con un **estilo consistente en toda la aplicación** y con una sola línea de código, reduciendo la duplicación y simplificando la lógica de las vistas.
Implementar popup como component en vez de crearlo de cero en cada vista.

Realizar vista de settings que permita guardar ajustes como si queremos obtener hora de wifi al iniciar el dispositivo o no.
Permitir guardar valores de forma persistente; por ejemplo la hora actual.


Generar archivos .h en los controllers y components con comentarios en las funciones en vez de usar readme.


Al iniciar el dispositivo, la conexión wifi puede tardar un rato y consumir más energía por lo que puede que no se lea la micro sd al inicio y si se entra luego no funcione correctamentela reproducción de archivos.
Se debería esprar a hacer las inicializaciones de forma progresiva evitando que se avanze la pantalla de standby y mostrando "Loading..." hasta que todo esté funcional?

Añadir tiempo a la vista standby
Arreglar envio audio por streaming.
Permitir reproducir otro tipo de archivos de audio (no solo wav)
Realizar ejemplo de vistas funcional led rgb.
Implementar "notificaciones" en la pantalla lock screen.
Implementar vista de ajustes (para volumen o wifi).
Implementar inicialización de la conexión a wifi mediante qr code o similar.
Cargar imagenes .png y .bmp