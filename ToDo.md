Implementar notificaciones.
Las notificaciones quiero que sean más como alertas o popups que requieran interacción siempre, aunque sea un simple botón para confirmar que has leido el contenido por lo que en vez de usar toasts, querría reutilizar el sistema de popups actual.
Querría que las notificaciones visuales solo se mostraran en la vista de standby. En cualquier otra vista no debe haber notificaciones. Y querría que fueran popups (con sonido al aparecer).
Además, si hay un popup activo no quiero que aparezca otro en la misma pantalla por lo que las otras notificaciones se quedarían guardadas en una "cola" que podríamos ver dentro de otra vista como si fuera un historial de notificaciones pendientes.
Además, en el caso de que el dispositivo esté dormido, si había una notificación programada, quiero que despierte al dispositivo sin encender la pantalla, reproduzca un sonido de la micro sd y vuelva a light sleep.
```
    ✅ Task 1: Core Data Structure & Manager.

        Create a new data model header notification_data_model.h to define the structure of a Notification.

        Create the NotificationManager class (.h and .cpp) with the fundamental logic to add, retrieve, and manage notifications in an in-memory queue.

        Add the new files to the CMakeLists.txt build system.

    Task 2: System Integration & The "Dispatcher" Logic.

        Initialize the NotificationManager in main.cpp.

        Implement the "Dispatcher" logic inside NotificationManager: a periodic timer that checks conditions (Is the view Standby? Is a popup already active?) and shows a notification popup if necessary.

        Add helper functions to ViewManager and PopupManager to allow these checks.

    Task 3: Create the "Add Notification" View.

        Create a new view class (AddNotificationView).

        This view will have two lv_textarea widgets for the title and message, and a "Save" button.

        Pressing "Save" will call NotificationManager::add_notification() and return to the menu.

    Task 4: Integrate the New View into the System.

        Add the new VIEW_ID_ADD_NOTIFICATION to the view_id_t enum in view_manager.h.

        Add the view to the factory in view_manager.cpp.

        Add an entry in MenuView to allow launching the new view.

    Task 5: Implement Persistence.

        Modify NotificationManager to save the notification queue to a file on LittleFS when changes occur and load it on startup.

    Task 6 (Future): Implement the Notification History View.

        Create a NotificationHistoryView to display a list of all unread notifications from the manager.

    Task 7 (Future): Implement Sleep/Wake Functionality.

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