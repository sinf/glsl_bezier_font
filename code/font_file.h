#ifndef _FONT_FILE_H
#define _FONT_FILE_H
#include "font_data.h"

/*
enum {
	OK=0,
	CAN_NOT_OPEN_FILE=1,
	FILE_IS_EMPTY=2,
	UNIDENTIFIED_FILE_FORMAT=3,
	UNEXPECTED_EOF=4,
	UNSUPPORTED_FEATURE=5,
	TTC_HAS_NO_FONTS=6,
	MALFORMED_DATA=7,
	MALLOC_FAIL=8,
};
*/

typedef enum {
	F_SUCCESS=0,
	F_FAIL_OPEN, /* failed to open file */
	F_FAIL_EOF, /* unexpected end of file */
	F_FAIL_UNK_FILEF, /* unsupported file format */
	F_FAIL_UNSUP_FEA, /* unsupported font feature */
	F_FAIL_UNSUP_VER, /* unsupported version */
	F_FAIL_CORRUPT, /* malformed data */
	F_FAIL_ALLOC, /* out of memory */
	F_FAIL_INCOMPLETE, /* font lacks required information */
	F_FAIL_IMPOSSIBLE, /* should never happen */
	NUM_FONT_STATUS_CODES
} FontStatus;

/* Returns 0 if success and nonzero if failure */
FontStatus load_truetype( Font font[1], const char filename[] );

#endif
