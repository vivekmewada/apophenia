/** \file apop_update.c The \c apop_update function.
The header is in asst.h.

Copyright (c) 2006--2009 by Ben Klemens.  Licensed under the modified GNU GPL v2; see COPYING and COPYING2.  */

#include "asst.h"
#include "model.h"
#include "stats.h"
#include "variadic.h"
#include "settings.h"
#include "conversions.h"

static void write_double(const double *draw, apop_data *d){
  int i;
  int size = (d->vector ? d->vector->size : 0) + (d->matrix ? d->matrix->size1 * d->matrix->size2 : 0);
  gsl_vector *v = gsl_vector_alloc(size);
    for (i=0; i< v->size; i++)
        gsl_vector_set(v, i, draw[i]);
    apop_data_unpack(v, d);
    gsl_vector_free(v);
}

static apop_model *check_conjugacy(apop_data *data, apop_model prior, apop_model likelihood){
  apop_model   *outp;
    if (!strcmp(prior.name, "Gamma distribution") && !strcmp(likelihood.name, "Exponential distribution")){
        outp = apop_model_copy(prior);
        apop_vector_increment(outp->parameters->vector, 0, data->matrix->size1*data->matrix->size2);
        apop_data_set(outp->parameters, 1, -1, 1/(1/apop_data_get(outp->parameters, 1, -1) + apop_matrix_sum(data->matrix)));
        return outp;
    }
    if (!strcmp(prior.name, "Beta distribution") && !strcmp(likelihood.name, "Binomial distribution")){
        outp = apop_model_copy(prior);
        if (!data && likelihood.parameters){
            double  n   = likelihood.parameters->vector->data[0];
            double  p   = likelihood.parameters->vector->data[1];
            apop_vector_increment(outp->parameters->vector, 0, n*p);
            apop_vector_increment(outp->parameters->vector, 1, n*(1-p));
        } else {
            double  y   = apop_matrix_sum(data->matrix);
            apop_vector_increment(outp->parameters->vector, 0, y);
            apop_vector_increment(outp->parameters->vector, 1, data->matrix->size1*data->matrix->size2 - y);
        }
        return outp;
    }
    if (!strcmp(prior.name, "Beta distribution") && !strcmp(likelihood.name, "Bernoulli distribution")){
        outp = apop_model_copy(prior);
        double  sum     = 0;
        int     i, j, n = (data->matrix->size1*data->matrix->size2);
        for (i=0; i< data->matrix->size1; i++)
            for (j=0; j< data->matrix->size2; j++)
                if (gsl_matrix_get(data->matrix, i, j))
                    sum++;
        apop_vector_increment(outp->parameters->vector, 0, sum);
        apop_vector_increment(outp->parameters->vector, 1, n - sum);
        return outp;
    }

    /*
output \f$(\mu, \sigma) = (\frac{\mu_0}{\sigma_0^2} + \frac{\sum_{i=1}^n x_i}{\sigma^2})/(\frac{1}{\sigma_0^2} + \frac{n}{\sigma^2}), (\frac{1}{\sigma_0^2} + \frac{n}{\sigma^2})^{-1}\f$

That is, the output is weighted by the number of data points for the
likelihood. If you give me a parametrized normal, with no data, then I'll take the weight to be \f$n=1\f$. 

*/
    if (!strcmp(prior.name, "Normal distribution") && !strcmp(likelihood.name, "Normal distribution")){
        double mu_like, var_like;
        long int n;
        outp = apop_model_copy(prior);
        long double  mu_pri    = prior.parameters->vector->data[0];
        long double  var_pri = gsl_pow_2(prior.parameters->vector->data[1]);
        if (!data && likelihood.parameters){
            mu_like  = likelihood.parameters->vector->data[0];
            var_like = gsl_pow_2(likelihood.parameters->vector->data[1]);
            n        = 1;
        } else {
            n = data->matrix->size1 * data->matrix->size2;
            apop_matrix_mean_and_var(data->matrix, &mu_like, &var_like);
        }
        gsl_vector_set(outp->parameters->vector, 0, (mu_pri/var_pri + n*mu_like/var_like)/(1/var_pri + n/var_like));
        gsl_vector_set(outp->parameters->vector, 1, pow((1/var_pri + n/var_like), -.5));
        return outp;
    }
    return NULL;
}

/** Allocate an \ref apop_update_settings struct.  */
apop_update_settings *apop_update_settings_init(apop_update_settings in){
   apop_update_settings *out = malloc(sizeof(apop_update_settings));
   Apop_assert(out, NULL, 0, 's', "malloc failed. You are probably out of memory.");
   apop_varad_setting(in, out, periods, 6e3);
   apop_varad_setting(in, out, histosegments, 5e2);
   apop_varad_setting(in, out, burnin, 0.05);
   apop_varad_setting(in, out, method, 'd'); //default
   return out;
}

/** Allocate an \ref apop_update_settings struct. See also \ref apop_update_settings_init, which is not deprecated.  */
apop_update_settings *apop_update_settings_alloc(apop_data *d){
    return apop_update_settings_init((apop_update_settings){ }); }

/** Take in a prior and likelihood distribution, and output a posterior
 distribution.

This function first checks a table of conjugate distributions for the pair
you sent in. If the names match the table, then the function returns a
closed-form model with updated parameters.  If the parameters aren't in
the table of conjugate priors/likelihoods, then it uses Markov Chain Monte
Carlo to sample from the posterior distribution, and then outputs a histogram
model for further analysis. Notably, the histogram can be used as
the input to this function, so you can chain Bayesian updating procedures.

To change the default settings (MCMC starting point, periods, burnin...),
add an \ref apop_update_settings struct to the prior.

Here are the conjugate distributions currently defined:

<table>
<tr>
<td> Prior <td></td> Likelihood  <td></td>  Notes 
</td> </tr> <tr>
<td> \ref apop_beta "Beta" <td></td> \ref apop_binomial "Binomial"  <td></td>  
</td> </tr> <tr>
<td> \ref apop_beta "Beta" <td></td> \ref apop_bernoulli "Bernoulli"  <td></td> 
</td> </tr> <tr>
<td> \ref apop_exponential "Exponential" <td></td> \ref apop_gamma "Gamma"  <td></td>  Gamma likelihood represents the distribution of \f$\lambda^{-1}\f$, not plain \f$\lambda\f$
</td> </tr> <tr>
<td> \ref apop_normal "Normal" <td></td> \ref apop_normal "Normal" <td></td>  Assumes prior with fixed \f$\sigma\f$; updates distribution for \f$\mu\f$
</td></tr>
</table>

\param data     The input data, that will be used by the likelihood function (default = \c NULL.)
\param  prior   The prior \ref apop_model (No default, must not be \c NULL.)
\param likelihood The likelihood \ref apop_model. If the system needs to
estimate the posterior via MCMC, this needs to have a \c draw method. (No default, must not be \c NULL.)
\param rng      A \c gsl_rng, already initialized (e.g., via \ref apop_rng_alloc). (default: see \ref autorng)
\return an \ref apop_model struct representing the posterior, with updated parameters. 
\todo The table of conjugate prior/posteriors (in its static \c check_conjugacy subfuction), is a little short, and can always be longer.

*/
APOP_VAR_HEAD apop_model * apop_update(apop_data *data, apop_model *prior, apop_model *likelihood, gsl_rng *rng){
    static gsl_rng *spare_rng = NULL;
    apop_data *apop_varad_var(data, NULL);
    apop_model *apop_varad_var(prior, NULL);
    apop_model *apop_varad_var(likelihood, NULL);
    gsl_rng *apop_varad_var(rng, NULL);
    if (!rng){
        if (!spare_rng) spare_rng = apop_rng_alloc(++apop_opts.rng_seed);
        rng = spare_rng;
    }
    return apop_update_base(data, prior, likelihood, rng);
APOP_VAR_END_HEAD
  apop_model *maybe_out = check_conjugacy(data, *prior, *likelihood);
    if (maybe_out) return maybe_out;
    apop_update_settings *s = apop_settings_get_group(prior, "apop_update");
    if (!s) {
        Apop_settings_add_group(prior, apop_update, data);
        s = apop_settings_get_group(prior, "apop_update");
    }
  double        ratio, ll, cp_ll = GSL_NEGINF;
  int           vs  = likelihood->vbase  >= 0 ? likelihood->vbase  : data->matrix->size2;
  int           ms1 = likelihood->m1base >= 0 ? likelihood->m1base : data->matrix->size2;
  int           ms2 = likelihood->m2base >= 0 ? likelihood->m2base : data->matrix->size2;
  double        *draw               = malloc(sizeof(double)* (vs+ms1*ms2));
  apop_data     *current_param      = apop_data_alloc(vs , ms1 , ms2);
  apop_data     *out                = apop_data_alloc(0, s->periods*(1-s->burnin), vs+ms1*ms2);
    if (s->starting_pt)
        apop_data_memcpy(current_param, s->starting_pt);
    else {
        if (current_param->vector) gsl_vector_set_all(current_param->vector, 1);
        if (current_param->matrix) gsl_matrix_set_all(current_param->matrix, 1);
    }
    if (!likelihood->parameters)
        likelihood->parameters = apop_data_alloc(vs, ms1, ms2);
    for (int i=0; i< s->periods; i++){     //main loop
        apop_draw(draw, rng, prior);
        write_double(draw, likelihood->parameters);
        ll    = apop_log_likelihood(data,likelihood);
        ratio = ll - cp_ll;
        apop_assert(!gsl_isnan(ratio),  NULL, 0, 'c',"Trouble evaluating the likelihood function at vector "
                                        "beginning with %g or %g. Maybe offer a new starting point.\n"
                                        , current_param->vector->data[0], likelihood->parameters->vector->data[0]);
        if (ratio >= 0 || log(gsl_rng_uniform(rng)) < ratio){
            apop_data_memcpy(current_param, likelihood->parameters);
            cp_ll = ll;
        }
        if (i >= s->periods * s->burnin){
            APOP_ROW(out, i-(s->periods *s->burnin), v)
            gsl_vector *vv = apop_data_pack(current_param);
            gsl_vector_memcpy(v, vv);
            gsl_vector_free(vv);
        }
    }
    apop_model *outp   = apop_model_copy(apop_histogram);
    Apop_settings_add_group(outp, apop_histogram, out, s->histosegments);
    apop_histogram_normalize(outp);
    apop_data_free(out); free(draw);
    return outp;
}
