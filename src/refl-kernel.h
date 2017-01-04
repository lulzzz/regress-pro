#ifndef REFL_KERNEL_H
#define REFL_KERNEL_H

#include "common.h"
#include "cmpl.h"
#include "spectra.h"
#include "ellipsometry-decls.h"

#include <gsl/gsl_vector.h>


/* reflectivity for normal incidence with multi-layer film */
double mult_layer_refl_ni(size_t nb /*nb of mediums */,
                          const cmpl ns[], const double ds[],
                          double lambda,
                          gsl_vector *rjacob_th, gsl_vector *rjacob_n);

extern double
mult_layer_refl_ni_bandwidth(size_t _nb, const cmpl ns[], const double ds[],
                             double lambda, const double bandwidth,
                             gsl_vector *r_jacob_th, gsl_vector *r_jacob_n);

#endif
