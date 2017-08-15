
/* disp-ho.c
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

#include <assert.h>
#include <string.h>
#include "physical-constants.h"
#include "dispers.h"
#include "cmpl.h"

static void     ho_free(disp_t *d);
static disp_t * ho_copy(const disp_t *d);

static cmpl ho_n_value(const disp_t *disp, double lam);
static cmpl ho_n_value_deriv(const disp_t *disp, double lam,
                             cmpl_vector *der);
static int  ho_fp_number(const disp_t *disp);
static double * ho_map_param(disp_t *d, int index);
static int  ho_apply_param(struct disp_struct *d,
                           const fit_param_t *fp, double val);
static void ho_encode_param(str_t param, const fit_param_t *fp);

static double ho_get_param_value(const struct disp_struct *d,
                                 const fit_param_t *fp);

static int ho_write(writer_t *w, const disp_t *_d);
static int ho_read(lexer_t *l, disp_t *d);

struct disp_class ho_disp_class = {
    .disp_class_id       = DISP_HO,
    .full_name           = "Harmonic Oscillator",
    .short_name          = "ho",

    .free                = ho_free,
    .copy                = ho_copy,

    .n_value             = ho_n_value,
    .fp_number           = ho_fp_number,
    .n_value_deriv       = ho_n_value_deriv,
    .apply_param         = ho_apply_param,
    .map_param           = ho_map_param,
    .get_param_value     = ho_get_param_value,

    .encode_param        = ho_encode_param,
    .write               = ho_write,
    .read                = ho_read,
};

static const char *ho_param_names[] = {"Nosc", "En", "Eg", "Nu", "Phi"};

/* The HO multiplicative factor is: 16 * Pi * Ry^2 * r0^3 where
   Ry is the Rydberg constant Ry = 13.6058 eV et r0 is the Bohr radius:
   r0 = 0.0529177 nm.
   The definition above would give the following constant:

#define HO_MULT_FACT 1.3788623090164978586499199863586
*/

// #define HO_MULT_FACT 1.37886
#define HO_MULT_FACT 1.3788623090164978586499199863586
#define HO_EXTRA_PARAMS_NO 3
#define HO_NB_PARAMS 5
#define HO_NOSC_OFFS 0
#define HO_EN_OFFS   1
#define HO_EG_OFFS   2
#define HO_NU_OFFS   3
#define HO_PHI_OFFS  4

#define HO_PARAM_NB(hn,pn) (HO_NB_PARAMS * (hn) + pn)

void
ho_free(disp_t *d)
{
    struct disp_ho *ho = & d->disp.ho;
    assert(d->type == DISP_HO);
    free(ho->params);
    disp_base_free(d);
}

disp_t *
ho_copy(const disp_t *src)
{
    struct disp_struct *res;
    struct disp_ho *ho;
    struct ho_params *params;

    res = disp_base_copy(src);

    ho = & res->disp.ho;
    params = ho->params;
    ho->params = emalloc(sizeof(struct ho_params) * ho->nb_hos);
    memcpy(ho->params, params, sizeof(struct ho_params) * ho->nb_hos);

    return res;
}

void
disp_add_ho(struct disp_struct *d)
{
    struct disp_ho *ho = &d->disp.ho;
    int n = ho->nb_hos;
    struct ho_params *params = emalloc(sizeof(struct ho_params) * (n + 1));
    memcpy(params, ho->params, sizeof(struct ho_params) * n);
    params[n].nosc = 0.0;
    params[n].en = 15.7;
    params[n].eg = 0.3;
    params[n].nu = 1.0 / 3.0;
    params[n].phi = 0.0;

    free(ho->params);
    ho->params = params;
    ho->nb_hos = n + 1;
}

void
disp_delete_ho(struct disp_struct *d, int index)
{
    struct disp_ho *ho = &d->disp.ho;
    int n = ho->nb_hos;
    int nrem = n - index - 1;
    memcpy(ho->params + index, ho->params + index + 1, sizeof(struct ho_params) * nrem);
    ho->nb_hos = n - 1;
}

cmpl
ho_n_value(const disp_t *disp, double lam)
{
    return ho_n_value_deriv(disp, lam, NULL);
}

cmpl
ho_n_value_deriv(const disp_t *d, double lambda, cmpl_vector *pd)
{
    const struct disp_ho *m = & d->disp.ho;
    const int nb = m->nb_hos;
    const double e = EV_TO_NM / lambda;
    cmpl hh[nb], invhhden[nb];

    cmpl hsum = 0.0, hnusum = 0.0;
    for (int k = 0; k < nb; k++) {
        const struct ho_params *p = m->params + k;
        const double nosc = p->nosc, en = p->en, eg = p->eg, nu = p->nu, phi = p->phi;

        invhhden[k] = 1 / (SQR(en) - SQR(e) + I * eg * e);
        hh[k] = HO_MULT_FACT * nosc * (cos(phi) - I * sin(phi)) * invhhden[k];

        hsum += hh[k];
        hnusum += nu * hh[k];
    }

    const double ehm1 = m->eps_host - 1;
    const double alphah = 1 + m->nu_host * ehm1;
    const cmpl invden = 1 / (-m->nu_host * ehm1 + alphah * (1 - hnusum));
    const cmpl epsnum = ehm1 + alphah * hsum;
    const cmpl eps = m->eps_inf + epsnum * invden;
    cmpl n = csqrt(eps);
    const int chop_k = (cimag(n) > 0.0);

    if(chop_k) {
        n = creal(n) + I * 0.0;
    }

    if(pd == NULL) {
        return n;
    }

    const cmpl epsfact = 1 / (2 * csqrt(eps));

    const cmpl dndnuhc = epsfact * alphah * epsnum * SQR(invden);
    for (int k = 0; k < nb; k++) {
        const struct ho_params *p = m->params + k;
        const double nosc = p->nosc, en = p->en, nu = p->nu;
        const int koffs = k * HO_NB_PARAMS + HO_EXTRA_PARAMS_NO;

        cmpl_vector_set(pd, koffs + HO_NU_OFFS, dndnuhc * hh[k]);

        const cmpl dndh = alphah * (alphah * nu * hsum * invden + 1) * invden;
        const cmpl hhdndh = epsfact * dndh * hh[k];

        cmpl_vector_set(pd, koffs + HO_NOSC_OFFS, hhdndh / nosc);
        cmpl_vector_set(pd, koffs + HO_PHI_OFFS, - I * hhdndh);

        const cmpl depsde = hhdndh * invhhden[k];
        cmpl_vector_set(pd, koffs + HO_EN_OFFS, - 2 * en * depsde);
        cmpl_vector_set(pd, koffs + HO_EG_OFFS, - I * e * depsde);
    }

    cmpl_vector_set(pd, 0, epsfact); /* Derivative with eps_inf. */
    cmpl_vector_set(pd, 1, 0); /* Derivative with eps_host. TO BE DONE */
    cmpl_vector_set(pd, 2, 0); /* Derivative with nu_host. TO BE DONE */

    if(chop_k) {
        for (int k = 0; k < nb * HO_NB_PARAMS + HO_EXTRA_PARAMS_NO; k++) {
            const cmpl y = cmpl_vector_get(pd, k);
            cmpl_vector_set(pd, k, creal(y) + I * 0.0);
        }
    }

    return n;
}

int
ho_fp_number(const disp_t *disp)
{
    return disp->disp.ho.nb_hos * HO_NB_PARAMS + HO_EXTRA_PARAMS_NO;
}

int disp_ho_oscillator_parameters_number(struct disp_struct *d)
{
    return HO_NB_PARAMS;
}

double *
ho_map_param(disp_t *_d, int index)
{
    struct disp_ho *d = &_d->disp.ho;
    const int osc_params_no = d->nb_hos * HO_NB_PARAMS;
    const int ho_index = index - HO_EXTRA_PARAMS_NO;
    if (index < HO_EXTRA_PARAMS_NO) {
        switch (index) {
        case 0: return &d->eps_inf;
        case 1: return &d->eps_host;
        case 2: return &d->nu_host;
        default: ;
        }
    } else if (ho_index < osc_params_no) {
        int no = ho_index / HO_NB_PARAMS;
        int np = ho_index % HO_NB_PARAMS;
        struct ho_params *ho = d->params + no;
        switch(np) {
        case 0: return &ho->nosc;
        case 1: return &ho->en;
        case 2: return &ho->eg;
        case 3: return &ho->nu;
        default: return &ho->phi;
        }
    }
    return NULL;
}

void
ho_encode_param(str_t param, const fit_param_t *fp)
{
    const int ho_index = fp->param_nb - HO_EXTRA_PARAMS_NO;
    if (fp->param_nb < HO_EXTRA_PARAMS_NO) {
        switch (fp->param_nb) {
        case 0:
            str_copy_c(param, "EpsInf");
            break;
        case 1:
            str_copy_c(param, "Eps_host");
            break;
        case 2:
            str_copy_c(param, "Nu_host");
            break;
        default: ;
        }
    } else {
        int onb = ho_index / HO_NB_PARAMS;
        int pnb = ho_index % HO_NB_PARAMS;
        str_printf(param, "%s:%i", ho_param_names[pnb], onb);
    }
}

int
ho_apply_param(struct disp_struct *disp, const fit_param_t *fp,
               double val)
{
    double *pval = ho_map_param(disp, fp->param_nb);
    if(! pval) return 1;
    *pval = val;
    return 0;
}

double
ho_get_param_value(const disp_t *d, const fit_param_t *fp)
{
    const double *pval = ho_map_param((disp_t *) d, fp->param_nb);
    assert(pval != NULL);
    return *pval;
}

static void
set_default_eps_host(struct disp_ho *ho)
{
    ho->eps_inf = 1.0;
    ho->eps_host = 1.0;
    ho->nu_host = 0.0;
}

static int
eps_host_is_default(const struct disp_ho *ho)
{
    return (ho->eps_inf == 1.0 && ho->eps_host == 1.0 && ho->nu_host == 0.0);
}

disp_t *
disp_new_ho(const char *name, int nb_hos, struct ho_params *params)
{
    disp_t *d = disp_new_with_name(DISP_HO, name);

    struct disp_ho *ho = &d->disp.ho;
    set_default_eps_host(ho);
    ho->nb_hos = nb_hos;
    ho->params = emalloc(nb_hos * sizeof(struct ho_params));
    memcpy(ho->params, params, nb_hos * sizeof(struct ho_params));

    return d;
}

int
ho_write(writer_t *w, const disp_t *_d)
{
    const struct disp_ho *d = &_d->disp.ho;
    if (!eps_host_is_default(d)) {
        writer_printf(w, "extension %g %g %g", d->eps_inf, d->eps_host, d->nu_host);
        writer_newline(w);
    }
    writer_printf(w, "%d", d->nb_hos);
    writer_newline(w);
    struct ho_params *ho = d->params;
    int i;
    for (i = 0; i < d->nb_hos; i++, ho++) {
        if (i > 0) {
            writer_newline(w);
        }
        writer_printf(w, "%g %g %g %g %g", ho->nosc, ho->en, ho->eg, ho->nu, ho->phi);
    }
    writer_newline_exit(w);
    return 0;
}

int
ho_read(lexer_t *l, disp_t *d_gen)
{
    struct disp_ho *d = &d_gen->disp.ho;
    d->params = NULL;
    if (lexer_check_ident(l, "extension") == 0) {
        if (lexer_number(l, &d->eps_inf)) return 1;
        if (lexer_number(l, &d->eps_host)) return 1;
        if (lexer_number(l, &d->nu_host)) return 1;
    } else {
        set_default_eps_host(d);
    }
    int n_osc;
    if (lexer_integer(l, &n_osc)) return 1;
    d->nb_hos = n_osc;
    d->params = emalloc(n_osc * sizeof(struct ho_params));
    struct ho_params *ho = d->params;
    int i;
    for (i = 0; i < n_osc; i++, ho++) {
        if (lexer_number(l, &ho->nosc)) return 1;
        if (lexer_number(l, &ho->en)) return 1;
        if (lexer_number(l, &ho->eg)) return 1;
        if (lexer_number(l, &ho->nu)) return 1;
        if (lexer_number(l, &ho->phi)) return 1;
    }
    return 0;
}
