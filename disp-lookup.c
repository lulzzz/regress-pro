
#include <assert.h>
#include <string.h>
#include "dispers.h"
#include "cmpl.h"

static void     lookup_free      (disp_t *d);
static disp_t * lookup_copy      (const disp_t *d);

static cmpl lookup_n_value       (const disp_t *disp, double lam);
static cmpl lookup_n_value_deriv (const disp_t *disp, double lam,
				  cmpl_vector *der);
static int  lookup_fp_number     (const disp_t *disp);
static int  lookup_decode_param_string (const char *p);
static int  lookup_apply_param   (struct disp_struct *d,
				  const fit_param_t *fp, double val);
static void lookup_encode_param  (str_t param, const fit_param_t *fp);

static double lookup_get_param_value (const struct disp_struct *d,
				      const fit_param_t *fp);

struct disp_class disp_lookup_class = {
  .disp_class_id       = DISP_LOOKUP,
  .model_id            = MODEL_LOOKUP,

  .short_id            = "Lookup",
  .full_id             = "LookupDispersion",

  .free                = lookup_free,
  .copy                = lookup_copy,

  .n_value             = lookup_n_value,
  .fp_number           = lookup_fp_number,
  .n_value_deriv       = lookup_n_value_deriv,
  .apply_param         = lookup_apply_param,
  .get_param_value     = lookup_get_param_value,

  .decode_param_string = lookup_decode_param_string,
  .encode_param        = lookup_encode_param,
};

struct disp_struct *
disp_new_lookup (const char *name, int nb_comps, struct lookup_comp *comp,
		 double p0)
{
  disp_t *d = disp_new_with_name (DISP_LOOKUP, name);

  d->disp.lookup.p = p0;
  d->disp.lookup.nb_comps = nb_comps;
  d->disp.lookup.component = comp;

  return d;
}

void
lookup_free (disp_t *d)
{
  struct disp_lookup *lk = & d->disp.lookup;
  int j;
  for (j = 0; j < lk->nb_comps; j++)
    disp_free (lk->component[j].disp);
  free (lk->component);
  str_free (d->name);
  free (d);
}

disp_t *
lookup_copy (const disp_t *src)
{
  disp_t *cp = disp_base_copy (src);
  struct disp_lookup *lk = & cp->disp.lookup;
  size_t sz = lk->nb_comps * sizeof(struct lookup_comp);
  struct lookup_comp *newcomp = emalloc (sz);
  int j;

  memcpy (newcomp, lk->component, sz);
  for (j = 0; j < lk->nb_comps; j++)
    newcomp[j].disp = disp_copy (newcomp[j].disp);

  lk->component = newcomp;

  return cp;
}

static int
lookup_find_interval (struct disp_lookup const * lookup, double p)
{
  int j;

  assert (lookup->nb_comps >= 2);

  for (j = 1; j < lookup->nb_comps - 1; j++)
    {
      if (lookup->component[j].p >= p)
	break;
    }

  return j-1;
}

cmpl
lookup_n_value (const disp_t *disp, double lam)
{
  struct disp_lookup const * lookup = & disp->disp.lookup;
  int j = lookup_find_interval (lookup, lookup->p);
  cmpl n1 = n_value (lookup->component[j].disp, lam);
  cmpl n2 = n_value (lookup->component[j+1].disp, lam);
  double p1 = lookup->component[j].p;
  double p2 = lookup->component[j+1].p;
  cmpl n = n1 + (n2 - n1) * ((lookup->p - p1) / (p2 - p1));
  return n;
}

cmpl
lookup_n_value_deriv (const disp_t *disp, double lam, cmpl_vector *der)
{
  struct disp_lookup const * lookup = & disp->disp.lookup;
  int j  = lookup_find_interval (lookup, lookup->p);
  cmpl n1 = n_value (lookup->component[j].disp, lam);
  cmpl n2 = n_value (lookup->component[j+1].disp, lam);
  double p1 = lookup->component[j].p;
  double p2 = lookup->component[j+1].p;
  cmpl dn = (n2 - n1) / (p2 - p1);
  cmpl_vector_set (der, 0, dn);
  return n1 + (n2 - n1) * ((lookup->p - p1) / (p2 - p1));
}

int
lookup_fp_number (const disp_t *disp)
{
  return 1;
}

int
lookup_decode_param_string (const char *p)
{
  if (strcmp (p, "P") == 0)
    return 0;
  return -1;
}

int
lookup_apply_param   (struct disp_struct *d,
		      const fit_param_t *fp, double val)
{
  struct disp_lookup *lk = & d->disp.lookup;
  assert (fp->param_nb == 0);
  lk->p = val;
  return 0;
}

double
lookup_get_param_value (const struct disp_struct *d, const fit_param_t *fp)
{
  const struct disp_lookup *lk = & d->disp.lookup;
  assert (fp->param_nb == 0);
  return lk->p;
}

void
lookup_encode_param  (str_t param, const fit_param_t *fp)
{
  str_copy_c (param, "P");
}