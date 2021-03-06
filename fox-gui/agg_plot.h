
/* agg_plot.h
 *
 * Copyright (C) 2005-2011 Francesco Abbate
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef AGGPLOT_PLOT_H
#define AGGPLOT_PLOT_H

#include <new>

#include "utils.h"
#include "list.h"
#include "canvas.h"
#include "plot_units.h"
#include "colors.h"
#include "rect.h"

#include "agg2/agg_array.h"
#include "agg2/agg_bounding_rect.h"
#include "agg2/agg_vcgen_markers_term.h"
#include "agg2/agg_conv_transform.h"
#include "agg2/agg_color_rgba.h"
#include "agg2/agg_path_storage.h"
#include "agg2/agg_array.h"
#include "agg2/agg_conv_stroke.h"
#include "agg2/agg_conv_dash.h"
#include "agg2/agg_gsv_text.h"
#include "plot_clipboard.h"

namespace newplot {

template <class VertexSource>
struct plot_item {
    VertexSource* vs;
    agg::rgba8 color;
    bool outline;

    plot_item() : vs(0) {};

    plot_item(VertexSource* vs, agg::rgba8& c, bool as_outline):
        vs(vs), color(c), outline(as_outline)
    {};

    VertexSource& vertex_source() {
        return *vs;
    };

    void xy_coordinates(cpair_table *vec)
    {
        double x, y;
        agg::trans_affine m;
        vs->apply_transform(m, 1.0);
        vs->rewind(0);
        for (unsigned cmd = vs->vertex(&x, &y); !agg::is_stop(cmd); cmd = vs->vertex(&x, &y)) {
            cpair cp = {float(x), float(y)};
            vec->add(cp);
        }
    }
};

template <class VertexSource, class ResourceManager>
class plot {
    typedef plot_item<VertexSource> item;
    typedef agg::pod_bvector<item> item_list;

    static const unsigned max_layers = 8;

public:
    typedef pod_list<item> iterator;

    plot(bool use_units = true) :
        m_root_layer(), m_layers(), m_current_layer(&m_root_layer),
        m_drawing_queue(0), m_clip_flag(true),
        m_need_redraw(true), m_rect(),
        m_use_units(use_units), m_pad_units(false), m_title_buf(),
        m_sync_mode(true) {
        compute_user_trans();

        m_title_buf.capacity(32);
        m_title = m_title_buf.data();
        m_title[0] = '\0';
    };

    virtual ~plot() {
        layer_dispose_elements(m_root_layer);
        for(unsigned k = 0; k < m_layers.size(); k++) {
            item_list *layer = m_layers[k];
            layer_dispose_elements(*layer);
            delete layer;
        }
    };

    void set_title(const char *text);
    const char *title() const {
        return m_title;
    };

    void set_units(bool use_units);
    bool use_units() const {
        return m_use_units;
    };

    void set_limits(const agg::rect_base<double>& r);

    virtual void add(VertexSource* vs, agg::rgba8& color, bool outline);
    virtual void before_draw() { };

    void draw(canvas &canvas, agg::trans_affine& m);

    virtual bool push_layer();
    virtual bool pop_layer();

    /* drawing queue related methods */
    void push_drawing_queue();
    void clear_current_layer();
    void clear_drawing_queue();
    int current_layer_index();

    bool clip_is_active() const {
        return m_clip_flag;
    };
    void set_clip_mode(bool flag) {
        m_clip_flag = flag;
    };

    bool need_redraw() const {
        return m_need_redraw;
    };
    void commit_pending_draw();

    void draw_queue(canvas &canvas, agg::trans_affine& m, opt_rect<double>& bbox);

    void sync_mode(bool req_mode) {
        m_sync_mode = req_mode;
    };
    bool sync_mode() const {
        return m_sync_mode;
    };

    void pad_mode(bool req) {
        if(req != m_pad_units) {
            m_pad_units = req;
            m_need_redraw = true;
            compute_user_trans();
        }
    };

    bool pad_mode() {
        return m_pad_units;
    };

    void xy_tables(vector_objects<cpair_table> *table_list)
    {
        unsigned n = m_current_layer->size();
        for (unsigned i = 0; i < n; i++) {
            cpair_table *t = new cpair_table();
            m_current_layer->at(i).xy_coordinates(t);
            table_list->add(t);
        }
    }

protected:
    void draw_elements(canvas &canvas, agg::trans_affine& m);
    void draw_element(item& c, canvas &canvas, agg::trans_affine& m);
    void draw_title(canvas& canvas, agg::trans_affine& m);
    void draw_axis(canvas& can, agg::trans_affine& m);

    agg::trans_affine get_scaled_matrix(agg::trans_affine& canvas_mtx);

    void clip_plot_area(canvas &canvas, agg::trans_affine& canvas_mtx);

    void compute_user_trans();

    static void viewport_scale(agg::trans_affine& trans);

    bool fit_inside(VertexSource *obj) const;

    void layer_dispose_elements(item_list& layer);

    item_list& current_layer() {
        return *m_current_layer;
    };

    item_list m_root_layer;
    agg::pod_auto_vector<item_list*, max_layers> m_layers;
    item_list *m_current_layer;

    agg::trans_affine m_trans;
    pod_list<item> *m_drawing_queue;

    bool m_clip_flag;

    bool m_need_redraw;
    opt_rect<double> m_rect;

    bool m_use_units;
    plot_units m_ux, m_uy;

private:
    bool m_pad_units;

    agg::pod_vector<char> m_title_buf;
    char *m_title;

    bool m_sync_mode;
};

static double
compute_scale(agg::trans_affine& m)
{
    return min(m.sy / 320.0, m.sx / 640.0);
}

static double
std_line_width(double scale, double w = 1.0)
{
#if 0
    const double dsf = M_LN10;
    double ls = log(scale) / dsf;
    return exp(round(ls) * dsf) * w * 1.5;
#else
    return w * 1.5;
#endif
}

template <class VS, class RM>
void plot<VS,RM>::commit_pending_draw()
{
    push_drawing_queue();
    m_need_redraw = false;
}

template <class VS, class RM>
void plot<VS,RM>::add(VS* vs, agg::rgba8& color, bool outline)
{
    item d(vs, color, outline);
    pod_list<item> *new_node = new pod_list<item>(d);
    m_drawing_queue = pod_list<item>::push_back(m_drawing_queue, new_node);
    RM::acquire(vs);
}

template <class VS, class RM>
void plot<VS,RM>::set_title(const char *text)
{
    unsigned int len = strlen(text);
    m_title_buf.resize(len+1);
    m_title = m_title_buf.data();
    memcpy(m_title, text, len+1);
}

template <class VS, class RM>
void plot<VS,RM>::push_drawing_queue()
{
    for(pod_list<item> *c = m_drawing_queue; c != 0; c = c->next()) {
        m_current_layer->add(c->content());
    }

    while(m_drawing_queue) {
        m_drawing_queue = list::pop(m_drawing_queue);
    }
}

template <class VS, class RM>
void plot<VS,RM>::clear_drawing_queue()
{
    while(m_drawing_queue) {
        item& d = m_drawing_queue->content();
        RM::dispose(d.vs);
        m_drawing_queue = list::pop(m_drawing_queue);
    }
}

template <class VS, class RM>
void plot<VS,RM>::draw(canvas &canvas, agg::trans_affine& canvas_mtx)
{
    before_draw();
    draw_title(canvas, canvas_mtx);
    if(m_use_units) {
        draw_axis(canvas, canvas_mtx);
    }
    draw_elements(canvas, canvas_mtx);
};

template <class VS, class RM>
void plot<VS,RM>::draw_title(canvas &canvas, agg::trans_affine& canvas_mtx)
{
    double xt = 0.5, yt = 1;

    agg::trans_affine m;
    this->viewport_scale(m);
    trans_affine_compose(m, canvas_mtx);

    double scale = compute_scale(canvas_mtx);

    agg::gsv_text title;
    agg::conv_stroke<agg::gsv_text> titlestroke(title);

    title.size(12.0 * scale);
    title.text(m_title);

    titlestroke.width(std_line_width(scale));
    titlestroke.line_cap(agg::round_cap);
    titlestroke.line_join(agg::round_join);

    m.transform(&xt, &yt);

    xt += -title.text_width() / 2;
    yt += 20.0 * scale;

    title.start_point(xt, yt);
    canvas.draw(titlestroke, agg::rgba(0, 0, 0));
}

template<class VS, class RM>
void plot<VS,RM>::draw_element(item& c, canvas &canvas, agg::trans_affine& m)
{
    VS& vs = c.vertex_source();
    vs.apply_transform(m, 1.0);

    if(c.outline) {
        canvas.draw_outline(vs, c.color);
    } else {
        canvas.draw(vs, c.color);
    }
}

template<class VS, class RM>
agg::trans_affine plot<VS,RM>::get_scaled_matrix(agg::trans_affine& canvas_mtx)
{
    agg::trans_affine m = m_trans;
    viewport_scale(m);
    trans_affine_compose(m, canvas_mtx);
    return m;
}

template<class VS, class RM>
void plot<VS,RM>::clip_plot_area(canvas &canvas, agg::trans_affine& canvas_mtx)
{
    if(this->clip_is_active()) {
        agg::trans_affine mvp;
        viewport_scale(mvp);
        trans_affine_compose(mvp, canvas_mtx);
        agg::rect_base<int> clip = rect_of_slot_matrix<int>(mvp);
        canvas.clip_box(clip);
    }
}

template<class VS, class RM>
void plot<VS,RM>::draw_elements(canvas &canvas, agg::trans_affine& canvas_mtx)
{
    agg::trans_affine m = get_scaled_matrix(canvas_mtx);

    this->clip_plot_area(canvas, canvas_mtx);

    for(unsigned j = 0; j < m_root_layer.size(); j++) {
        draw_element(m_root_layer[j], canvas, m);
    }

    for(unsigned k = 0; k < m_layers.size(); k++) {
        item_list& layer = *(m_layers[k]);
        for(unsigned j = 0; j < layer.size(); j++) {
            draw_element(layer[j], canvas, m);
        }
    }

    canvas.reset_clipping();
}

template<class VS, class RM>
void plot<VS,RM>::draw_queue(canvas &canvas, agg::trans_affine& canvas_mtx,
                             opt_rect<double>& bb)
{
    typedef typename plot<VS,RM>::iterator iter_type;

    before_draw();

    this->clip_plot_area(canvas, canvas_mtx);

    iter_type *c0 = m_drawing_queue;
    for(iter_type *c = c0; c != 0; c = c->next()) {
        item& d = c->content();
        agg::trans_affine m = get_scaled_matrix(canvas_mtx);
        draw_element(d, canvas, m);

        agg::rect_base<double> ebb;
        bool not_empty = agg::bounding_rect_single(d.vertex_source(), 0, &ebb.x1, &ebb.y1, &ebb.x2, &ebb.y2);

        if(not_empty) {
            bb.add<rect_union>(ebb);
        }
    }

    canvas.reset_clipping();
}

template<class VS, class RM>
void plot<VS,RM>::compute_user_trans()
{
    agg::rect_base<double> r;

    if(m_use_units && m_pad_units) {
        int ixi, ixs, iyi, iys;
        double xd, yd;
        m_ux.limits(ixi, ixs, xd);
        r.x1 = ixi * xd;
        r.x2 = ixs * xd;

        m_uy.limits(iyi, iys, yd);
        r.y1 = iyi * yd;
        r.y2 = iys * yd;
    } else {
        r = m_rect.is_defined() ? m_rect.rect() : agg::rect_base<double>(0.0, 0.0, 1.0, 1.0);
    }

    double dx = r.x2 - r.x1, dy = r.y2 - r.y1;
    double fx = (dx == 0 ? 1.0 : 1/dx), fy = (dy == 0 ? 1.0 : 1/dy);
    this->m_trans = agg::trans_affine(fx, 0.0, 0.0, fy, -r.x1 * fx, -r.y1 * fy);
}

template <class VS, class RM>
void plot<VS,RM>::draw_axis(canvas &canvas, agg::trans_affine& canvas_mtx)
{
    typedef agg::path_storage path_type;
    typedef agg::conv_dash<agg::conv_transform<path_type>, agg::vcgen_markers_term> dash_type;

    agg::trans_affine m;
    this->viewport_scale(m);
    trans_affine_compose(m, canvas_mtx);

    double scale = compute_scale(canvas_mtx);

    agg::rect_base<int> clip = rect_of_slot_matrix<int>(canvas_mtx);
    canvas.clip_box(clip);

    agg::path_storage mark;
    agg::conv_transform<path_type> mark_tr(mark, m);
    agg::conv_stroke<agg::conv_transform<path_type> > mark_stroke(mark_tr);

    agg::path_storage ln;
    agg::conv_transform<path_type> lntr(ln, m);
    dash_type lndash(lntr);
    agg::conv_stroke<dash_type> lns(lndash);

    const double yeps = 1.0e-3;
    const double xeps = 1.0e-3;

    {
        int jinf = m_uy.begin(), jsup = m_uy.end();
        for(int j = jinf; j <= jsup; j++) {
            double x = 0.0, y = m_uy.mark_value(j);
            this->m_trans.transform(&x, &y);
            if(y >= - yeps && y <= 1.0 + yeps) {
                agg::gsv_text lab;
                agg::conv_stroke<agg::gsv_text> labs(lab);
                char lab_text[32];
                double xlab = 0, ylab = y;

                lab.size(12.0 * scale);
                m_uy.mark_label(lab_text, 32, j);
                lab.text(lab_text);
                labs.width(std_line_width(scale));

                m.transform(&xlab, &ylab);

                xlab += -lab.text_width() - 10.0 * scale;
                ylab += -5.0 * scale;

                lab.start_point(xlab, ylab);
                canvas.draw(labs, agg::rgba(0, 0, 0));

                mark.move_to(0.0, y);
                mark.line_to(-0.01, y);

                if(j > jinf && j < jsup) {
                    ln.move_to(0.0, y);
                    ln.line_to(1.0, y);
                }
            }
        }
    }

    {
        int jinf = m_ux.begin(), jsup = m_ux.end();
        for(int j = jinf; j <= jsup; j++) {
            double x = m_ux.mark_value(j), y = m_uy.mark_value(j);
            this->m_trans.transform(&x, &y);
            if(x >= - xeps && x <= 1.0 + xeps) {
                agg::gsv_text lab;
                agg::conv_stroke<agg::gsv_text> labs(lab);
                char lab_text[32];
                double xlab = x, ylab = 0;

                lab.size(12.0 * scale);
                m_ux.mark_label(lab_text, 32, j);
                lab.text(lab_text);
                labs.width(std_line_width(scale));

                m.transform(&xlab, &ylab);

                xlab += -lab.text_width()/2.0;
                ylab += -24.0 * scale;

                lab.start_point(xlab, ylab);
                canvas.draw(labs, agg::rgba(0, 0, 0));

                mark.move_to(x, 0.0);
                mark.line_to(x, -0.01);

                if(j > jinf && j < jsup) {
                    ln.move_to(x, 0.0);
                    ln.line_to(x, 1.0);
                }
            }
        }
    }

    lndash.add_dash(7.0, 3.0);

    lns.width(std_line_width(scale, 0.25));
    canvas.draw(lns, colors::black);

    mark_stroke.width(std_line_width(scale, 0.75));
    canvas.draw(mark_stroke, colors::black);

    agg::path_storage box;
    agg::conv_transform<path_type> boxtr(box, m);
    agg::conv_stroke<agg::conv_transform<path_type> > boxs(boxtr);

    box.move_to(0.0, 0.0);
    box.line_to(0.0, 1.0);
    box.line_to(1.0, 1.0);
    box.line_to(1.0, 0.0);
    box.close_polygon();

    boxs.width(std_line_width(scale));

    canvas.draw(boxs, colors::black);

    canvas.reset_clipping();
};

template<class VS, class RM>
void plot<VS,RM>::viewport_scale(agg::trans_affine& m)
{
    const double xoffs = 0.09375, yoffs_d = 0.09375, yoffs_u = 1.5*0.09375;
    static agg::trans_affine rsz(1-2*xoffs, 0.0, 0.0, 1-(yoffs_d+yoffs_u), xoffs, yoffs_d);
    trans_affine_compose(m, rsz);
}

template<class VS, class RM>
void plot<VS,RM>::set_units(bool use_units)
{
    if(m_use_units != use_units) {
        m_use_units = use_units;
        m_need_redraw = true;
        compute_user_trans();
    }
}

template<class VS, class RM>
void plot<VS,RM>::set_limits(const agg::rect_base<double>& r)
{
    m_rect.set(r);
    m_ux = plot_units(r.x1, r.x2);
    m_uy = plot_units(r.y1, r.y2);
    m_need_redraw = true;
    compute_user_trans();
}

template<class VS, class RM>
void plot<VS,RM>::layer_dispose_elements(plot<VS,RM>::item_list& layer)
{
    unsigned n = layer.size();
    for(unsigned k = 0; k < n; k++) {
        RM::dispose(layer[k].vs);
    }
}

template<class VS, class RM>
bool plot<VS,RM>::push_layer()
{
    if(m_layers.size() >= max_layers) {
        return false;
    }

    item_list *new_layer = new(std::nothrow) item_list();
    if(new_layer == 0) {
        return false;
    }

    commit_pending_draw();
    m_layers.add(new_layer);
    m_current_layer = new_layer;

    return true;
}

template<class VS, class RM>
bool plot<VS,RM>::pop_layer()
{
    unsigned n = m_layers.size();

    if(n == 0) {
        return false;
    }

    item_list *layer = m_layers[n-1];
    layer_dispose_elements(*layer);
    delete layer;

    m_layers.inc_size(-1);
    n--;

    clear_drawing_queue();
    m_need_redraw = true;
    m_current_layer = (n > 0 ? m_layers[n-1] : &m_root_layer);

    return true;
}

template <class VS, class RM>
void plot<VS,RM>::clear_current_layer()
{
    clear_drawing_queue();
    layer_dispose_elements(current_layer());
    m_current_layer->clear();
}

template <class VS, class RM>
int plot<VS,RM>::current_layer_index()
{
    return m_layers.size();
}

}

#endif
