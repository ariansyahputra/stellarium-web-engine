/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include <math.h>
#include "skybrightness.h"
#include "utils/utils.h"

#define exp10(x) exp((x) * log(10.f))
#define exp10f(x) expf((x) * logf(10.f))

static inline float pow2(float x) { return x * x; }
static inline float pow4(float x) { return x * x * x * x; }

static inline float fast_expf(float x) {
    x = 1.0f + x / 1024;
    x *= x; x *= x; x *= x; x *= x;
    x *= x; x *= x; x *= x; x *= x;
    x *= x; x *= x;
    return x;
}

static inline float fast_exp10f(float x) {
    return fast_expf(x * logf(10.f));
}

// Radiant to degree.
static const float DR = 180.0f / 3.14159f;

// Nanolambert to cd/m²
static const float NLAMBERT_TO_CDM2 = 3.183e-6f;


static const float WA = 0.55f;
static const float MO = -11.05f;
static const float OZ = 0.031f;
static const float WT = 0.031f;
static const float BO = 1.0E-13f;
static const float CM = 0.00f;
static const float MS = -26.74f;

void skybrightness_prepare(skybrightness_t *sb,
        int year, int month, float moon_phase,
        float latitude, float altitude,
        float temperature, float relative_humidity,
        float dist_moon_zenith, float dist_sun_zenith,
        float twilight_coef, float moon_brightness_coef,
        float darknight_brightness_coef)
{
    sb->Y = year;
    sb->M = month;
    sb->AM = moon_phase * DR;
    sb->LA = latitude * DR;
    sb->AL = altitude;
    sb->TE = temperature;
    sb->RH = relative_humidity;
    sb->ZM = dist_moon_zenith * DR;
    sb->ZS = dist_sun_zenith * DR;
    sb->k_BT = twilight_coef;
    sb->k_BM = moon_brightness_coef;
    sb->k_BN = darknight_brightness_coef;

    // Precompute as much as possible.
    float K, KR, KA, KO, KW, LT, RA, SL, XM, XS;
    const float M = sb->M; // Month (1=Jan, 12=Dec)
    const float RD = 3.14159f / 180.0f;
    const float LA = sb->LA; // Latitude (deg.)
    const float AL = sb->AL; // Altitude above sea level (m)
    const float RH = sb->RH; // relative humidity (%)
    const float TE = sb->TE; // Air temperature (deg. C)
    const float ZM = sb->ZM; // Zenith distance of Moon (deg.)
    const float ZS = sb->ZS; // Zenith distance of Sun (deg.)

    LT = LA * RD;
    RA = (M - 3) * 30.0f * RD;
    SL = LA > 0.f ? 1.f : -1.f;
    // 1080 Airmass for each component
    // 1130 UBVRI extinction for each component
    KR = .1066f * expf(-1 * AL / 8200) * powf((WA / .55f), -4);
    KA = .1f * powf((WA / .55f), -1.3f) * expf(-1 * AL / 1500);
    KA = KA * powf((1 - .32f / logf(RH / 100.0f)), 1.33f) *
             (1 + 0.33f * SL * sinf(RA));
    KO = OZ * (3.0f + .4f * (LT * cosf(RA) - cosf(3 * LT))) / 3.0f;
    KW = WT * .94f * (RH / 100.0f) * expf(TE / 15) * expf(-1 * AL / 8200);
    K = KR + KA + KO + KW;
    sb->K = K;

    // air mass Moon
    XM = 1 / (cosf(ZM * RD) + .025f * expf(-11 * cosf(ZM * RD)));
    if (ZM > 90.0f) XM = 40.0;
    sb->XM = XM;

    // air mass Sun
    XS = 1 / (cosf(ZS * RD) + .025f * expf(-11 * cosf(ZS * RD)));
    if (ZS > 90.0f) XS = 40.0;
    sb->XS = XS;
}


float skybrightness_get_luminance(
        const skybrightness_t *sb,
        float moon_dist, float sun_dist, float zenith_dist)
{
    float BL, B, ZZ,
           K, X, XS, XM, BN, MM, C3, FM, BM, HS, BT, C4, FS,
           BD;

    const float RD = 3.14159f / 180.0f;
    // 80 Input for Moon and Sun
    const float AM = sb->AM; // Moon phase (deg.; 0=FM, 90=FQ/LQ, 180=NM)
    const float ZM = sb->ZM; // Zenith distance of Moon (deg.)
    const float RM = max(moon_dist * DR, 1.); // Angular distance to Moon (deg)
    const float ZS = sb->ZS; // Zenith distance of Sun (deg.)
    const float RS = max(sun_dist * DR, 1.); // Angular distance to Sun (deg.)
    // 140 Input for the Site, Date, Observer
    const float Y = sb->Y; // Year
    const float Z = zenith_dist * DR; // Zenith distance (deg.)

    // 1000 Extinction Subroutine
    // 1080 Airmass for each component
    ZZ = Z * RD;
    K = sb->K;

    // 2000 SKY Subroutine
    X = 1 / (cosf(ZZ) + .025f * fast_expf(-11 * cosf(ZZ))); // air mass
    XM = sb->XM;
    XS = sb->XS;

    // 2130 Dark night sky brightness
    BN = BO * (1 + .3f * cosf(6.283f * (Y - 1992) / 11));
    BN = BN * (.4f + .6f / sqrtf(1.0f - .96f * powf((sinf(ZZ)), 2)));
    BN = BN * (fast_exp10f(-.4f * K * X));
    BN *= sb->k_BN;

    // 2170 Moonlight brightness
    MM = -12.73f + .026f * fabsf(AM) + 4E-09f * pow4(AM); // moon mag in V
    MM = MM + CM; // Moon mag
    C3 = fast_exp10f(-.4f * K * XM);
    FM = 6.2E+07f / pow2(RM) + (exp10f(6.15f - RM / 40));
    FM = FM + exp10f(5.36f) * (1.06f + pow2(cosf(RM * RD)));
    BM = exp10f(-.4f * (MM - MO + 43.27f));
    BM = BM * (1 - exp10f(-.4f * K * X));
    BM = BM * (FM * C3 + 440000.0f * (1 - C3));

    // Added from the original code, a scale factor
    BM *= sb->k_BM;

    // 2260 Twilight brightness
    HS = 90.0f - ZS; // Height of Sun
    BT = exp10f(-.4f * (MS - MO + 32.5f - HS - (Z / (360 * K))));
    BT = BT * (100 / RS) * (1.0f - exp10f(-.4f * K * X));
    BT *= sb->k_BT;

    // 2300 Daylight brightness
    C4 = fast_exp10f(-.4f * K * XS);
    FS = 6.2E+07f / pow2(RS) + (fast_exp10f(6.15f - RS / 40));
    FS = FS + fast_exp10f(5.36f) * (1.06f + pow2(cosf(RS * RD)));
    BD = exp10f(-.4f * (MS - MO + 43.27f));
    BD = BD * (1 - exp10f(-.4f * K * X));
    BD = BD * (FS * C4 + 440000.0f * (1 - C4));

    // 2370 Total sky brightness
    if (BD > BT)
        B = BN + BT;
    else
        B = BN + BD;
    if (ZM < 80.0f) B = B + BM;
    // Graduate the moon impact on atmosphere from 0 to 100% when its altitude
    // is ranging from 0 to 10 deg to avoid discontinuity.
    // This hack can probably be reduced when extinction is taken into account.
    if (ZM > 80.0f && ZM <= 90.0f) B = B + BM * (90.f - ZM) / 10.f;
    // End sky subroutine.


    // 250 Visual limiting magnitude
    BL = B / 1.11E-15f; // in nanolamberts
    // 330 PRINT : REM  Write results and stop program
    return BL * NLAMBERT_TO_CDM2;
}
