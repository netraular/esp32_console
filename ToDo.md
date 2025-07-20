
### **Propuesta de Arquitectura: Un Modelo de Vistas Basado en Clases**

**Problema Actual:**
Nuestra gestión de vistas es manual y frágil. Cada vista es responsable de su propia limpieza de recursos (timers, tareas), lo que puede llevar a fugas de memoria y fallos si algo se olvida.

**Solución Propuesta:**
Adoptaremos un modelo orientado a objetos estandarizado. Crearemos una clase **"plantilla" base (`View`)** que definirá el ciclo de vida de cada pantalla.

**¿Cómo funciona?**

1.  **Cada vista será una clase** que hereda de la plantilla `View`.
2.  El `ViewManager` usará un **registro central** para crear y destruir las vistas.
3.  Al cambiar de pantalla, el `ViewManager` **destruirá automáticamente** la vista anterior y creará la nueva.

**Beneficios Clave:**

*   **Robustez:** La limpieza de recursos se vuelve **automática y garantizada**, eliminando las fugas de memoria.
*   **Organización:** Cada vista **encapsula su propio estado y lógica** en una clase, haciendo el código más limpio y fácil de entender.
*   **Escalabilidad:** Añadir una nueva vista consistirá en **crear su clase y registrarla en un único lugar**, sin modificar la lógica principal del sistema.

---

### **Mejoras Adicionales sobre la Nueva Arquitectura**

Una vez establecida esta estructura base, podremos implementar fácilmente funcionalidades adicionales en la clase `View` para potenciar todas las vistas de forma centralizada:

1.  **Identificación Clara de Vistas:**
    Cada vista declarará un **nombre de cadena legible** (ej. "MenuView") además de su ID numérico. Esto hará que los logs de depuración sean mucho más claros (`Cargando vista 'MenuView'` en lugar de `Cargando vista 5`).

2.  **Notificaciones y Pop-ups Estándar:**
    Se añadirán métodos de ayuda en la clase base como `show_error_popup("Mensaje")`. Esto nos permitirá mostrar diálogos y notificaciones con un **estilo consistente en toda la aplicación** y con una sola línea de código, reduciendo la duplicación y simplificando la lógica de las vistas.

3.  **Registro de Botones Simplificado:**
    La clase base ofrecerá un método para **registrar los manejadores de botones de forma más limpia y segura**. Esto reducirá el código repetitivo y la posibilidad de errores al conectar un botón con la acción específica de cada vista.

En resumen, pasamos de un sistema manual propenso a errores a una arquitectura **automatizada, robusta y escalable**, que además nos servirá como cimiento para futuras mejoras.

---

### **Resumen (TL;DR) Personalizado para tu Refactorización de Vistas**

1.  **Sincroniza `main`**: Asegúrate de partir de la última versión del proyecto.
    ```bash
    git checkout main
    git pull
    ```

2.  **Crea una Rama para la Nueva Arquitectura**: Aquí vivirán todos los cambios de tu nuevo sistema de vistas, completamente aislado del código principal.
    ```bash
    # Un buen nombre es descriptivo, como este:
    git checkout -b refactor/class-based-views
    ```

3.  **Trabaja en la Rama (Commits Pequeños)**: Implementa la refactorización paso a paso. Haz un commit por cada parte lógica completada.
    *   *Ejemplo 1:* `git commit -m "Feat: Crea la clase base 'View' con ciclo de vida"`
    *   *Ejemplo 2:* `git commit -m "Refactor: Implementa ViewManager para gestionar instancias de vistas"`
    *   *Ejemplo 3:* `git commit -m "Refactor: Migra la pantalla de menú a la nueva clase MenuView"`

4.  **Sube tu Rama a GitHub**: Crea una copia de seguridad y prepara el terreno para el seguimiento.
    ```bash
    git push -u origin refactor/class-based-views
    ```

5.  **Abre un "Draft Pull Request" en GitHub**:
    *   Ve a GitHub y crea un **Pull Request en Borrador** (Draft PR) desde `refactor/class-based-views` hacia `main`.
    *   **Título del PR:** "Refactor: Implementación de arquitectura de vistas basada en clases".
    *   **Descripción:** ¡Pega la propuesta que escribiste! Es perfecta para explicar el *porqué* del cambio.
    *   Esto te permitirá ver si los tests automáticos pasan y servirá como un diario de progreso.

6.  **Cuando Termines (La Decisión Final)**:
    *   **Si la nueva arquitectura funciona y es estable**: Marca el PR como "listo para revisar", fusiónalo (merge) en `main` y celebra tu nuevo código robusto y organizado.
    *   **Si la idea no funciona o es problemática**: Simplemente **cierra el Pull Request sin fusionar** y elimina la rama `refactor/class-based-views`. **Tu rama `main` quedará 100% limpia**, como si el experimento nunca hubiera ocurrido.

Este método te da la total libertad y seguridad para construir tu nueva **arquitectura de Vistas basada en Clases** sin miedo a "ensuciar" el proyecto principal.

---










Generar archivos .h en los controllers y components con comentarios en las funciones en vez de usar readme.

Implementar popup como component en vez de crearlo de cero en cada vista.

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