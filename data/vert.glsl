#version 130

// This is a workaround to shitty drivers that don't support the 'layout' qualifier
//%bindattr v_pos 0
//%bindattr v_nor 1
//%bindattr v_col 2

uniform mat4 mvp;
in vec3 v_pos;
in vec3 v_nor;
in vec4 v_col;
flat out vec3 normal;
out vec4 color;

void main()
{
	gl_Position = mvp * vec4( v_pos, 1.0 );
	normal = v_nor;
	color = v_col;
}
