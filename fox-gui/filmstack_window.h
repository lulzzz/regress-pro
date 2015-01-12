#ifndef FILMSTACK_WINDOW_H
#define FILMSTACK_WINDOW_H

#include <fx.h>

#include "stack.h"

class filmstack_window : public FXDialogBox {
    FXDECLARE(filmstack_window)

protected:
    filmstack_window() {};
private:
    filmstack_window(const filmstack_window&);
    filmstack_window &operator=(const filmstack_window&);

public:
    filmstack_window(stack_t *s, FXWindow *w, FXuint opts=DECOR_ALL,FXint pl=0,FXint pr=0,FXint pt=0,FXint pb=0,FXint hs=0,FXint vs=0);
    virtual ~filmstack_window();

    virtual void create();

private:
    FXMenuPane *popupmenu;
    int current_layer;

public:
    long on_cmd_film_menu(FXObject*,FXSelector,void* ptr);
    long on_cmd_insert_layer(FXObject*,FXSelector,void* ptr);
    long on_cmd_replace_layer(FXObject*,FXSelector,void* ptr);
    long on_cmd_delete_layer(FXObject*,FXSelector,void* ptr);
    long on_cmd_edit_layer(FXObject*,FXSelector,void* ptr);
    long on_change_name(FXObject*,FXSelector,void* ptr);
    long on_change_thickness(FXObject*,FXSelector,void* ptr);

    enum {
        ID_FILM_MENU = FXDialogBox::ID_LAST,
        ID_FILM_MENU_LAST = ID_FILM_MENU + 64,
        ID_FILM_NAME,
        ID_FILM_NAME_LAST = ID_FILM_NAME + 64,
        ID_FILM_THICKNESS,
        ID_FILM_THICKNESS_LAST = ID_FILM_THICKNESS + 64,
        ID_EDIT_LAYER,
        ID_DELETE_LAYER,
        ID_INSERT_LAYER,
        ID_REPLACE_LAYER,
        ID_LAST
    };

private:
    FXWindow *setup_stack_window(FXComposite *);
    void rebuild_stack_window();
    void notify_stack_change();

    FXWindow *stack_window;
    stack_t *stack;
};

#endif
