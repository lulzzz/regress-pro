#include <nlopt.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_blas.h>

#include "math-utils.h"
#include "nlopt_fit.h"
#include "fit_result.h"
#include "gsl_cpp.h"
#include "cmpl.h"

enum {
    MY_NLOPT_MAXEVAL = 5000,
    MY_NLOPT_EVAL_REFRESH = 100,
    MY_NLOPT_MAX_SECONDS = 8,
};

// To be used as the user data in the nlopt ojective function.
// Hold the resources to evaluate the f and df using the fit_engine's
// multifit fdf function.
struct objective_data {
    fit_engine *fit;
    gsl_vector *f;
    gsl_matrix *J;
    nlopt_opt opt;
    gui_hook_func_t hfun;
    void *hdata;
    int eval_count;

    objective_data(fit_engine *_fit, nlopt_opt _opt, gui_hook_func_t _hfun, void *_hdata):
        fit(_fit), opt(_opt), hfun(_hfun), hdata(_hdata), eval_count(0) {
        f = gsl_vector_alloc(fit->run->mffun.n);
        J = gsl_matrix_alloc(fit->run->mffun.n, fit->run->mffun.p);
    }

    ~objective_data() {
        gsl_vector_free(f);
        gsl_matrix_free(J);
    }

    bool check_for_stop_request() const {
        if (eval_count % MY_NLOPT_EVAL_REFRESH == 0) {
            const float xf = float(eval_count) / float(MY_NLOPT_MAXEVAL);
            return (*hfun)(hdata, xf, nullptr) != 0;
        }
        return false;
    }

    void eval_f(const gsl_vector *x) {
        fit->run->mffun.f(x, (void *) fit, f);
    }

    void eval_df(const gsl_vector *x) {
        fit->run->mffun.df(x, (void *) fit, J);
    }

    int n() const { return fit->run->mffun.n; }
};

static double
objective_func(unsigned n, const double *x, double *grad, void *_data)
{
    objective_data *data = (objective_data *) _data;
    if (data->check_for_stop_request()) {
        nlopt_force_stop(data->opt);
    }
    gsl_vector_const_view xv = gsl_vector_const_view_array(x, n);
    data->eval_f(&xv.vector);
    const double chisq = pow2(gsl_blas_dnrm2(data->f));
    if (grad) {
        data->eval_df(&xv.vector);
        for (int k = 0; k < (int) n; k++) { // Iterate over the fit parameters.
            double der = 0.0;
            for (int i = 0; i < data->n(); i++) { // Iterate over the points.
                der += 2 * gsl_vector_get(data->f, i) * gsl_matrix_get(data->J, i, k);
            }
            grad[k] = der;
        }
    }
    data->eval_count ++;
    return chisq;
}

static void
set_optimizer_bounds(nlopt_opt opt, fit_engine *fit, seeds *seeds, gsl_vector *x)
{
    const int dim = fit->parameters->number;
    double_array8 lower_bounds(dim), upper_bounds(dim);
    for(int j = 0; j < dim; j++) {
        const double xc = fit_engine_get_seed_value(fit, &fit->parameters->values[j], &seeds->values[j]);
        gsl_vector_set(x, j, xc);
    }
    for(int j = 0; j < dim; j++) {
        const double xc = gsl_vector_get(x, j);
        double delta;
        if(seeds->values[j].type == SEED_RANGE) {
            delta = seeds->values[j].delta;
        } else {
            const fit_param_t fp = fit->parameters->values[j];
            delta = fit_engine_estimate_param_grid_step(fit, x, &fp, fabs(xc) > 0.1 ? fabs(xc) / 2 : 0.05);
        }
        fprintf(stderr, "parameter: %d, delta: %g\n", j, delta);
        lower_bounds[j] = xc - delta;
        upper_bounds[j] = xc + delta;
    }
    nlopt_set_lower_bounds(opt, lower_bounds);
    nlopt_set_upper_bounds(opt, upper_bounds);
}

int
nlopt_fit(fit_engine *parent_fit, seeds *seeds, lmfit_result *result, str_ptr analysis, int preserve_init_stack,
          gui_hook_func_t hfun, void *hdata)
{
    const int dim = parent_fit->parameters->number;

    if(hfun) {
        (*hfun)(hdata, 0.0, "Running NLopt search...");
    }

    fit_engine *fit = fit_engine_clone(parent_fit);
    fit_engine_prepare(fit, parent_fit->run->spectr, FIT_KEEP_ACQUISITION|FIT_ENABLE_SUBSAMPLING);

    nlopt_opt opt = nlopt_create(NLOPT_GN_CRS2_LM, dim);
    objective_data data(fit, opt, hfun, hdata);
    gsl::vector x(dim);
    set_optimizer_bounds(opt, fit, seeds, x);
    nlopt_set_min_objective(opt, objective_func, (void *) &data);
    nlopt_set_maxeval(opt, MY_NLOPT_MAXEVAL);
    nlopt_set_stopval(opt, 1e-5);
    nlopt_set_ftol_rel(opt, 1e-5);

    fprintf(stderr, "initial point:");
    for (int i = 0; i < dim; i++) {
        fprintf(stderr, " %g", gsl_vector_get(x, i));
    }
    const double minf0 = objective_func(dim, x.data(), nullptr, &data);
    fprintf(stderr, "\nwith objective function: %g\n", minf0);
    fflush(stderr);

    double chisq;
    int status = GSL_SUCCESS;
    if (nlopt_optimize(opt, x.data(), &chisq) < 0) {
        // TO BE done properly.
        fprintf(stderr, "nlopt failed!\n");
        status = 1;
    }
    fprintf(stderr, "found minimum at:");
    for (int i = 0; i < dim; i++) {
        fprintf(stderr, " %g", gsl_vector_get(x, i));
    }
    fprintf(stderr, "\nwith objective function: %g\n", chisq);
    fflush(stderr);
    fit_engine_disable(fit);
    fit_engine_free(fit);

    if (!preserve_init_stack) {
        /* we take care to commit the results obtained from the fit */
        fit_engine_commit_parameters(parent_fit, x);
        fit_engine_update_disp_info(parent_fit);
    }

    gsl_vector_memcpy(parent_fit->run->results, x);

    result->chisq = chisq;
    result->nb_points = data.n();
    result->nb_iterations = 0;
    result->gsl_status = status;

    return 0;
}
