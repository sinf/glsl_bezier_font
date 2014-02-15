#ifndef _MATRIX_H
#define _MATRIX_H
#include <stddef.h>

#define PI 3.14159265358979323846
#define HALF_PI 1.57079632679489661923

#define RADIANS(deg) (deg * ( 180.0f / PI ))
#define DEGREES(rad) (rad * ( PI / 180.0f ))

typedef float Mat4[16];
typedef float Vec4[4];

void vec3_addmul( float out[3], float const a[3], float const b[3], float b_scale );
void vec3_sub( float out[3], float const a[3], float const b[3] );
float vec3_dot( const float a[3], const float b[3] );
float vec3_len( const float a[3] );
void vec3_cross( float s[3], const float u[3], const float v[3] );
void vec3_normalize1( float a[3] );
void vec3_normalize( float a[][3], size_t num_vectors );
float vec3_dist( float const a[3], float const b[3] );

float vec4_dot( const Vec4 a, const Vec4 b );
float vec4_len( const Vec4 a );
void vec4_normalize( float a[], size_t num_vectors );
void vec4_add( Vec4 dst[], const Vec4 delta, size_t count );

void mat4_transpose( Mat4 out, const Mat4 in );
void mat4_mult( Mat4 out, const Mat4 ma, const Mat4 mb );
void mat4_get_col( Vec4 out, const Mat4 in, unsigned char x ); /* 0 <= x <= 3 */
void mat4_mult_vec( Vec4 out, const Mat4 m, const Vec4 v );

void mat4_frustum( Mat4 m, float l, float r, float t, float b, float n, float f );
void mat4_persp( Mat4 m, float fovy, float aspect, float z_near, float z_far );

void mat4_translation( Mat4 m, float x, float y, float z );
void mat4_scaling( Mat4 m, float x, float y, float z );
void mat4_rotation_x( Mat4 m, float a );
void mat4_rotation_y( Mat4 m, float a );
void mat4_rotation_z( Mat4 m, float a );

extern const Mat4 MAT4_IDENTITY;

#endif
