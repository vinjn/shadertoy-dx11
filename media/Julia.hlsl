// https://www.shadertoy.com/view/Mss3R8
// Created by inigo quilez - iq/2013
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.


// learn more here: // http://www.iquilezles.org/www/articles/distancefractals/distancefractals.htm	

float calc( float2 p, float time )
{
	p = -1.0 + 2.0*p;
	p.x *= resolution.x/resolution.y;

	float ltime = 0.5-0.5*cos(time*0.12);
    float zoom = pow( 0.9, 100.0*ltime );
	float an = 2.0*ltime;
	p = mul(p, float2x2(cos(an),sin(an),-sin(an),cos(an)));
	float2 ce = float2( 0.2655,0.301 );
	ce += zoom*0.8*cos(4.0+4.0*ltime);
	p = ce + (p-ce)*zoom;
	float2 c = float2( -0.745, 0.186 ) - 0.045*zoom*(1.0-ltime);
	
	float2 z = p;
	float2 dz = float2( 1.0, 0.0 );
	float t = 0.0;
#if 0	
	for( int i=0; i<200; i++ )
	{
		dz = 2.0*float2(z.x*dz.x-z.y*dz.y, z.x*dz.y + z.y*dz.x );
        z = float2( z.x*z.x - z.y*z.y, 2.0*z.x*z.y ) + c;
		if( dot(z,z)>100.0 ) break;
		t += 1.0;
	}
#else

	for( int i=0; i<200; i++ )
	{
		if( dot(z,z)<100.0 )
		{
		dz = 2.0*float2(z.x*dz.x-z.y*dz.y, z.x*dz.y + z.y*dz.x );
        z = float2( z.x*z.x - z.y*z.y, 2.0*z.x*z.y ) + c;
		t += 1.0;
		}
	}
#endif
	
	float d = sqrt( dot(z,z)/dot(dz,dz) )*log(dot(z,z));

	return pow( clamp( (200.0/zoom)*d, 0.0, 1.0 ), 0.5 );
}

static const int NumSamples = 8;
	
float4 main( float4 pos : SV_POSITION) : SV_Target
{
	#if 1
	float scol = calc( pos.xy/resolution.xy, time );
    #else
	float scol = 0.0;
	float h = 0.0;
	float iSamples = 1.0/float(NumSamples);
	for( int i=0; i<NumSamples; i++ )
	{
		float2 of = 0.5 + 0.5*float2( cos(6.3*h), sin(15.0*h) );
	    scol += calc( (pos.xy+of)/resolution.xy, time - h*0.4/24.0 );
		h += iSamples;
	}
	scol *= iSamples;
	#endif
  
	float3 vcol = pow( float3(scol, scol, scol), float3(1.0,1.1,1.4) );
	
	vcol *= float3(1.0,0.98,0.95);
	
	float2 uv = pos.xy/resolution.xy;
	vcol *= 0.7 + 0.3*pow(16.0*uv.x*uv.y*(1.0-uv.x)*(1.0-uv.y),0.25);
	
	return float4( vcol, 1.0 );
}