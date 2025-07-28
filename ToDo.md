Si tengo un error con la micro sd no quiero que no pueda volver a accederse a la micro sd hasta reiniciar el dispositivo. Quiero que se monte de nuevo si es necesario o se gestione de forma que pueda volver a intentarse.

Al estar en light sleep y tener una notificación que despierte la esp32, quiero que simplemente haga el sonido de la notificación (sin mostrarla) y vuelva a entrar en light sleep.
Extra: no quiero que encienda la pantalla al despertar por una notificación.

Realizar vista de settings que permita guardar ajustes como si queremos obtener hora de wifi al iniciar el dispositivo o no.
Permitir guardar valores de forma persistente; por ejemplo la hora actual.

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