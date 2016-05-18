#ifndef REFL_KERNEL_H
#define REFL_KERNEL_H

#include <complex>
#include <cmath>

#include "defs.h"

// #include "spectra.h"
#include "ellipsometry-decls.h"

#include <gsl/gsl_vector.h>


/* reflectivity for normal incidence with multi-layer film */
double mult_layer_refl_ni(size_t nb /*nb of mediums */,
                          const std::complex<double> ns[], const double ds[],
                          double lambda,
                          gsl_vector *rjacob_th, gsl_vector *rjacob_n);

#endif
