// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/Widget/Page/Module/WidgetCameraSettingPageBase.h"

#include "Camera/CameraModule.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Module/SettingSaveGame.h"
#include "Setting/SettingModule.h"
#include "Setting/Widget/Item/WidgetBoolSettingItemBase.h"
#include "Setting/Widget/Item/WidgetFloatSettingItemBase.h"
#include "Widget/WidgetModuleStatics.h"

UWidgetCameraSettingPageBase::UWidgetCameraSettingPageBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Title = NSLOCTEXT("WH.WidgetCameraSettingPageBase", "Camera", "摄像机");

	MinCameraMoveRate = 100.f;
	MaxCameraMoveRate = 1000.f;
	
	MinCameraMoveSpeed = 1.f;
	MaxCameraMoveSpeed = 10.f;

	MinCameraTurnRate = 5.f;
	MaxCameraTurnRate = 90.f;

	MinCameraLookUpRate = 5.f;
	MaxCameraLookUpRate = 90.f;

	MinCameraRotateSpeed = 1.f;
	MaxCameraRotateSpeed = 10.f;

	MinCameraZoomRate = 100.f;
	MaxCameraZoomRate = 1000.f;

	MinCameraZoomSpeed = 1.f;
	MaxCameraZoomSpeed = 10.f;
}

void UWidgetCameraSettingPageBase::OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);
	
	SettingItem_EnableCameraPanZMove = UObjectPoolModuleStatics::SpawnObject<UWidgetBoolSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetCameraSettingPageBase", "VerticalPanning", "垂直平移") }, USettingModule::Get().GetBoolSettingItemClass());
	SettingItem_EnableCameraPanZMove->SetValue(UCameraModule::Get().IsEnableCameraPanZMove());
	AddSettingItem(FName("EnableCameraPanZMove"), SettingItem_EnableCameraPanZMove, NSLOCTEXT("WH.WidgetCameraSettingPageBase", "Movement", "移动"));
	
	SettingItem_ReverseCameraPanMove = UObjectPoolModuleStatics::SpawnObject<UWidgetBoolSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetCameraSettingPageBase", "InvertPanning", "平移反转") }, USettingModule::Get().GetBoolSettingItemClass());
	SettingItem_ReverseCameraPanMove->SetValue(UCameraModule::Get().IsReverseCameraPanMove());
	AddSettingItem(FName("ReverseCameraPanMove"), SettingItem_ReverseCameraPanMove, NSLOCTEXT("WH.WidgetCameraSettingPageBase", "Movement", "移动"));

	SettingItem_CameraMoveRate = UObjectPoolModuleStatics::SpawnObject<UWidgetFloatSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetCameraSettingPageBase", "MovementSpeed", "移动速度"), MinCameraMoveRate, MaxCameraMoveRate, 0 }, USettingModule::Get().GetFloatSettingItemClass());
	SettingItem_CameraMoveRate->SetValue(UCameraModule::Get().GetCameraMoveRate());
	AddSettingItem(FName("CameraMoveRate"), SettingItem_CameraMoveRate, NSLOCTEXT("WH.WidgetCameraSettingPageBase", "Movement", "移动"));

	SettingItem_SmoothCameraMove = UObjectPoolModuleStatics::SpawnObject<UWidgetBoolSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetCameraSettingPageBase", "SmoothMovement", "平滑移动") }, USettingModule::Get().GetBoolSettingItemClass());
	SettingItem_SmoothCameraMove->SetValue(UCameraModule::Get().IsSmoothCameraMove());
	AddSettingItem(FName("SmoothCameraMove"), SettingItem_SmoothCameraMove, NSLOCTEXT("WH.WidgetCameraSettingPageBase", "Movement", "移动"));

	SettingItem_CameraMoveSpeed = UObjectPoolModuleStatics::SpawnObject<UWidgetFloatSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetCameraSettingPageBase", "SmoothingSpeed", "平滑速率"), MinCameraMoveSpeed, MaxCameraMoveSpeed, 0 }, USettingModule::Get().GetFloatSettingItemClass());
	SettingItem_CameraMoveSpeed->SetValue(UCameraModule::Get().GetCameraMoveSpeed());
	AddSettingItem(FName("CameraMoveSpeed"), SettingItem_CameraMoveSpeed, NSLOCTEXT("WH.WidgetCameraSettingPageBase", "Movement", "移动"));

	SettingItem_ReverseCameraPitch = UObjectPoolModuleStatics::SpawnObject<UWidgetBoolSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetCameraSettingPageBase", "InvertVerticalLook", "垂直反转") }, USettingModule::Get().GetBoolSettingItemClass());
	SettingItem_ReverseCameraPitch->SetValue(UCameraModule::Get().IsReverseCameraPitch());
	AddSettingItem(FName("ReverseCameraPitch"), SettingItem_ReverseCameraPitch, NSLOCTEXT("WH.WidgetCameraSettingPageBase", "Rotation", "旋转"));

	SettingItem_CameraTurnRate = UObjectPoolModuleStatics::SpawnObject<UWidgetFloatSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetCameraSettingPageBase", "HorizontalSensitivity", "水平速度"), MinCameraTurnRate, MaxCameraTurnRate, 0 }, USettingModule::Get().GetFloatSettingItemClass());
	SettingItem_CameraTurnRate->SetValue(UCameraModule::Get().GetCameraTurnRate());
	AddSettingItem(FName("CameraTurnRate"), SettingItem_CameraTurnRate, NSLOCTEXT("WH.WidgetCameraSettingPageBase", "Rotation", "旋转"));

	SettingItem_CameraLookUpRate = UObjectPoolModuleStatics::SpawnObject<UWidgetFloatSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetCameraSettingPageBase", "VerticalSensitivity", "垂直速度"), MinCameraLookUpRate, MaxCameraLookUpRate, 0 }, USettingModule::Get().GetFloatSettingItemClass());
	SettingItem_CameraLookUpRate->SetValue(UCameraModule::Get().GetCameraLookUpRate());
	AddSettingItem(FName("CameraLookUpRate"), SettingItem_CameraLookUpRate, NSLOCTEXT("WH.WidgetCameraSettingPageBase", "Rotation", "旋转"));

	SettingItem_SmoothCameraRotate = UObjectPoolModuleStatics::SpawnObject<UWidgetBoolSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetCameraSettingPageBase", "SmoothRotation", "平滑旋转") }, USettingModule::Get().GetBoolSettingItemClass());
	SettingItem_SmoothCameraRotate->SetValue(UCameraModule::Get().IsSmoothCameraRotate());
	AddSettingItem(FName("SmoothCameraRotate"), SettingItem_SmoothCameraRotate, NSLOCTEXT("WH.WidgetCameraSettingPageBase", "Rotation", "旋转"));

	SettingItem_CameraRotateSpeed = UObjectPoolModuleStatics::SpawnObject<UWidgetFloatSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetCameraSettingPageBase", "SmoothingSpeed", "平滑速率"), MinCameraRotateSpeed, MaxCameraRotateSpeed, 0 }, USettingModule::Get().GetFloatSettingItemClass());
	SettingItem_CameraRotateSpeed->SetValue(UCameraModule::Get().GetCameraRotateSpeed());
	AddSettingItem(FName("CameraRotateSpeed"), SettingItem_CameraRotateSpeed, NSLOCTEXT("WH.WidgetCameraSettingPageBase", "Rotation", "旋转"));

	SettingItem_CameraZoomRate = UObjectPoolModuleStatics::SpawnObject<UWidgetFloatSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetCameraSettingPageBase", "ZoomSpeed", "缩放速度"), MinCameraZoomRate, MaxCameraZoomRate, 0 }, USettingModule::Get().GetFloatSettingItemClass());
	SettingItem_CameraZoomRate->SetValue(UCameraModule::Get().GetCameraZoomRate());
	AddSettingItem(FName("CameraZoomRate"), SettingItem_CameraZoomRate, NSLOCTEXT("WH.WidgetCameraSettingPageBase", "Zoom", "缩放"));

	SettingItem_SmoothCameraZoom = UObjectPoolModuleStatics::SpawnObject<UWidgetBoolSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetCameraSettingPageBase", "SmoothZoom", "平滑缩放") }, USettingModule::Get().GetBoolSettingItemClass());
	SettingItem_SmoothCameraZoom->SetValue(UCameraModule::Get().IsSmoothCameraZoom());
	AddSettingItem(FName("SmoothCameraZoom"), SettingItem_SmoothCameraZoom, NSLOCTEXT("WH.WidgetCameraSettingPageBase", "Zoom", "缩放"));

	SettingItem_CameraZoomSpeed = UObjectPoolModuleStatics::SpawnObject<UWidgetFloatSettingItemBase>(nullptr, { NSLOCTEXT("WH.WidgetCameraSettingPageBase", "SmoothingSpeed", "平滑速率"), MinCameraZoomSpeed, MaxCameraZoomSpeed, 0 }, USettingModule::Get().GetFloatSettingItemClass());
	SettingItem_CameraZoomSpeed->SetValue(UCameraModule::Get().GetCameraZoomSpeed());
	AddSettingItem(FName("CameraZoomSpeed"), SettingItem_CameraZoomSpeed, NSLOCTEXT("WH.WidgetCameraSettingPageBase", "Zoom", "缩放"));
}

void UWidgetCameraSettingPageBase::OnApply()
{
	Super::OnApply();

	UCameraModule::Get().SetEnableCameraPanZMove(SettingItem_EnableCameraPanZMove->GetValue().Get<bool>());
	UCameraModule::Get().SetReverseCameraPanMove(SettingItem_ReverseCameraPanMove->GetValue().Get<bool>());
	UCameraModule::Get().SetCameraMoveRate(SettingItem_CameraMoveRate->GetValue().Get<float>());
	UCameraModule::Get().SetSmoothCameraMove(SettingItem_SmoothCameraMove->GetValue().Get<bool>());
	UCameraModule::Get().SetCameraMoveSpeed(SettingItem_CameraMoveSpeed->GetValue().Get<float>());
	UCameraModule::Get().SetReverseCameraPitch(SettingItem_ReverseCameraPitch->GetValue().Get<bool>());
	UCameraModule::Get().SetCameraTurnRate(SettingItem_CameraTurnRate->GetValue().Get<float>());
	UCameraModule::Get().SetCameraLookUpRate(SettingItem_CameraLookUpRate->GetValue().Get<float>());
	UCameraModule::Get().SetSmoothCameraRotate(SettingItem_SmoothCameraRotate->GetValue().Get<bool>());
	UCameraModule::Get().SetCameraRotateSpeed(SettingItem_CameraRotateSpeed->GetValue().Get<float>());
	UCameraModule::Get().SetCameraZoomRate(SettingItem_CameraZoomRate->GetValue().Get<float>());
	UCameraModule::Get().SetSmoothCameraZoom(SettingItem_SmoothCameraZoom->GetValue().Get<bool>());
	UCameraModule::Get().SetCameraZoomSpeed(SettingItem_CameraZoomSpeed->GetValue().Get<float>());
}

void UWidgetCameraSettingPageBase::NativeOnActivated()
{
	Super::NativeOnActivated();
}

void UWidgetCameraSettingPageBase::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	SettingItem_EnableCameraPanZMove->SetValue(UCameraModule::Get().IsEnableCameraPanZMove());
	SettingItem_ReverseCameraPanMove->SetValue(UCameraModule::Get().IsReverseCameraPanMove());
	SettingItem_CameraMoveRate->SetValue(UCameraModule::Get().GetCameraMoveRate());
	SettingItem_SmoothCameraMove->SetValue(UCameraModule::Get().IsSmoothCameraMove());
	SettingItem_CameraMoveSpeed->SetValue(UCameraModule::Get().GetCameraMoveSpeed());
	SettingItem_ReverseCameraPitch->SetValue(UCameraModule::Get().IsReverseCameraPitch());
	SettingItem_CameraTurnRate->SetValue(UCameraModule::Get().GetCameraTurnRate());
	SettingItem_CameraLookUpRate->SetValue(UCameraModule::Get().GetCameraLookUpRate());
	SettingItem_SmoothCameraRotate->SetValue(UCameraModule::Get().IsSmoothCameraRotate());
	SettingItem_CameraRotateSpeed->SetValue(UCameraModule::Get().GetCameraRotateSpeed());
	SettingItem_CameraZoomRate->SetValue(UCameraModule::Get().GetCameraZoomRate());
	SettingItem_SmoothCameraZoom->SetValue(UCameraModule::Get().IsSmoothCameraZoom());
	SettingItem_CameraZoomSpeed->SetValue(UCameraModule::Get().GetCameraZoomSpeed());
}

void UWidgetCameraSettingPageBase::OnReset(bool bForce)
{
	Super::OnReset(bForce);

	SettingItem_EnableCameraPanZMove->SetValue(GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().bEnableCameraPanZMove);
	SettingItem_ReverseCameraPanMove->SetValue(GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().bReverseCameraPanMove);
	SettingItem_CameraMoveRate->SetValue(GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().CameraMoveRate);
	SettingItem_SmoothCameraMove->SetValue(GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().bSmoothCameraMove);
	SettingItem_CameraMoveSpeed->SetValue(GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().CameraMoveSpeed);
	SettingItem_ReverseCameraPitch->SetValue(GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().bReverseCameraPitch);
	SettingItem_CameraTurnRate->SetValue(GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().CameraTurnRate);
	SettingItem_CameraLookUpRate->SetValue(GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().CameraLookUpRate);
	SettingItem_SmoothCameraRotate->SetValue(GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().bSmoothCameraRotate);
	SettingItem_CameraRotateSpeed->SetValue(GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().CameraRotateSpeed);
	SettingItem_CameraZoomRate->SetValue(GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().CameraZoomRate);
	SettingItem_SmoothCameraZoom->SetValue(GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().bSmoothCameraZoom);
	SettingItem_CameraZoomSpeed->SetValue(GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().CameraZoomSpeed);
}

void UWidgetCameraSettingPageBase::OnValueChange(UWidgetSettingItemBase* InSettingItem, const FParameter& InValue)
{
	Super::OnValueChange(InSettingItem, InValue);

	if(InSettingItem == SettingItem_SmoothCameraMove)
	{
		SettingItem_CameraMoveSpeed->SetVisible(InSettingItem->GetValue().Get<bool>());
	}
	else if(InSettingItem == SettingItem_SmoothCameraRotate)
	{
		SettingItem_CameraRotateSpeed->SetVisible(InSettingItem->GetValue().Get<bool>());
	}
	else if(InSettingItem == SettingItem_SmoothCameraZoom)
	{
		SettingItem_CameraZoomSpeed->SetVisible(InSettingItem->GetValue().Get<bool>());
	}
}

bool UWidgetCameraSettingPageBase::CanApply_Implementation() const
{
	return UCameraModule::Get().IsEnableCameraPanZMove() != SettingItem_EnableCameraPanZMove->GetValue().Get<bool>() ||
		UCameraModule::Get().IsReverseCameraPanMove() != SettingItem_ReverseCameraPanMove->GetValue().Get<bool>() ||
		UCameraModule::Get().GetCameraMoveRate() != SettingItem_CameraMoveRate->GetValue().Get<float>() ||
		UCameraModule::Get().IsSmoothCameraMove() != SettingItem_SmoothCameraMove->GetValue().Get<bool>() ||
		UCameraModule::Get().GetCameraMoveSpeed() != SettingItem_CameraMoveSpeed->GetValue().Get<float>() ||
		UCameraModule::Get().IsReverseCameraPitch() != SettingItem_ReverseCameraPitch->GetValue().Get<bool>() ||
		UCameraModule::Get().GetCameraTurnRate() != SettingItem_CameraTurnRate->GetValue().Get<float>() ||
		UCameraModule::Get().GetCameraLookUpRate() != SettingItem_CameraLookUpRate->GetValue().Get<float>() ||
		UCameraModule::Get().IsSmoothCameraRotate() != SettingItem_SmoothCameraRotate->GetValue().Get<bool>() ||
		UCameraModule::Get().GetCameraRotateSpeed() != SettingItem_CameraRotateSpeed->GetValue().Get<float>() ||
		UCameraModule::Get().GetCameraZoomRate() != SettingItem_CameraZoomRate->GetValue().Get<float>() ||
		UCameraModule::Get().IsSmoothCameraZoom() != SettingItem_SmoothCameraZoom->GetValue().Get<bool>() ||
		UCameraModule::Get().GetCameraZoomSpeed() != SettingItem_CameraZoomSpeed->GetValue().Get<float>();
}

bool UWidgetCameraSettingPageBase::CanReset_Implementation() const
{
	return UCameraModule::Get().IsEnableCameraPanZMove() != GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().bEnableCameraPanZMove ||
		UCameraModule::Get().IsReverseCameraPanMove() != GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().bReverseCameraPanMove ||
		UCameraModule::Get().GetCameraMoveRate() != GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().CameraMoveRate ||
		UCameraModule::Get().IsSmoothCameraMove() != GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().bSmoothCameraMove ||
		UCameraModule::Get().GetCameraMoveSpeed() != GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().CameraMoveSpeed ||
		UCameraModule::Get().IsReverseCameraPitch() != GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().bReverseCameraPitch ||
		UCameraModule::Get().GetCameraTurnRate() != GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().CameraTurnRate ||
		UCameraModule::Get().GetCameraLookUpRate() != GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().CameraLookUpRate ||
		UCameraModule::Get().IsSmoothCameraRotate() != GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().bSmoothCameraRotate ||
		UCameraModule::Get().GetCameraRotateSpeed() != GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().CameraRotateSpeed ||
		UCameraModule::Get().GetCameraZoomRate() != GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().CameraZoomRate ||
		UCameraModule::Get().IsSmoothCameraZoom() != GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().bSmoothCameraZoom ||
		UCameraModule::Get().GetCameraZoomSpeed() != GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().CameraZoomSpeed;
}

FSaveData* UWidgetCameraSettingPageBase::GetDefaultSaveData() const
{
	return &USaveGameModuleStatics::GetOrCreateSaveGame<USettingSaveGame>()->GetDefaultDataRef<FSettingModuleSaveData>().CameraData;
}
