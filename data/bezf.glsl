#version 130

uniform vec4 the_color;

const int FILL_CONVEX=0, FILL_CONCAVE=1, FILL_SOLID=2;
uniform int fill_mode = FILL_SOLID;

in vec2 tex_coord;
out vec4 frag_color;

void main()
{
	if ( fill_mode == FILL_SOLID )
	{
		frag_color = the_color;
	}
	else
	{
		bool inside_convex = tex_coord.x * tex_coord.x - tex_coord.y < 0;
		float alpha = 0.0;
		
		if ( ( fill_mode == FILL_CONVEX ) == inside_convex )
			alpha = 1.0;
		
		frag_color = vec4( the_color.rgb, the_color.a * alpha );
	}
}
