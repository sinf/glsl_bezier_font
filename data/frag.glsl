#version 130

flat in vec3 normal;
in vec4 color;
out vec4 frag_color;

void main()
{
	const vec3 light_dir = normalize( -vec3( -1.0, -2.0, 3.0 ) );
	const vec3 amb = vec3( 0.25 );
	
	float diff = dot( light_dir, normal ) * 0.5 + 0.5;
	vec3 col = mix( amb, color.rgb, diff );
	frag_color = vec4( col, color.a );
}
