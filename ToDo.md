Implementar notificaciones.
Las notificaciones quiero que sean más como alertas o popups que requieran interacción siempre, aunque sea un simple botón para confirmar que has leido el contenido por lo que en vez de usar toasts, querría reutilizar el sistema de popups actual.
Querría que las notificaciones visuales solo se mostraran en la vista de standby. En cualquier otra vista no debe haber notificaciones. Y querría que fueran popups (con sonido al aparecer).
Además, si hay un popup activo no quiero que aparezca otro en la misma pantalla por lo que las otras notificaciones se quedarían guardadas en una "cola" que podríamos ver dentro de otra vista como si fuera un historial de notificaciones pendientes.
Además, en el caso de que el dispositivo esté dormido, si había una notificación programada, quiero que despierte al dispositivo sin encender la pantalla, reproduzca un sonido de la micro sd y vuelva a light sleep.
Implement Sleep/Wake Functionality for notifications.
```
Create the AlarmManager and integrate it with the PowerManager to handle waking the device for scheduled notifications.
```

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