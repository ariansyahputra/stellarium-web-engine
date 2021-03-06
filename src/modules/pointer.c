/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

// Render a pointer around the selected object.
// This is convenient to put this as a module, because it has to be rendered
// Before the UI.

typedef struct pointer {
    obj_t           obj;
} pointer_t;


static int pointer_render(const obj_t *obj, const painter_t *painter_)
{
    int i;
    double win_pos[2], win_size[2], angle;
    const double T = 2.0;    // Animation period.
    double r, transf[3][3];
    obj_t *selection = core->selection;
    painter_t painter = *painter_;
    vec4_set(painter.color, 1, 1, 1, 1);
    if (!selection) return 0;

    // If the selection has a custom rendering method, we use it.
    if (selection->klass->render_pointer) {
        selection->klass->render_pointer(selection, &painter);
    }

    obj_get_2d_ellipse(selection, painter.obs, painter.proj,
                       win_pos, win_size, &angle);
    r = max(win_size[0], win_size[1]);
    r += 5;
    // Draw four strokes around the object.
    for (i = 0; i < 4; i++) {
        r = max(r, 8);
        r += 0.4 * (sin(sys_get_unix_time() / T * 2 * M_PI) + 1.1);
        mat3_set_identity(transf);
        mat3_itranslate(transf, win_pos[0], win_pos[1]);
        mat3_rz(i * 90 * DD2R, transf, transf);
        mat3_itranslate(transf, r, 0);
        mat3_iscale(transf, 8, 1, 1);
        painter.lines_width = 3;
        paint_2d_line(&painter, transf, VEC(0, 0), VEC(1, 0));
    }
    return 0;
}

/*
 * Meta class declarations.
 */

static obj_klass_t pointer_klass = {
    .id = "pointer",
    .size = sizeof(pointer_t),
    .flags = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .render = pointer_render,
    .render_order = 199, // Just before the ui.
};

OBJ_REGISTER(pointer_klass)
