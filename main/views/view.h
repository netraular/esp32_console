#ifndef VIEW_H
#define VIEW_H

#include "lvgl.h"

/**
 * @brief Abstract base class for all views in the application.
 *
 * This class defines the basic lifecycle of a view. Each specific view
 * (e.g., StandbyView) should inherit from this class and implement its
 * virtual methods. The ViewManager will manage pointers to View objects,
 * ensuring that their destructors are called automatically for resource
 * cleanup when the view is changed.
 */
class View {
public:
    /**
     * @brief Virtual destructor.
     *
     * Ensures that the destructor of the derived class is called when a
     * View object is deleted through a base class pointer. This is the
     * cornerstone of the automatic resource cleanup mechanism.
     */
    virtual ~View() = default;

    /**
     * @brief Creates the UI elements for the view.
     *
     * This pure virtual function must be implemented by all derived classes.
     * It's responsible for creating all LVGL widgets and setting up the
     * initial state of the view.
     *
     * @param parent The parent LVGL object on which the view will be created.
     */
    virtual void create(lv_obj_t* parent) = 0;

protected:
    lv_obj_t* container = nullptr; // Main container for the view's widgets
};

#endif // VIEW_H