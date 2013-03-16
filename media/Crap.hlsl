// ..\\media\\ducky.png

float mod289(float x)
{
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

float4 mod289(float4 x)
{
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

float4 perm(float4 x)
{
    return mod289(((x * 34.0) + 1.0) * x);
}

float noise3d(float3 p)
{
    float3 a = floor(p);
    float3 d = p - a;
    d = d * d * (3.0 - 2.0 * d);

    float4 b = a.xxyy + float4(0.0, 1.0, 0.0, 1.0);
    float4 k1 = perm(b.xyxy);
    float4 k2 = perm(k1.xyxy + b.zzww);

    float4 c = k2 + a.zzzz;
    float4 k3 = perm(c);
    float4 k4 = perm(c + 1.0);

    float4 o1 = frac(k3 * (1.0 / 41.0));
    float4 o2 = frac(k4 * (1.0 / 41.0));

    float4 o3 = o2 * d.z + o1 * (1.0 - d.z);
    float2 o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);

    return o4.y * d.y + o4.x * (1.0 - d.y);
}

float4 main( float4 pos: SV_POSITION) : SV_Target
{
	float2 uv = 2.0 * pos.xy / iResolution - 1.0;
	
    float3 water[4];
    float3 fire[4];

    float3x3 r = float3x3(0.36, 0.48, -0.8, -0.8, 0.60, 0.0, 0.48, 0.64, 0.60);
    float3 p_pos = mul(float3(uv * float2(16.0, 9.0), 0.0), r);
    float3 p_time = mul(float3(0.0, 0.0, iGlobalTime * 2.0), r);

    /* Noise sampling points for water */
    water[0] = p_pos / 2.0 + p_time;
    water[1] = p_pos / 4.0 + p_time;
    water[2] = p_pos / 8.0 + p_time;
    water[3] = p_pos / 16.0 + p_time;

    /* Noise sampling points for fire */
    p_pos = 16.0 * p_pos - mul(float3(0.0, mod289(iGlobalTime) * 128.0, 0.0), r);
    fire[0] = p_pos / 2.0 + p_time * 2.0;
    fire[1] = p_pos / 4.0 + p_time * 1.5;
    fire[2] = p_pos / 8.0 + p_time;
    fire[3] = p_pos / 16.0 + p_time;

    float2x2 rot = float2x2(cos(iGlobalTime), sin(iGlobalTime), -sin(iGlobalTime), cos(iGlobalTime));

	float2 poszw = mul(uv, rot);

	/* Dither the transition between water and fire */
    float test = poszw.x * poszw.y + 1.5 * sin(iGlobalTime);
    float2 d = float2(16.0, 9.0) * uv;
    test += 0.5 * (length(frac(d) - 0.5) - length(frac(d + 0.5) - 0.5));

    /* Compute 4 octaves of noise */
    float3 points[4];
	points[0] = (test > 0.0) ? fire[0] : water[0];
	points[1] = (test > 0.0) ? fire[1] : water[1];
	points[2] = (test > 0.0) ? fire[2] : water[2];
	points[3] = (test > 0.0) ? fire[3] : water[3];
	
    float4 n = float4(noise3d(points[0]),
                  noise3d(points[1]),
                  noise3d(points[2]),
                  noise3d(points[3]));

    float4 color;

    if (test > 0.0)
    {
        /* Use noise results for fire */
        float p = dot(n, float4(0.125, 0.125, 0.25, 0.5));

        /* Fade to black on top of screen */
        p -= uv.y * 0.8 + 0.25;
        p = max(p, 0.0);
        p = min(p, 1.0);

        float q = p * p * (3.0 - 2.0 * p);
        float r = q * q * (3.0 - 2.0 * q);
        color = float4(min(q * 2.0, 1.0),
                     max(r * 1.5 - 0.5, 0.0),
                     max(q * 8.0 - 7.3, 0.0),
                     1.0);
    }
    else
    {
        /* Use noise results for water */
        float p = dot(abs(2.0 * n - 1.0),
                      float4(0.5, 0.25, 0.125, 0.125));
        float q = sqrt(p);

        color = float4(1.0 - q,
                     1.0 - 0.5 * q,
                     1.0,
                     1.0);
    }

	return color;
}