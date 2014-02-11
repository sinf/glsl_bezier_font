#ifndef _FONT_FILE_H
#define _FONT_FILE_H
#include "font_data.h"

/*
Microsoft's OpenType specification:
	http://www.microsoft.com/typography/otspec/default.htm

Old TrueType 1.66 specification in Word format:
	http://www.microsoft.com/typography/SpecificationsOverview.mspx
	
Apple's TrueType Reference Manual:
	https://developer.apple.com/fonts/ttrefman/index.html
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
	F_FAIL_TRIANGULATE, /* failed to triangulate geometry */
	F_FAIL_BUFFER_LIMIT, /* some statically allocated buffer is too small */
	NUM_FONT_STATUS_CODES
} FontStatus;

/* Returns 0 if success and nonzero if failure */
FontStatus load_truetype( Font font[1], const char filename[] );

#endif
