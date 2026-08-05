/* param_scaler.c — bees-compatible, NV tables from Between RAM. */

#include "print_funcs.h"

#include "param_scaler.h"
#include "scaler_tables.h"
#include "types.h"

static u32 scalerDataWords[eParamNumTypes] = {
    0,    /* eParamTypeBool */
    0,    /* eParamTypeFix */
    1024, /* eParamTypeAmp */
    1024, /* eParamTypeIntegrator */
    1024, /* eParamTypeNote */
    1024, /* eParamTypeSvfFreq */
    0,    /* eParamTypeFract */
    0,    /* eParamTypeShort */
    0,    /* eParamTypeIntegratorShort */
    0,    /* eParamTypePatchMatrix */
};

static u32 scalerRepWords[eParamNumTypes] = {
    0, 0, 1024, /* amp */
    0, 0, 0,    0, 0, 0, 0,
};

static const char scalerDataPath[eParamNumTypes][32] = {
    "",
    "",
    "scaler_amp_val.dat",
    "scaler_integrator_val.dat",
    "scaler_note_val.dat",
    "scaler_svf_fc_val.dat",
    "",
    "",
    "",
    "",
};

static const char scalerRepPath[eParamNumTypes][32] = {
    "", "", "scaler_amp_rep.dat", "", "", "", "", "", "", "",
};

/* word offsets into scaler NV blob */
static const u32 scalerDataOffset[eParamNumTypes] = {
    0,    0, 0, /* amp */
    1024,       /* integrator */
    2048,       /* note */
    3072,       /* svf */
    0,    0, 0, 0,
};

static const u32 scalerRepOffset[eParamNumTypes] = {
    0, 0, 4096, /* amp rep */
    0, 0, 0,    0, 0, 0, 0,
};

scaler_init_fn scaler_init_pr[eParamNumTypes] = {
    &scaler_bool_init,       &scaler_fix_init,   &scaler_amp_init,
    &scaler_integrator_init, &scaler_note_init,  &scaler_svf_fc_init,
    &scaler_fract_init,      &scaler_short_init, &scaler_integrator_short_init,
    &scaler_labels_init,
};

scaler_get_value_fn scaler_get_val_pr[eParamNumTypes] = {
    &scaler_bool_val,       &scaler_fix_val,   &scaler_amp_val,
    &scaler_integrator_val, &scaler_note_val,  &scaler_svf_fc_val,
    &scaler_fract_val,      &scaler_short_val, &scaler_integrator_short_val,
    &scaler_labels_val,
};

scaler_get_str_fn scaler_get_str_pr[eParamNumTypes] = {
    &scaler_bool_str,       &scaler_fix_str,   &scaler_amp_str,
    &scaler_integrator_str, &scaler_note_str,  &scaler_svf_fc_str,
    &scaler_fract_str,      &scaler_short_str, &scaler_integrator_short_str,
    &scaler_labels_str,
};

scaler_get_in_fn scaler_get_in_pr[eParamNumTypes] = {
    &scaler_bool_in,       &scaler_fix_in,   &scaler_amp_in,
    &scaler_integrator_in, &scaler_note_in,  &scaler_svf_fc_in,
    &scaler_fract_in,      &scaler_short_in, &scaler_integrator_short_in,
    &scaler_labels_in,
};

scaler_inc_fn scaler_inc_pr[eParamNumTypes] = {
    &scaler_bool_inc,       &scaler_fix_inc,   &scaler_amp_inc,
    &scaler_integrator_inc, &scaler_note_inc,  &scaler_svf_fc_inc,
    &scaler_fract_inc,      &scaler_short_inc, &scaler_integrator_short_inc,
    &scaler_labels_inc,
};

void scaler_init(ParamScaler *sc, const ParamDesc *const desc) {
  sc->desc = (const ParamDesc *)desc;
  if (scaler_init_pr[desc->type] != NULL) {
    (*(scaler_init_pr[desc->type]))(sc);
  }
}

s32 scaler_get_value(ParamScaler *sc, io_t in) {
  scaler_get_value_fn fn = scaler_get_val_pr[sc->desc->type];
  if (fn != NULL) {
    return (*fn)(sc, in);
  }
  return 0;
}

void scaler_get_str(char *dst, ParamScaler *sc, io_t in) {
  scaler_get_str_fn fn = scaler_get_str_pr[sc->desc->type];
  if (fn != NULL) {
    (*fn)(dst, sc, in);
  }
}

io_t scaler_get_in(ParamScaler *sc, s32 value) {
  scaler_get_in_fn fn = scaler_get_in_pr[sc->desc->type];
  if (fn != NULL) {
    return (*fn)(sc, value);
  }
  return 0;
}

s32 scaler_inc(ParamScaler *sc, io_t *pin, io_t inc) {
  scaler_inc_fn fn = scaler_inc_pr[sc->desc->type];
  if (fn != NULL) {
    return (*fn)(sc, pin, inc);
  }
  return 0;
}

u32 scaler_get_data_bytes(ParamType p) { return scalerDataWords[p] * 4; }

u32 scaler_get_rep_bytes(ParamType p) { return scalerRepWords[p] * 4; }

const char *scaler_get_data_path(ParamType p) { return scalerDataPath[p]; }

const char *scaler_get_rep_path(ParamType p) { return scalerRepPath[p]; }

u32 scaler_get_data_offset(ParamType p) { return scalerDataOffset[p] * 4; }

u32 scaler_get_rep_offset(ParamType p) { return scalerRepOffset[p] * 4; }

const s32 *scaler_get_nv_data(ParamType p) {
  return (const s32 *)(scaler_tables_bytes() + scaler_get_data_offset(p));
}

const s32 *scaler_get_nv_rep(ParamType p) {
  return (const s32 *)(scaler_tables_bytes() + scaler_get_rep_offset(p));
}
