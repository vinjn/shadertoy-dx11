// https://www.shadertoy.com/view/Mss3R8
// Created by inigo quilez - iq/2013
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.


// learn more here: // http://www.iquilezles.org/www/articles/distancefractals/distancefractals.htm	

float calc( vec2 p, float time )
{
	p = -1.0 + 2.0*p;
	p.x *= iResolution.x/iResolution.y;

	float ltime = 0.5-0.5*cos(time*0.12);
    float zoom = pow( 0.9, 100.0*ltime );
	float an = 2.0*ltime;
	p = mat2(cos(an),sin(an),-sin(an),cos(an))*p;
	vec2 ce = vec2( 0.2655,0.301 );
	ce += zoom*0.8*cos(4.0+4.0*ltime);
	p = ce + (p-ce)*zoom;
	vec2 c = vec2( -0.745, 0.186 ) - 0.045*zoom*(1.0-ltime);
	
	vec2 z = p;
	vec2 dz = vec2( 1.0, 0.0 );
	float t = 0.0;
#if 0	
	for( int i=0; i<200; i++ )
	{
		dz = 2.0*vec2(z.x*dz.x-z.y*dz.y, z.x*dz.y + z.y*dz.x );
        z = vec2( z.x*z.x - z.y*z.y, 2.0*z.x*z.y ) + c;
		if( dot(z,z)>100.0 ) break;
		t += 1.0;
	}
#else

	for( int i=0; i<200; i++ )
	{
		if( dot(z,z)<100.0 )
		{
		dz = 2.0*vec2(z.x*dz.x-z.y*dz.y, z.x*dz.y + z.y*dz.x );
        z = vec2( z.x*z.x - z.y*z.y, 2.0*z.x*z.y ) + c;
		t += 1.0;
		}
	}
#endif
	
	float d = sqrt( dot(z,z)/dot(dz,dz) )*log(dot(z,z));

	return pow( clamp( (200.0/zoom)*d, 0.0, 1.0 ), 0.5 );
}

const int NumSamples = 8;
	
void main(void)
{
	#if 1
	float scol = calc( gl_FragCoord.xy/iResolution.xy, iGlobalTime );
    #else
	float scol = 0.0;
	float h = 0.0;
	float iSamples = 1.0/float(NumSamples);
	for( int i=0; i<NumSamples; i++ )
	{
		vec2 of = 0.5 + 0.5*vec2( cos(6.3*h), sin(15.0*h) );
	    scol += calc( (gl_FragCoord.xy+of)/iResolution.xy, iGlobalTime - h*0.4/24.0 );
		h += iSamples;
	}
	scol *= iSamples;
	#endif

	
	vec3 vcol = pow( vec3(scol), vec3(1.0,1.1,1.4) );
	
	vcol *= vec3(1.0,0.98,0.95);
	
	vec2 uv = gl_FragCoord.xy/iResolution.xy;
	vcol *= 0.7 + 0.3*pow(16.0*uv.x*uv.y*(1.0-uv.x)*(1.0-uv.y),0.25);

	
	gl_FragColor = vec4( vcol, 1.0 );
}