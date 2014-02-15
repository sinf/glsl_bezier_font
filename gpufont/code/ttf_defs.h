#ifndef _OPENTYPE_DEFS_H
#define _OPENTYPE_DEFS_H
#include "gpufont_data.h"
#pragma pack(push)
#pragma pack(1)

/*
Some OpenType / Truetype data structures that appear in TTF files.
Only used internally by ttf_file.c and triangulate.c
*/

typedef struct {
	uint32 sfnt_version;
	uint16
		num_tables,
		search_range,
		entry_selector,
		range_shift;
} OffsetTable;

typedef struct {
	int32 version;
	int32 font_rev;
	uint32 checksum_adj;
	uint32 magic; /* 0x5F0F3CF5 */
	uint16 flags;
	uint16 units_per_em;
	uint64 time_created;
	uint64 time_modified;
	int16 xmin, ymin, xmax, ymax; /* common bounding box that can encapsulate any glyph */
	uint16 mac_style; /* bold, italic, etc... */
	uint16 lowest_good_ppem;
	int16 font_direction_hint; /* 2 */
	int16 index_to_loc_format; /* 0=16bit, 1=32bit */
	int16 glyph_data_format; /* 0 */
} HeadTable;

/* Maximum Profile (maxp) table version 0.5 */
typedef struct {
	uint32 version; /* 0x5000 */
	uint16 num_glyphs;
} MaxProTable;

/* Maximum Profile (maxp) table version 1.0 */
typedef struct {
	uint32 version; /* 0x10000 */
	uint16 num_glyphs;
	uint16 max_points;
	uint16 max_contours;
	uint16 max_com_points;
	uint16 max_com_contours;
	uint16 max_zones;
	uint16 max_twilight_points;
	uint16 max_storage;
	uint16 max_fun_defs;
	uint16 max_instr_defs;
	uint16 max_stack_elems;
	uint16 max_size_of_instr;
	uint16 max_com_elems;
	uint16 max_com_recursion;
} MaxProTableOne;

typedef struct {
	uint32 version; /* 0x10000 */
	int16 ascender;
	int16 descender;
	int16 linegap;
	uint16 adv_width_max;
	int16 min_lsb;
	int16 min_rsb;
	int16 max_x_extent;
	int16 caret_slope_rise;
	int16 caret_slope_run;
	int16 caret_offset;
	int16 junk[4];
	int16 metric_data_format; /* 0 */
	uint16 num_hmetrics;
} HorzHeaderTable;

typedef struct {
	uint16 flags;
	uint16 glyph_index;
} SubGlyphHeader;

/* Point flags (used in the glyf table) */
enum {
	PT_ON_CURVE=1,
	PT_SHORT_X=2,
	PT_SHORT_Y=4,
	PT_SAME_FLAGS=8,
	PT_SAME_X=16,
	PT_SAME_Y=32
};

/* Composite glyph flags */
enum {
	COM_ARGS_ARE_WORDS=0x1,
	COM_ARGS_ARE_XY_VALUES=0x2,
	COM_ROUND_XY_TO_GRID=0x4,
	COM_HAVE_A_SCALE=0x8,
	COM_MORE_COMPONENTS=0x20,
	COM_HAVE_X_AND_Y_SCALE=0x40,
	COM_HAVE_MATRIX=0x80,
	COM_HAVE_INSTRUCTIONS=0x100,
	COM_USE_MY_METRICS=0x200,
	COM_OVERLAP=0x400,
	COM_SCALED_OFFSET=0x800,
	COM_UNSCALED_OFFSET=0x1000
};

#pragma pack(pop)
#endif
