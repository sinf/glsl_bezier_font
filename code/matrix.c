#include <math.h>
#include "matrix.h"

const Mat4 MAT4_IDENTITY = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1
};

void vec3_addmul( float out[3], float const a[3], float const b[3], float bs )
{
	out[0] = a[0] + bs * b[0];
	out[0] = a[1] + bs * b[1];
	out[0] = a[2] + bs * b[2];
}

void vec3_sub( float out[3], float const a[3], float const b[3] )
{
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
}

float vec3_dot( const float a[3], const float b[3] ) {
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}


float vec3_len( const float a[3] ) {
	return sqrt( a[0] * a[0] + a[1] * a[1] + a[2] * a[2] );
}

void vec3_normalize1( float a[3] )
{
	float k = vec3_len( a );
	a[0] /= k;
	a[1] /= k;
	a[2] /= k;
}

void vec3_normalize( float a[][3], size_t num_vectors )
{
	size_t n;
	for( n=0; n<num_vectors; n++ )
		vec3_normalize1( a[n] );
}

void vec3_cross( float s[3], const float u[3], const float v[3] )
{
	s[0] = u[1] * v[2] - u[2] * v[1];
	s[1] = u[2] * v[0] - u[0] * v[2];
	s[2] = u[0] * v[1] - u[1] * v[0];
}

float vec3_dist( float const a[3], float const b[3] )
{
	float dx = b[0] - a[0];
	float dy = b[1] - a[1];
	float dz = b[2] - a[2];
	return sqrt( dx*dx + dy*dy + dz*dz );
}

float vec4_dot( const Vec4 a, const Vec4 b ) {
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
}


float vec4_len( const Vec4 a ) {
	return sqrt( a[0] * a[0] + a[1] * a[1] + a[2] * a[2] + a[3] * a[3] );
}


void vec4_normalize( float a[], size_t num_vectors )
{
	size_t n;
	for( n=0; n<num_vectors; n++ )
	{
		float k = 1.0f / vec4_len( a+4*n );
		a[4*n+0] *= k;
		a[4*n+1] *= k;
		a[4*n+2] *= k;
		a[4*n+3] *= k;
	}
}

void vec4_add( Vec4 dst[], float const delta[4], size_t count )
{
	float dx = delta[0];
	float dy = delta[1];
	float dz = delta[2];
	float dw = delta[3];
	size_t n;
	for( n=0; n<count; n++ )
	{
		dst[n][0] += dx;
		dst[n][1] += dy;
		dst[n][2] += dz;
		dst[n][3] += dw;
	}
}

void mat4_transpose( Mat4 out, const Mat4 in )
{
	out[0] = in[0];
	out[1] = in[4];
	out[2] = in[8];
	out[3] = in[12];
	
	out[4] = in[1];
	out[5] = in[5];
	out[6] = in[9];
	out[7] = in[13];
	
	out[8] = in[2];
	out[9] = in[6];
	out[10] = in[10];
	out[11] = in[14];
	
	out[12] = in[3];
	out[13] = in[7];
	out[14] = in[11];
	out[15] = in[15];
}


void mat4_mult( Mat4 out, const Mat4 ma, const Mat4 mb )
{
	int c;
	for( c=0; c<4; c++ )
	{
		int d;
		for( d=0; d<4; d++ )
		{
			float temp = 0;
			int e;
			
			for( e=0; e<4; e++ )
				temp += ma[ c + e * 4 ] * mb[ e + d * 4 ];
			
			out[ c + d * 4 ] = temp;
		}
	}
}


void mat4_get_col( Vec4 out, const Mat4 in, unsigned char x )
{
	/* assert( x >= 0 && x <= 3 ); */
	out[0] = in[x];
	out[1] = in[4+x];
	out[2] = in[8+x];
	out[3] = in[12+x];
}

void mat4_mult_vec( Vec4 out, const Mat4 m, const Vec4 v )
{
	int n;
	for( n=0; n<4; n++ )
		out[n] = vec4_dot( m+4*n, v );
}

/* makes no sense because mat4_transp would often be unnecessary overhead
void mat4_mult_vec( float out[4], const Mat4 m, const float v[4] )
{
	float t[16];
	mat4_transp( t, m );
	out[0] = vec4_dot( t, v );
	out[1] = vec4_dot( t, v+4 );
	out[2] = vec4_dot( t, v+8 );
	out[3] = vec4_dot( t, v+12 );
}
Vec4!T opMul()( Vec4!T a )
{
	return Vec4!T(
		a.dot( vcol( 0 ) ),
		a.dot( vcol( 1 ) ),
		a.dot( vcol( 2 ) ),
		a.dot( vcol( 3 ) )
	);
}
*/


void mat4_frustum( Mat4 ma, float l, float r, float t, float b, float n, float f )
{
	ma[0*4+0] = 2 * n / (r - l);
	ma[0*4+1] = 0;
	ma[0*4+2] = 0;
	ma[0*4+3] = 0;
	ma[1*4+0] = 0;
	ma[1*4+1] = 2 * n / (t - b);
	ma[1*4+2] = 0;
	ma[1*4+3] = 0;
	ma[2*4+0] = (r + l) / (r - l);
	ma[2*4+1] = (t + b) / (t - b);
	ma[2*4+2] = -(f + n) / (f - n);
	ma[2*4+3] = -1;
	ma[3*4+0] = 0;
	ma[3*4+1] = 0;
	ma[3*4+2] = -2 * f * n / (f - n);
	ma[3*4+3] = 0;
}


void mat4_persp( Mat4 m, float fovy, float aspect, float z_near, float z_far )
{
	float scale = tan( fovy / 2 ) * z_near;
	float r = aspect * scale, l = -r;
	float t = scale, b = -t;
	mat4_frustum( m, l, r, b, t, z_near, z_far );
}


void mat4_translation( Mat4 m, float x, float y, float z )
{
	m[0]=1; m[1]=0; m[2]=0; m[3]=0;
	m[4]=0; m[5]=1; m[6]=0; m[7]=0;
	m[8]=0; m[9]=0; m[10]=1; m[11]=0;
	m[12]=x; m[13]=y; m[14]=z; m[15]=1;
}


void mat4_scaling( Mat4 m, float x, float y, float z )
{
	m[0]=x; m[1]=0; m[2]=0; m[3]=0;
	m[4]=0; m[5]=y; m[6]=0; m[7]=0;
	m[8]=0; m[9]=0; m[10]=z; m[11]=0;
	m[12]=0; m[13]=0; m[14]=0; m[15]=1;
}


void mat4_rotation_x( Mat4 m, float a )
{
	float c = cos( a );
	float s = sin( a );
	m[0]=1; m[1]=0; m[2]=0; m[3]=0;
	m[4]=0; m[5]=c; m[6]=s; m[7]=0;
	m[8]=0; m[9]=-s; m[10]=c; m[11]=0;
	m[12]=m[13]=m[14]=0; m[15]=1;
}


void mat4_rotation_y( Mat4 m, float a )
{
	float c = cos( a );
	float s = sin( a );
	m[0]=c; m[1]=0; m[2]=-s; m[3]=0;
	m[4]=0; m[5]=1; m[6]=0; m[7]=0;
	m[8]=s; m[9]=0; m[10]=c; m[11]=0;
	m[12]=m[13]=m[14]=0;
	m[15]=1;
}


void mat4_rotation_z( Mat4 m, float a )
{
	float c = cos( a );
	float s = sin( a );
	m[0]=c; m[1]=s; m[2]=0; m[3]=0;
	m[4]=-s; m[5]=c; m[6]=0; m[7]=0;
	m[8]=0; m[9]=0; m[10]=1; m[11]=0;
	m[12]=m[13]=m[14]=0; m[15]=1;
}
