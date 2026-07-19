/* play_maps — encoder/switch bindings for between play mode */

#ifndef BETWEEN_PLAY_MAPS_H
#define BETWEEN_PLAY_MAPS_H

#include "morph2d.h"
#include "param_common.h"
#include "types.h"

#define PLAY_MAPS_ENC_COUNT 4
#define PLAY_MAPS_SW_COUNT 4
#define PLAY_MAPS_FS_COUNT 2
/* panel sw0–3 then footswitches fs0–1 (logical index for play apply). */
#define PLAY_MAPS_SW_TOTAL (PLAY_MAPS_SW_COUNT + PLAY_MAPS_FS_COUNT)

typedef enum {
  ePlayEncNone = 0,
  ePlayEncMorphX,
  ePlayEncMorphY,
  ePlayEncParamSlot,
  ePlayEncParamAll
} PlayEncKind;

typedef enum {
  ePlaySwNone = 0,
  ePlaySwSnapA,
  ePlaySwSnapB,
  ePlaySwSnapC,
  ePlaySwSnapD,
  ePlaySwSetSlot,
  ePlaySwMomSlot,
  ePlaySwSetAll,
  ePlaySwMomAll
} PlaySwKind;

typedef struct {
  PlayEncKind kind;
  MorphSlot slot; /* for ePlayEncParamSlot */
  char label[PARAM_LABEL_LEN];
} PlayEncMap;

typedef struct {
  PlaySwKind kind;
  MorphSlot slot; /* for set/mom single-slot */
  char label[PARAM_LABEL_LEN];
  ParamValue value; /* set/mom raw value */
} PlaySwMap;

typedef struct {
  PlayEncMap enc[PLAY_MAPS_ENC_COUNT];
  PlaySwMap sw[PLAY_MAPS_SW_COUNT];
  PlaySwMap fs[PLAY_MAPS_FS_COUNT]; /* footswitches; same targets as sw */
} PlayMaps;

/* panel 0..3 or footswitch 4..5 → map; NULL if out of range. */
PlaySwMap *play_maps_sw_total_at(PlayMaps *m, u8 idx);
const PlaySwMap *play_maps_sw_total_at_const(const PlayMaps *m, u8 idx);

void play_maps_set_defaults(PlayMaps *m);

/* clear param bindings whose labels are not in desc[0..num_params). */
void play_maps_clear_invalid(PlayMaps *m, const ParamDesc *desc,
			     u16 num_params);

/* parse/format value portion of play.encN / play.swN (no key). */
u8 play_maps_parse_enc(const char *val, PlayEncMap *out);
u8 play_maps_parse_sw(const char *val, PlaySwMap *out);
/* write into buf; returns 0 on failure. */
u8 play_maps_format_enc(char *buf, u32 buf_size, const PlayEncMap *m);
u8 play_maps_format_sw(char *buf, u32 buf_size, const PlaySwMap *m);

/* short UI summary, e.g. "morph.x", "slot.b/amp", "snap.a". */
void play_maps_summary_enc(char *buf, u32 buf_size, const PlayEncMap *m);
void play_maps_summary_sw(char *buf, u32 buf_size, const PlaySwMap *m);

/* footer text for a switch (slot letter, param name, or "-"). */
void play_maps_footer_sw(char *buf, u32 buf_size, const PlaySwMap *m);

/* 1 if switch targets a single slot (for corner triangle). */
u8 play_maps_sw_single_slot(const PlaySwMap *m, MorphSlot *out_slot);

/* snap kind → slot; 1 if kind is snap. */
u8 play_maps_sw_snap_slot(PlaySwKind kind, MorphSlot *out);

/* restore one control to defaults. */
void play_maps_reset_enc(PlayMaps *m, u8 idx);
void play_maps_reset_sw(PlayMaps *m, u8 idx);
void play_maps_reset_fs(PlayMaps *m, u8 idx);

#endif
