#pragma once
#define MCD_DRIFT_FIELDS(X) \
 X(entrySpeedMps,12.0f,8.0f,50.0f) \
 X(entryHoldSeconds,0.025f,0.01f,0.5f) \
 X(neutralExitSeconds,0.15f,0.05f,0.5f) \
 X(baseSlipRad,0.09f,0.0f,0.3f) \
 X(slideSlipRad,0.18f,0.0f,0.4f) \
 X(handbrakeSlipRad,0.14f,0.0f,0.3f) \
 X(targetYawMax,0.8f,0.1f,1.2f) \
 X(targetPathSpeed,36.0f,5.0f,60.0f) \
 X(highSpeedGain,0.3f,0.0f,0.6f) \
 X(boostStartKmh,120.0f,40.0f,250.0f) \
 X(boostEndKmh,240.0f,80.0f,400.0f) \
 X(handbrakeTurnGain,0.25f,0.0f,0.5f) \
 X(pathAssistLimit,30.0f,0.0f,50.0f) \
 X(pathSlew,0.9f,0.1f,1.5f) \
 X(refundFraction,0.98f,0.0f,1.0f) \
 X(recoveryAccel,3.0f,0.0f,6.0f) \
 X(refundMaxAccel,18.0f,0.0f,20.0f)
#define MCD_CAMERA_FIELDS(X) \
 X(orbitGain,1.8f,0.0f,3.0f) \
 X(orbitMaxRad,0.4886922f,0.0f,0.7f) \
 X(orbitSpeed,0.45f,0.05f,1.0f) \
 X(orbitAccel,0.8f,0.1f,2.0f) \
 X(rollGain,0.06f,0.0f,0.2f) \
 X(rollMaxRad,0.0174533f,0.0f,0.05f) \
 X(rollSpeed,0.045f,0.01f,0.1f) \
 X(rollAccel,0.12f,0.02f,0.3f) \
 X(zoomSlipRad,0.12f,0.03f,0.4f) \
 X(zoomSpeed,0.5f,0.1f,1.0f) \
 X(zoomAccel,0.75f,0.1f,2.0f) \
 X(minDistance,3.75f,3.0f,8.0f) \
 X(closeFraction,0.38f,0.0f,0.5f) \
 X(sideAngleRad,0.2f,0.05f,0.5f) \
 X(sideOffset,0.25f,0.0f,0.5f) \
 X(lookAhead,0.35f,0.0f,0.8f) \
 X(eyeHeight,-0.56f,-0.8f,0.5f) \
 X(maxDrop,0.82f,0.0f,1.0f) \
 X(maxRise,0.2f,0.0f,1.0f) \
 X(targetHeight,-0.68f,-1.0f,0.5f)
#define MCD_HUD_FIELDS(X) \
 X(right,42.0f,0.0f,1920.0f) \
 X(bottom,30.0f,0.0f,1080.0f) \
 X(size,28.0f,12.0f,96.0f) \
 X(opacity,0.9f,0.1f,1.0f)
namespace mcd {
#define MCD_MEMBER(n,d,lo,hi) float n=d;
struct DriftTuning { MCD_DRIFT_FIELDS(MCD_MEMBER) };
struct CameraTuning { bool enabled=true; MCD_CAMERA_FIELDS(MCD_MEMBER) };
struct HudTuning {bool enabled=true; MCD_HUD_FIELDS(MCD_MEMBER) };
#undef MCD_MEMBER
}
