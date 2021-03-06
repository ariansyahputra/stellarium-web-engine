/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifdef GL_ES
precision mediump float;
#endif

uniform highp float u_atm_p[12];
uniform highp vec3  u_sun;
uniform highp float u_tm[3]; // Tonemapping koefs.

varying lowp    vec4        v_color;

#ifdef VERTEX_SHADER

attribute highp   vec4       a_pos;
attribute highp   vec3       a_sky_pos;
attribute highp   float      a_luminance;

vec3 xyy_to_srgb(vec3 xyy)
{
    vec3 xyz;
    vec3 rgb;
    const mat3 xyz_to_rgb = mat3(3.2406, -0.9689, 0.0557,
                                 -1.5372, 1.8758, -0.2040,
                                 -0.4986, 0.0415, 1.0570);
    xyz = vec3(xyy[0] * xyy[2] / xyy[1], xyy[2],
               (1.0 - xyy[0] - xyy[1]) * xyy[2] / xyy[1]);
    rgb = xyz_to_rgb * xyz;
    return pow(rgb, vec3(1.0 / 2.2));
}

float tonemap(float lw)
{
    // Logarithmic tonemapping, same as in tonemapper.c
    // Assumes u_tm[2] == 1.
    return log(1.0 + u_tm[0] * lw) / log(1.0 + u_tm[0] * u_tm[1]);
}

void main()
{
    vec3 xyy;
    float cos_gamma, cos_gamma2, gamma, cos_theta;
    vec3 p = a_sky_pos;

    gl_Position = a_pos;

    p[2] = abs(p[2]); // Mirror below horizon.
    cos_gamma = dot(p, u_sun);
    cos_gamma2 = cos_gamma * cos_gamma;
    gamma = acos(cos_gamma);
    cos_theta = p[2];

    xyy.x = ((1. + u_atm_p[0] * exp(u_atm_p[1] / cos_theta)) *
             (1. + u_atm_p[2] * exp(u_atm_p[3] * gamma) +
              u_atm_p[4] * cos_gamma2)) * u_atm_p[5];
    xyy.y = ((1. + u_atm_p[6] * exp(u_atm_p[7] / cos_theta)) *
             (1. + u_atm_p[8] * exp(u_atm_p[9] * gamma) +
              u_atm_p[10] * cos_gamma2)) * u_atm_p[11];
    xyy.z = a_luminance;

    // Scotopic vision adjustment with blue shift (xy = 0.25, 0.25)
    // Algo inspired from Stellarium.
    if (xyy.z < 3.9) {
        float s, v;
        // s: ratio between scotopic and photopic vision.
        s = smoothstep(0.0, 1.0, (log(xyy.z) / log(10.) + 2.) / 2.6);
        xyy.x = mix(0.25, xyy.x, s);
        xyy.y = mix(0.25, xyy.y, s);
        v = xyy.z * (1.33 * (1. + xyy.y / xyy.x + xyy.x *
                            (1. - xyy.x - xyy.y)) - 1.68);
        xyy.z = 0.4468 * (1. - s) * v + s * xyy.z;
    }

    xyy.z = max(0.0, tonemap(xyy.z));
    v_color = vec4(clamp(xyy_to_srgb(xyy), 0.0, 1.0), 1.0);
}

#endif
#ifdef FRAGMENT_SHADER

void main()
{
    gl_FragColor = v_color;
}

#endif
