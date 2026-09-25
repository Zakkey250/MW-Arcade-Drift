#pragma once
#include <cmath>
namespace mcd {
struct Vector {float x,y,z;};
inline Vector add(Vector a,Vector b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vector mul(Vector a,float k){return {a.x*k,a.y*k,a.z*k};}
inline float dot(Vector a,Vector b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vector cross(Vector a,Vector b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline Vector tangent(Vector v,Vector n){return add(v,mul(n,-dot(v,n)));}
inline float length(Vector v){return std::sqrt(dot(v,v));}
inline Vector rotateSurface(Vector v,Vector n,float angle,float speedDelta){
 float normal=dot(v,n);Vector t=tangent(v,n);float speed=length(t);
 if(speed<.001f)return v;
 Vector rotated=add(mul(t,std::cos(angle)),mul(cross(n,t),std::sin(angle)));
 return add(mul(rotated,(speed+speedDelta)/speed),mul(n,normal));
}
inline float surfaceBeta(Vector forward,Vector velocity,Vector n){
 auto f=tangent(forward,n),v=tangent(velocity,n);
 return std::atan2(dot(n,cross(v,f)),dot(v,f));
}
}
