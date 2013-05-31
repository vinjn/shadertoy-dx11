// https://www.shadertoy.com/view/MdsGz8
// @eddbiddulph
// Use the mouse to rotate the view!

#define EPS vec2(1e-3, 0.0)

vec3 rotateX(float a, vec3 v)
{
   return vec3(v.x, cos(a) * v.y + sin(a) * v.z, cos(a) * v.z - sin(a) * v.y);
}

vec3 rotateY(float a, vec3 v)
{
   return vec3(cos(a) * v.x + sin(a) * v.z, v.y, cos(a) * v.z - sin(a) * v.x);
}

float cube(vec3 p, vec3 s)
{
   return length(max(vec3(0.0), abs(p) - s));
}

float customCube(vec3 p, vec3 s)
{
   return cube(p, s) - 0.01;
}

float circular(float t)
{
   return sqrt(1.0 - t * t);
}

float processionOfCubes(vec3 p)
{
   float t = -iGlobalTime * 0.2;
   p.z -= t * 2.5;
   float rad = 0.1 + cos(floor(p.z) * 10.0) * 0.07;
   p.x += cos(floor(p.z) * 2.0) * 0.3;
   p.z -= floor(p.z);
   t /= rad;
   t -= floor(t);
   vec3 org = vec3(0.0, circular((t - 0.5) * 1.5) * length(vec2(rad, rad)), 0.5);
   return customCube(rotateX(t * 3.1415926 * 0.5, p - org), vec3(rad));
}

float scene(vec3 p)
{
   float tunnel0 = max(-p.y, length(max(vec2(0.0), vec2(-abs(p.z) + 2.0, max(length(p.xy) - 2.0, 0.9 - length(p.xy))))) - 0.03);
   return min(tunnel0, processionOfCubes(p));
}

vec3 sceneGrad(vec3 p)
{
   float d = scene(p);
   return (vec3(scene(p + EPS.xyy), scene(p + EPS.yxy), scene(p + EPS.yyx)) - vec3(d)) / EPS.x;
}

vec3 veil(vec3 p)
{
   float l = length(p);
   return vec3(1.0 - smoothstep(0.0, 4.0, l)) * vec3(1.0, 1.0, 0.75);
}

vec3 environment(vec3 ro, vec3 rd)
{
   float t = -ro.y / rd.y;
   vec2 tc = ro.xz + rd.xz * t;
   vec3 p = vec3(tc.x, 0.0, tc.y);
   float d = scene(p);

   float u = fract(dot(tc, vec2(1.0)) * 20.0);
   float s = t * 2.0;
   float stripes = smoothstep(0.0, 0.1 * s, u) - smoothstep(0.5, 0.5 + 0.1 * s, u);


   vec3 col = mix(vec3(0.3, 0.3, 0.6), vec3(0.3, 0.3, 0.6) * 1.3, stripes);

   return veil(p) * col * mix(0.0,
         mix(0.9, 1.0, 1.0) *
         mix(0.5, 1.0, sqrt(smoothstep(0.0, 0.3, d))) *
         mix(0.5, 1.0, sqrt(smoothstep(0.0, 0.06, d))), step(0.0, t));
}

void main(void)
{
	vec2 uv = gl_FragCoord.xy / iResolution.xy * 2.0 - vec2(1.0);
	uv.x *= iResolution.x / iResolution.y;
	vec3 ro = vec3(0.0, 0.2, 1.0), rd = vec3(uv, -0.9);
	
	vec2 angs = vec2(1.0-iMouse.y * 0.003, 1.0-iMouse.x * 0.01);
	
	ro = rotateY(angs.y, ro) + vec3(0.0, 0.6, 0.0);
	rd = rotateY(angs.y, rotateX(angs.x, rd));
	
	gl_FragColor.rgb = environment(ro, rd);
	
	for(int i = 0; i < 200; i += 1)
	{
		float d = scene(ro);
		
		if(abs(d) < 1e-3)
		{
			gl_FragColor.rgb = veil(ro) * mix(vec3(0.1, 0.1, 0.4) * 0.25, vec3(1.0),
						  (1.0 + dot(normalize(sceneGrad(ro)), normalize(vec3(0.0, 1.0, 0.4)))) * 0.5);
		}
		
		ro += rd * d * 0.5;
	}
	
	gl_FragColor.rgb = sqrt(gl_FragColor.rgb);
	gl_FragColor.a = 1.0;
}


