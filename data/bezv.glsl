#version 130
#extension GL_ARB_explicit_attrib_location : require
#extension GL_ARB_draw_instanced : require
//#extension GL_ARB_uniform_buffer_object : require

const int BATCH_SIZE = 2024;
const int FILL_CONVEX=0, FILL_CONCAVE=1, FILL_SOLID=2, SHOW_FLAGS=3;

uniform int fill_mode = FILL_SOLID;
uniform mat4 the_matrix;
uniform vec4 the_color;

#if 1
uniform vec2 glyph_positions[BATCH_SIZE];
#else
layout(std140) uniform GlyphPositions { vec2 glyph_positions[BATCH_SIZE]; };
#endif

layout(location=0) in vec2 attr_pos;
layout(location=1) in uint attr_flag;
out vec2 tex_coord;
flat out vec4 vs_color;

/*
index bit 0: PT_ON_CURVE
index bit 1: tells apart the 2 on-curve corners
*/
const vec2 texc_table[4] = vec2[4](
	vec2( 0.5, 0.0 ), /* off */
	vec2( 0.0, 0.0 ), /* on #1 */
	vec2( 0.5, 0.0 ), /* off */
	vec2( 1.0, 1.0 ) /* on #2 */
);

const vec4 flag_colors[4] = vec4[4](
	vec4( 1.0, 0.0, 1.0, 1.0 ), /* off: pink */
	vec4( 1.0, 1.0, 0.0, 1.0 ), /* on #1: yellow */
	vec4( 1.0, 0.0, 1.0, 1.0 ), /* off: pink */
	vec4( 0.0, 1.0, 1.0, 1.0 ) /* on #2: cyan */
);

/* this would be so much better but sadly gl_VertexID isn't available with glDrawElements
const vec2 texc_table[3] = vec2[3](
	vec2( 0.0, 0.0 ),
	vec2( 0.5, 0.0 ),
	vec2( 1.0, 1.0 )
);
*/

void main()
{
	gl_Position = the_matrix * vec4( attr_pos + glyph_positions[ gl_InstanceID ], 0.0, 1.0 );
	tex_coord = texc_table[ attr_flag ];
	vs_color = the_color;
	
	if ( fill_mode == SHOW_FLAGS )
		vs_color = flag_colors[ attr_flag ];
}
