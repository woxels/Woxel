/*
    James William Fletcher (github.com/mrbid)
        September 2021 - February 2023

    Portable floating-point Vec4 lib.
*/

#ifndef VEC_H
#define VEC_H

#include <math.h>
#include <string.h>

#define PI 3.141592741f         // PI
#define x2PI 6.283185482f       // PI * 2
#define d2PI 1.570796371f       // PI / 2
#define DEGREE 57.29578018f     // 1 Radian as Degrees
#define RADIAN 0.01745329238f   // PI / 180 (1 Degree as Radians)
#define RAD2DEG DEGREE
#define DEG2RAD RADIAN

typedef struct{
    float x,y,z,w;
} vec;

static inline float randf();  // uniform [0 to 1]
static inline float randfc(); // uniform [-1 to 1]
float randfn(); // box-muller normal [bi-directional]

static inline float fRandFloat(const float min, const float max);
static inline int   fRand(const float min, const float max);

int vec_ftoi(float f); // float to integer quantise

// normalising the result is optional / at the callers responsibility
void vRuv(vec* v);   // Random Unit Vector
void vRuvN(vec* v);  // Normal Random Unit Vector
void vRuvBT(vec* v); // Brian Tung Random Unit Vector (on surface of unit sphere)
void vRuvTA(vec* v); // T.Davison Trial & Error (inside unit sphere)
void vRuvTD(vec* v); // T.Davison Random Unit Vector Sphere

void  vCross(vec* r, const vec v1, const vec v2);
float vDot(const vec v1, const vec v2);
float vSum(const vec v);
float vSumAbs(const vec v);
void  vReflect(vec* r, const vec v, const vec n);

int  vEqualTol(const vec a, const vec b, const float tol);
int  vEqualInt(const vec a, const vec b);
void vMin(vec* r, const vec v1, const vec v2);
void vMax(vec* r, const vec v1, const vec v2);

void  vNorm(vec* v);
float vDist(const vec v1, const vec v2);
float vDistSq(const vec a, const vec b);
float vDistMh(const vec a, const vec b); // manhattan
float vDistLa(const vec a, const vec b); // longest axis
float vMod(const vec v); // modulus
float vMag(const vec v); // magnitude
void  vInv(vec* v); // invert
void  vCopy(vec* r, const vec v);
void  vDir(vec* r, const vec v1, const vec v2); // direction vector from v1 to v2

void vRotX(vec* v, const float radians);
void vRotY(vec* v, const float radians);
void vRotZ(vec* v, const float radians);

void vAdd(vec* r, const vec v1, const vec v2);
void vSub(vec* r, const vec v1, const vec v2);
void vDiv(vec* r, const vec numerator, const vec denominator);
void vMul(vec* r, const vec v1, const vec v2);

void vAddS(vec* r, const vec v1, const float v2);
void vSubS(vec* r, const vec v1, const float v2);
void vDivS(vec* r, const vec v1, const float v2);
void vMulS(vec* r, const vec v1, const float v2);

//

int srandfq = 74235;
static inline void srandf(const int seed){srandfq = seed;}
static inline float randf()
{
    // https://www.musicdsp.org/en/latest/Other/273-fast-float-random-numbers.html (moc.liamg@seir.kinimod)
    srandfq *= 16807;
    return (float)(srandfq & 0x7FFFFFFF) * 4.6566129e-010f;
}
static inline float randfc()
{
    // https://www.musicdsp.org/en/latest/Other/273-fast-float-random-numbers.html (moc.liamg@seir.kinimod)
    srandfq *= 16807;
    return ((float)(srandfq)) * 4.6566129e-010f;
}
static inline float fRandFloat(const float min, const float max)
{
    return min + randf() * (max-min); 
}
static inline int fRand(const float min, const float max)
{
    return (int)((min + randf() * (max-min))+0.5f); 
}

float randfn()
{
    float u = randfc();
    float v = randfc();
    float r = u * u + v * v;
    while(r == 0.f || r > 1.f)
    {
        u = randfc();
        v = randfc();
        r = u * u + v * v;
    }
    return u * sqrtf(-2.f * logf(r) / r);
}

void vRuv(vec* v)
{
    v->x = randfc();
    v->y = randfc();
    v->z = randfc();
}

void vRuvN(vec* v)
{
    v->x = randfn();
    v->y = randfn();
    v->z = randfn();
}

void vRuvBT(vec* v)
{
    // https://math.stackexchange.com/a/1586185
    // or should I have called this vRuvLR()
    // https://mathworld.wolfram.com/SpherePointPicking.html
    const float y = acosf(randfc()) - d2PI;
    const float p = x2PI * randf();
    v->x = cosf(y) * cosf(p);
    v->y = cosf(y) * sinf(p);
    v->z = sinf(y);
}

void vRuvTA(vec* v)
{
    // T.P.Davison@tees.ac.uk
    while(1)
    {
        v->x = randfc();
        v->y = randfc();
        v->z = randfc();
        const float len = vMag(*v);
        if(len <= 1.0f){return;}
    }
}

void vRuvTD(vec* v)
{
    // T.P.Davison@tees.ac.uk
    v->x = sinf((randf() * x2PI) - PI);
    v->y = cosf((randf() * x2PI) - PI);
    v->z = randfc();
}

void vCross(vec* r, const vec v1, const vec v2)
{
    r->x = (v1.y * v2.z) - (v2.y * v1.z);
    r->y = -((v1.x * v2.z) - (v2.x * v1.z));
    r->z = (v1.x * v2.y) - (v2.x * v1.y);
}

float vDot(const vec v1, const vec v2)
{
    return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z);
}

float vSum(const vec v)
{
    return v.x + v.y + v.z;
}

float vSumAbs(const vec v)
{
    return fabs(v.x) + fabs(v.y) + fabs(v.z);
}

void vInv(vec* v)
{
    v->x = -v->x;
    v->y = -v->y;
    v->z = -v->z;
}

void vNorm(vec* v)
{
    const float len = 1.f/sqrtf(v->x*v->x + v->y*v->y + v->z*v->z);
    v->x *= len;
    v->y *= len;
    v->z *= len;
}

float vDist(const vec v1, const vec v2)
{
    const float xm = (v1.x - v2.x);
    const float ym = (v1.y - v2.y);
    const float zm = (v1.z - v2.z);
    return sqrtf(xm*xm + ym*ym + zm*zm);
}

float vDistSq(const vec a, const vec b)
{
    const float xm = (a.x - b.x);
    const float ym = (a.y - b.y);
    const float zm = (a.z - b.z);
    return xm*xm + ym*ym + zm*zm;
}

float vDistMh(const vec a, const vec b)
{
    return (a.x - b.x) + (a.y - b.y) + (a.z - b.z);
}

float vDistLa(const vec v1, const vec v2)
{
    const float xm = fabsf(v1.x - v2.x);
    const float ym = fabsf(v1.y - v2.y);
    const float zm = fabsf(v1.z - v2.z);

    float dist = xm;
    if(ym > dist)
        dist = ym;
    if(zm > dist)
        dist = zm;

    return dist;
}

void vReflect(vec* r, const vec v, const vec n)
{
    const float angle = vDot(v, n);
    r->x = v.x - (2.f * n.x) * angle;
    r->y = v.y - (2.f * n.y) * angle;
    r->z = v.z - (2.f * n.z) * angle;
}

int vEqualTol(const vec a, const vec b, const float tol)
{
    return  a.x >= b.x - tol && a.x <= b.x + tol &&
            a.y >= b.y - tol && a.y <= b.y + tol &&
            a.z >= b.z - tol && a.z <= b.z + tol;
}

void vMin(vec* r, const vec v1, const vec v2)
{
    if(v1.x < v2.x && v1.y < v2.y && v1.z < v2.z)
    {
        r->x = v1.x;
        r->y = v1.y;
        r->z = v1.z;
    }

    r->x = v2.x;
    r->y = v2.y;
    r->z = v2.z;
}

void vMax(vec* r, const vec v1, const vec v2)
{
    if(v1.x > v2.x && v1.y > v2.y && v1.z > v2.z)
    {
        r->x = v1.x;
        r->y = v1.y;
        r->z = v1.z;
    }

    r->x = v2.x;
    r->y = v2.y;
    r->z = v2.z;
}

int vec_ftoi(float f)
{
    if(f < 0.f)
        f -= 0.5f;
    else
        f += 0.5f;
    return (int)f;
}

int vEqualInt(const vec a, const vec b)
{
    return vec_ftoi(a.x) == vec_ftoi(b.x) && vec_ftoi(a.y) == vec_ftoi(b.y) && vec_ftoi(a.z) == vec_ftoi(b.z);
}

float vMod(const vec v)
{
    return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
}

float vMag(const vec v)
{
    return v.x*v.x + v.y*v.y + v.z*v.z;
}

void vCopy(vec* r, const vec v)
{
    memcpy(r, &v, sizeof(vec));
}

void vDir(vec* r, const vec v1, const vec v2)
{
    vSub(r, v2, v1);
    vNorm(r);
}

void vRotX(vec* v, const float radians)
{
    v->y = v->y * cosf(radians) + v->z * sinf(radians);
    v->z = v->y * sinf(radians) - v->z * cosf(radians);
}

void vRotY(vec* v, const float radians)
{
    v->x = v->z * sinf(radians) - v->x * cosf(radians);
    v->z = v->z * cosf(radians) + v->x * sinf(radians);
}

void vRotZ(vec* v, const float radians)
{
    v->x = v->x * cosf(radians) + v->y * sinf(radians);
    v->y = v->x * sinf(radians) - v->y * cosf(radians);
}

void vAdd(vec* r, const vec v1, const vec v2)
{
    r->x = v1.x + v2.x;
    r->y = v1.y + v2.y;
    r->z = v1.z + v2.z;
}

void vSub(vec* r, const vec v1, const vec v2)
{
    r->x = v1.x - v2.x;
    r->y = v1.y - v2.y;
    r->z = v1.z - v2.z;
}

void vDiv(vec* r, const vec numerator, const vec denominator)
{
    r->x = numerator.x / denominator.x;
    r->y = numerator.y / denominator.y;
    r->z = numerator.z / denominator.z;
}

void vMul(vec* r, const vec v1, const vec v2)
{
    r->x = v1.x * v2.x;
    r->y = v1.y * v2.y;
    r->z = v1.z * v2.z;
}

void vAddS(vec* r, const vec v1, const float v2)
{
    r->x = v1.x + v2;
    r->y = v1.y + v2;
    r->z = v1.z + v2;
}

void vSubS(vec* r, const vec v1, const float v2)
{
    r->x = v1.x - v2;
    r->y = v1.y - v2;
    r->z = v1.z - v2;
}

void vDivS(vec* r, const vec v1, const float v2)
{
    r->x = v1.x / v2;
    r->y = v1.y / v2;
    r->z = v1.z / v2;
}

void vMulS(vec* r, const vec v1, const float v2)
{
    r->x = v1.x * v2;
    r->y = v1.y * v2;
    r->z = v1.z * v2;
}

#endif
