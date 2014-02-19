#version 130
#extension GL_ARB_explicit_attrib_location : require
#extension GL_ARB_draw_instanced : require

const int FILL_CURVE=0, FILL_SOLID=2, SHOW_FLAGS=3;
uniform int fill_mode = FILL_SOLID;
uniform mat4 the_matrix;
uniform vec4 the_color;

layout(location=0) in vec2 attr_pos;
layout(location=1) in uint attr_flag;
layout(location=2) in vec2 attr_instance_offset; /* one attribute per instance */

flat out vec4 color_above;
flat out vec4 color_below;
out vec2 tex_coord;

/* Bits of vertex attribute "attr_flag":
bit 0: PT_ON_CURVE
bit 1: tells apart the 2 on-curve corners
bit 2: set if the curve is convex, zero if concave
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

void main()
{
	gl_Position = the_matrix * vec4( attr_pos + attr_instance_offset, 0.0, 1.0 );
	tex_coord = texc_table[ attr_flag & 3u ];
	switch( fill_mode )
	{
		case FILL_CURVE:
			vec4 opaque = the_color;
			vec4 transp = vec4( the_color.rgb, 0.0 );
			if ( attr_flag >> 2 == 0u ) {
				// Curve is concave. Paint below the curve
				color_above = transp;
				color_below = opaque;
			} else {
				// Curve is convex. Paint above the curve
				color_above = opaque;
				color_below = transp;
			}
			break;
		case SHOW_FLAGS:
			color_above = color_below = flag_colors[ attr_flag & 3u ];
			break;
		default:
		case FILL_SOLID:
			color_above = color_below = the_color;
			break;
	}
}
