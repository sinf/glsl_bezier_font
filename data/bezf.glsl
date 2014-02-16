#version 130

flat in vec4 color_above; /* color above the curve y=x² */
flat in vec4 color_below; /* color below the curve y=x² */
in vec2 tex_coord;
out vec4 frag_color;

// Computes the intersection of 2 areas A and B, where
// A = area between X-axis and curve y=x²
// B = area of the given rectangle
float get_coverage( float x1, float y1, float x2, float y2 )
{
	// c is the position where the rectangle is split into a curved part and a box
	// c = intersection of y=y2 and y=x² = +-sqrt(y2)
	// F(x) = integral of f(x) = (1/3)(x³)
	// overlapping area = (F(c)-F(x1)) - (c-x1)y1 + (x2-c)h
	
	float w,h, c, a1, a2, a3;
	
	w = x2 - x1; // rajauslaatikon leveys
	h = y2 - y1; // rajauslaatikon korkeus
	
	c = clamp( sqrt( abs(y2) ), x1, x2 );
	a1 = ( x2 - c ) * h; // palkki oikealla (c:n oikealla puolella)
	a2 = ( c*c*c - x1*x1*x1 ) / 3.0; // kaareva osa (c:n vasemmalla puolella)
	a3 = ( c - x1 ) * y1; // a1 ja a2 alapuolella oleva pylväs
	
	return ( a1 + a2 - a3 ) / ( w * h );
}

void main()
{
	if ( all( equal( color_above, color_below ) ) )
	{
		/* Drawing solid triangles or points */
		frag_color = color_above;
	}
	else
	{
		float u, v, x1, y1, x2, y2, alpha;
		
		u = fwidth( tex_coord.x ) * 0.5;
		v = fwidth( tex_coord.y ) * 0.5;
		
		/* The sampling box */
		x1 = tex_coord.x - u;
		x2 = tex_coord.x + u;
		y1 = tex_coord.y - v;
		y2 = tex_coord.y + v;
		
		/* Integrate the area that falls inside this box */
		alpha = get_coverage( x1, y1, x2, y2 );
		
		frag_color = mix( color_above, color_below, alpha );
	}
}
