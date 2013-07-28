#include <gsl/gsl_blas.h>

#include "batch_engine.h"
#include "fit-params.h"
#include "lmfit.h"
#include "grid-search.h"
#include "Strcpp.h"

bool batch_engine::init(generator<struct spectrum*>& gen)
{
    gen.reset();

    struct spectrum* s_first;
    if(gen.next(s_first) == generator_end) {
        return false;
    }

    m_spectra_kind = fit_engine_batch_prepare(m_fit_engine, s_first);

    m_spectra.dispose_all();

    spectrum_item itf;
    fit_engine_prepare_spectrum(m_fit_engine, s_first);
    itf.assign(s_first, m_fit_engine->parameters->number);
    m_spectra.push_back(itf);

    struct spectrum* s;
    for(generator_e gs = gen.next(s); gs > generator_end; gs = gen.next(s)) {
        if(s->config.system != m_spectra_kind) {
            return false;
        }
        spectrum_item it;
        fit_engine_prepare_spectrum(m_fit_engine, s);
        it.assign(s, m_fit_engine->parameters->number);
        m_spectra.push_back(it);
    }

    return true;
}

void batch_engine::prefit()
{
    for(unsigned k = 0; k < m_spectra.size(); k++) {
        spectrum_item it = m_spectra[k];
        fit_engine_attach_spectrum(m_fit_engine, it.self);

        double chisq;
        Str error_msgs, analysis;
        lmfit_grid(m_fit_engine, m_seeds, &chisq, analysis.str(), error_msgs.str(),
                   LMFIT_PRESERVE_STACK, NULL, NULL);

        gsl_vector_memcpy(it.seeds, m_fit_engine->run->results);
    }
}

void batch_engine::apply_goal_parameters(const gsl_vector *x)
{
    fit_engine_apply_parameters(m_fit_engine, m_gbo_params, x);
}

void batch_engine::fit(gsl_matrix* results, int output_param)
{
    const gsl_multifit_fdfsolver_type *T = gsl_multifit_fdfsolver_lmsder;
    struct fit_engine *fit = m_fit_engine;
    struct fit_config *cfg = fit->config;
    int np = fit->parameters->number;
    gsl_vector *x = gsl_vector_alloc(np);
    gsl_multifit_function_fdf *f = &fit->run->mffun;
    gsl_matrix* covar = gsl_matrix_alloc(f->p, f->p);

    for (unsigned k = 0; k < m_spectra.size(); k++) {
        const spectrum_item& si = m_spectra[k];
        gsl_multifit_fdfsolver *solver;
        int nb_iter;
        fit_engine_attach_spectrum(fit, si.self);
        solver = gsl_multifit_fdfsolver_alloc(T, f->n, f->p);
        gsl_vector_memcpy(x, si.seeds);
        lmfit_iter(x, f, solver, cfg->nb_max_iters, cfg->epsabs, cfg->epsrel, &nb_iter, NULL, NULL, NULL);
        gsl_matrix_set(results, k, 0, gsl_vector_get(x, output_param));
        gsl_multifit_covar(solver->J, 1e-6, covar);
        double cov = gsl_matrix_get(covar, output_param, output_param);
        gsl_matrix_set(results, k, 1, sqrt(cov));
        double chi = gsl_blas_dnrm2(solver->f);
        double dof = f->n - f->p;
        gsl_matrix_set(results, k, 2, chi / sqrt(dof));
        gsl_multifit_fdfsolver_free(solver);
    }
    gsl_matrix_free(covar);
}
