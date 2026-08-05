/* param_scaler.h
   bees-compatible parameter scaling for Between.
*/

#ifndef _ALEPH_AVR32_BEES_SCALER_H_
#define _ALEPH_AVR32_BEES_SCALER_H_

#include "op_math.h"
#include "param_common.h"
#include "util.h"

#include "scaler_amp.h"
#include "scaler_bool.h"
#include "scaler_fix.h"
#include "scaler_fract.h"
#include "scaler_integrator.h"
#include "scaler_integrator_short.h"
#include "scaler_label.h"
#include "scaler_note.h"
#include "scaler_short.h"
#include "scaler_svf_fc.h"

EXTERN_C_BEGIN

#define PARAM_SCALER_DATA_SIZE                                                 \
  (PARAM_SCALER_AMP_DATA_SIZE + PARAM_SCALER_BOOL_DATA_SIZE +                  \
   PARAM_SCALER_FIX_DATA_SIZE + PARAM_SCALER_FRACT_DATA_SIZE +                 \
   PARAM_SCALER_INTEGRATOR_DATA_SIZE +                                         \
   PARAM_SCALER_INTEGRATOR_SHORT_DATA_SIZE + PARAM_SCALER_NOTE_DATA_SIZE +     \
   PARAM_SCALER_SHORT_DATA_SIZE + PARAM_SCALER_SVF_FC_DATA_SIZE)

typedef s32 (*scaler_get_value_fn)(void *scaler, io_t in);
typedef void (*scaler_get_str_fn)(char *dst, void *scaler, io_t in);
typedef io_t (*scaler_get_in_fn)(void *scaler, s32 value);
typedef s32 (*scaler_tune_fn)(void *scaler, u8 tuneId, io_t in);
typedef s32 (*scaler_inc_fn)(void *scaler, io_t *pin, io_t inc);

typedef struct _paramScaler {
  const ParamDesc *desc;
  io_t inMin;
  io_t inMax;
} ParamScaler;

typedef void (*scaler_init_fn)(void *scaler);

extern void scaler_init(ParamScaler *sc, const ParamDesc *desc);
extern s32 scaler_get_value(ParamScaler *sc, io_t in);
extern void scaler_get_str(char *dst, ParamScaler *sc, io_t in);
extern io_t scaler_get_in(ParamScaler *sc, s32 value);
extern s32 scaler_inc(ParamScaler *sc, io_t *pin, io_t inc);

extern u32 scaler_get_data_bytes(ParamType p);
extern u32 scaler_get_rep_bytes(ParamType p);
extern const char *scaler_get_data_path(ParamType p);
extern const char *scaler_get_rep_path(ParamType p);
extern u32 scaler_get_data_offset(ParamType p);
extern u32 scaler_get_rep_offset(ParamType p);
extern const s32 *scaler_get_nv_data(ParamType p);
extern const s32 *scaler_get_nv_rep(ParamType p);

EXTERN_C_END
#endif
