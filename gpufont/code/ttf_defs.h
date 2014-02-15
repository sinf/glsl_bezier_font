#ifndef _OPENTYPE_DEFS_H
#define _OPENTYPE_DEFS_H
#include <stdint.h>
#pragma pack(push)
#pragma pack(1)

/*
Some OpenType / Truetype data structures that appear in TTF files.
Only used internally by ttf_file.c and triangulate.c
*/

typedef struct {
	uint32_t sfnt_version;
	uint16_t
		num_tables,
		search_range,
		entry_selector,
		range_shift;
} OffsetTable;

typedef struct {
	int32_t version;
	int32_t font_rev;
	uint32_t checksum_adj;
	uint32_t magic; /* 0x5F0F3CF5 */
	uint16_t flags;
	uint16_t units_per_em;
	uint64_t time_created;
	uint64_t time_modified;
	int16_t xmin, ymin, xmax, ymax; /* common bounding box that can encapsulate any glyph */
	uint16_t mac_style; /* bold, italic, etc... */
	uint16_t lowest_good_ppem;
	int16_t font_direction_hint; /* 2 */
	int16_t index_to_loc_format; /* 0=16bit, 1=32bit */
	int16_t glyph_data_format; /* 0 */
} HeadTable;

/* Maximum Profile (maxp) table version 0.5 */
typedef struct {
	uint32_t version; /* 0x5000 */
	uint16_t num_glyphs;
} MaxProTable;

/* Maximum Profile (maxp) table version 1.0 */
typedef struct {
	uint32_t version; /* 0x10000 */
	uint16_t num_glyphs;
	uint16_t max_points;
	uint16_t max_contours;
	uint16_t max_com_points;
	uint16_t max_com_contours;
	uint16_t max_zones;
	uint16_t max_twilight_points;
	uint16_t max_storage;
	uint16_t max_fun_defs;
	uint16_t max_instr_defs;
	uint16_t max_stack_elems;
	uint16_t max_size_of_instr;
	uint16_t max_com_elems;
	uint16_t max_com_recursion;
} MaxProTableOne;

typedef struct {
	uint32_t version; /* 0x10000 */
	int16_t ascender;
	int16_t descender;
	int16_t linegap;
	uint16_t adv_width_max;
	int16_t min_lsb;
	int16_t min_rsb;
	int16_t max_x_extent;
	int16_t caret_slope_rise;
	int16_t caret_slope_run;
	int16_t caret_offset;
	int16_t junk[4];
	int16_t metric_data_format; /* 0 */
	uint16_t num_hmetrics;
} HorzHeaderTable;

typedef struct {
	uint16_t flags;
	uint16_t glyph_index;
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
