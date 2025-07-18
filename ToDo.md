Simplemente ejecutar el prompt con las vistas siguientes desde pomodoro para revisar que todas tengan un callback para el clean up correcto de los recursos al pulsar ON/OFF y cambiar a la vista standby

Generar archivos .h con comentarios en las funciones en vez de usar readme.

Revisar todas las vistas sobre todo para ver que la opción de cerrar la vista limpie todos los recursos correctamente. (Ya que el botón de on/off va a cerrar la vista en cualquier estado)

Al iniciar el dispositivo, la conexión wifi puede tardar un rato y consumir más energía por lo que puede que no se lea la micro sd al inicio y si se entra luego no funcione correctamentela reproducción de archivos.
Se debería esprar a hacer las inicializaciones de forma progresiva evitando que se avanze la pantalla de standby y mostrando "Loading..." hasta que todo esté funcional?

Añadir tiempo a la vista standby
Arreglar envio audio por streaming.
Permitir reproducir otro tipo de archivos de audio (no solo wav)
Realizar ejemplo de vistas funcional led rgb.
Implementar función sleep de esp32.
Implementar "notificaciones" en la pantalla lock screen.
Implementar vista de ajustes (para volumen o wifi).
Implementar inicialización de la conexión a wifi mediante qr code o similar.