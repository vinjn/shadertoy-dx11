// Shader Invaders!!
// Concept & code by Alan Mackey.  Alien graphic by @emackey.
// Original version is http://glsl.heroku.com/424/12

bool IsUninitialized(float3 color) {
    return (color.r == 0.0f && color.g == 0.0f && color.b == 0.0f);
}

bool IsEnemy(float3 color) {
    return ((color.b > color.r) && (color.b > 0.5));
}

bool IsShot(float3 color) {
    return ((color.r > color.g) && (color.g == 0.0));
}

bool IsExplosion(float3 color) {
    return ((color.r == color.g) && (color.r > 0.77));
}

bool IsDebris(float3 color) {
    if (IsShot(color)) {return false;}
    return (color.r > 0.2);
}

bool alien(float x, float y) {
    if (x > 5.9999) {
        x = 11.0 - x;
    }

    if ((x < -0.0001) || (y < -0.0001)) {
        return false;
    } else if (y <= 1.9999) {
        return (x >= (1.9999 - y));
    } else if (y <= 3.9999) {
        return ((x < 1.9999) || (x >= 3.9999));
    } else if (y <= 7.9999) {
        return true;
    } else if (y <= 10.9999) {
        return ((x >= (10.9999 - y)) && (x <= (12.9999 - y)));
    }
    return false;
}

float  mod(float x, float y)
{
    return x - y * trunc(x/y);
}

float4 main( float4 pos : SV_POSITION ) : SV_Target
{
    float2 position = ( pos.xy / resolution.xy );
    float2 pixel = 1./resolution.xy;
    float2 mousepx = dot(mouse.xy, pixel);

    float3 space = float3(0.02, 0.04, 0.1);
    float3 shot = float3(1.0, 0.0, 0.3);
    float3 enemy = float3(0.3, 0.55, 0.65);
    float3 explosion = float3(0.8, 0.8, 0.2);

    float4 old = backbuffer.Sample(smooth, position);
    float4 me = float4(space.r, space.g, space.b, 1.0);

    // Y < 0.1: ship
    if (position.y < 0.02) {
        // empty; do nothing
    } else if (position.y < 0.1) {
        // Player ship
        if (abs(position.x - mousepx.x) < (0.1 - position.y) * 0.25) {
            me.rgb = float3(0.5, 0.7, 0.6);
        }
    } else if (position.y < 0.105) {
        // Shot generator
        if ((abs(position.x - mousepx.x) <= pixel.x) && (mod(time * 2.0, 1.0) < 0.1)) {
            me.rgb = shot;
        }
    } else {
        // Playing field
        float shoty = max(position.y - 0.015, 0.1025);
        float4 below = backbuffer.Sample(smooth, float2(position.x, shoty));
        float offset = 0.0;
        float offsetpx = 0.0;

        if (mod(time * 1.0, 1.0) > 0.95) {
            // Enemy marching
            offset = 0.01 * ((old.a > 0.5) ? 1.0 : -1.0);
            offsetpx = 0.01 / pixel.x;
            old.rgb = backbuffer.Sample(smooth, position + float2(offset, 0.0)).rgb;

            if (old.a < 0.4) {
                me.a = old.a + 0.015;
            } else if (old.a > 0.6) {
                me.a = old.a - 0.015;
            } else {
                // Change direction!
                me.a = (old.a > 0.5) ? 0.0 : 1.0;
            }
        } else {
            me.a = old.a;
        }

        if (IsUninitialized(old.rgb)) {
            // draw enemy ships for first time
            float enemyrow = (1.0 - position.y - 0.05) * 12.0;
            float enemyrowmod = mod(enemyrow, 1.0);
            float enemycol = position.x * 15.0 - 0.2;
            float enemycolmod = mod(enemycol, 1.0);
            bool oddrow = (floor(mod(enemyrow, 2.0)) == 1.0);

            if ((enemyrow >= 0.0) && (enemyrow < 4.0)) {
                me.a = (oddrow) ? 1.0 : 0.0;
                if ((enemycol >= (oddrow ? 4.0 : 0.0)) && (enemycol < (oddrow ? 15.0 : 11.0))) {
                    me.rgb = alien(enemycolmod * 25.0, enemyrowmod * 25.0) ? enemy : space;
                }
            }
        } else if (IsShot(below.rgb)) {
            // Move shot up; look for collision w/ enemy or debris
            if (IsEnemy(old.rgb)) {
                me.rgb = explosion;
            } else if (IsDebris(old.rgb)) {
                me.rgb = old.rgb;
            } else {
                me.rgb = shot;
            }
        } else if (IsEnemy(old.rgb)) {
            // Grow explosions to consume whole enemy
            bool exploding = 
                (IsExplosion(backbuffer.Sample(smooth, position + float2(offsetpx + 1.0, 0.0) * pixel).rgb)) ||
                (IsExplosion(backbuffer.Sample(smooth, position + float2(offsetpx - 1.0, 0.0) * pixel).rgb)) ||
                (IsExplosion(backbuffer.Sample(smooth, position + float2(offsetpx, 1.0) * pixel).rgb)) ||
                (IsExplosion(backbuffer.Sample(smooth, position + float2(offsetpx, -1.0) * pixel).rgb));
            me.rgb = exploding ? explosion : old.rgb;
        } else {
            // Fade debris to background color
            if (!IsShot(old.rgb) && !IsEnemy(old.rgb)) {
                float fade = mod(frac(sin(dot(position + time * 0.001, float2(14.9898,78.233))) * 43758.5453), 1.0);
                fade = pow(fade, 6.0) * 0.4;
                me.rgb = old.rgb * (1.0 - fade) + space * fade;
                if (length(me.rgb - space) < 0.05) {me.rgb = space;}
            }
        }
    }
    return me;
}