#include "Camera/CameraModuleTypes.h"

FCameraUserSettings FCameraModuleSaveData::ToUserSettings() const
{
	FCameraUserSettings Result;
	Result.HorizontalLookSensitivity = HorizontalLookSensitivity;
	Result.VerticalLookSensitivity = VerticalLookSensitivity;
	Result.bInvertHorizontalLook = bInvertHorizontalLook;
	Result.bInvertVerticalLook = bInvertVerticalLook;
	Result.HorizontalPanSensitivity = HorizontalPanSensitivity;
	Result.VerticalPanSensitivity = VerticalPanSensitivity;
	Result.bInvertHorizontalPan = bInvertHorizontalPan;
	Result.bInvertVerticalPan = bInvertVerticalPan;
	Result.ZoomSensitivity = ZoomSensitivity;
	Result.bSmoothMovement = bSmoothMovement;
	Result.MovementSmoothing = MovementSmoothing;
	Result.bSmoothRotation = bSmoothRotation;
	Result.RotationSmoothing = RotationSmoothing;
	Result.bSmoothZoom = bSmoothZoom;
	Result.ZoomSmoothing = ZoomSmoothing;
	return Result;
}

void FCameraModuleSaveData::FromUserSettings(const FCameraUserSettings& InSettings)
{
	HorizontalLookSensitivity = InSettings.HorizontalLookSensitivity;
	VerticalLookSensitivity = InSettings.VerticalLookSensitivity;
	bInvertHorizontalLook = InSettings.bInvertHorizontalLook;
	bInvertVerticalLook = InSettings.bInvertVerticalLook;
	HorizontalPanSensitivity = InSettings.HorizontalPanSensitivity;
	VerticalPanSensitivity = InSettings.VerticalPanSensitivity;
	bInvertHorizontalPan = InSettings.bInvertHorizontalPan;
	bInvertVerticalPan = InSettings.bInvertVerticalPan;
	ZoomSensitivity = InSettings.ZoomSensitivity;
	bSmoothMovement = InSettings.bSmoothMovement;
	MovementSmoothing = InSettings.MovementSmoothing;
	bSmoothRotation = InSettings.bSmoothRotation;
	RotationSmoothing = InSettings.RotationSmoothing;
	bSmoothZoom = InSettings.bSmoothZoom;
	ZoomSmoothing = InSettings.ZoomSmoothing;
}
