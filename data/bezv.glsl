#version 130
#extension GL_ARB_explicit_attrib_location : require
#extension GL_ARB_draw_instanced : require

uniform vec2 glyph_positions[2024]; // BATCH_SIZE
uniform mat4 the_matrix;

layout(location=0) in vec2 attr_pos;
out vec2 tex_coord;

const vec2 texc_table[3] = vec2[3](
	vec2( 0.0, 0.0 ),
	vec2( 0.5, 0.0 ),
	vec2( 1.0, 1.0 )
);

void main()
{
	gl_Position = the_matrix * vec4( attr_pos + glyph_positions[ gl_InstanceID ], 0.0, 1.0 );
	tex_coord = texc_table[ gl_VertexID % 3 ];
}
