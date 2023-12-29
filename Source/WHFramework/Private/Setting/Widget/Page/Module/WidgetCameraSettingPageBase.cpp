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
	WidgetName = FName("CameraSettingPage");

	Title = FText::FromString(TEXT("摄像机"));

	MinCameraMoveRate = 100.f;
	MaxCameraMoveRate = 1000.f;
	
	MinCameraMoveSpeed = 1.f;
	MaxCameraMoveSpeed = 10.f;

	MinCameraTurnRate = 30.f;
	MaxCameraTurnRate = 150.f;

	MinCameraLookUpRate = 30.f;
	MaxCameraLookUpRate = 150.f;

	MinCameraRotateSpeed = 1.f;
	MaxCameraRotateSpeed = 10.f;

	MinCameraZoomRate = 100.f;
	MaxCameraZoomRate = 1000.f;

	MinCameraZoomSpeed = 1.f;
	MaxCameraZoomSpeed = 10.f;
}

void UWidgetCameraSettingPageBase::OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnInitialize(InOwner, InParams);
}

void UWidgetCameraSettingPageBase::OnCreate(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);
	
	SettingItem_CameraMoveRate = CreateSubWidget<UWidgetFloatSettingItemBase>({ FText::FromString(TEXT("移动速度")), MinCameraMoveRate, MaxCameraMoveRate, 0 }, USettingModule::Get().GetFloatSettingItemClass());
	AddSettingItem(FName("CameraMoveRate"), SettingItem_CameraMoveRate, FText::FromString(TEXT("移动")));

	SettingItem_SmoothCameraMove = CreateSubWidget<UWidgetBoolSettingItemBase>({ FText::FromString(TEXT("平滑移动")) }, USettingModule::Get().GetBoolSettingItemClass());
	AddSettingItem(FName("SmoothCameraMove"), SettingItem_SmoothCameraMove, FText::FromString(TEXT("移动")));

	SettingItem_CameraMoveSpeed = CreateSubWidget<UWidgetFloatSettingItemBase>({ FText::FromString(TEXT("平滑速度")), MinCameraMoveSpeed, MaxCameraMoveSpeed, 0 }, USettingModule::Get().GetFloatSettingItemClass());
	AddSettingItem(FName("CameraMoveSpeed"), SettingItem_CameraMoveSpeed, FText::FromString(TEXT("移动")));

	SettingItem_ReverseCameraPitch = CreateSubWidget<UWidgetBoolSettingItemBase>({ FText::FromString(TEXT("垂直反转")) }, USettingModule::Get().GetBoolSettingItemClass());
	AddSettingItem(FName("ReverseCameraPitch"), SettingItem_ReverseCameraPitch, FText::FromString(TEXT("旋转")));

	SettingItem_CameraTurnRate = CreateSubWidget<UWidgetFloatSettingItemBase>({ FText::FromString(TEXT("水平速度")), MinCameraTurnRate, MaxCameraTurnRate, 0 }, USettingModule::Get().GetFloatSettingItemClass());
	AddSettingItem(FName("CameraTurnRate"), SettingItem_CameraTurnRate, FText::FromString(TEXT("旋转")));

	SettingItem_CameraLookUpRate = CreateSubWidget<UWidgetFloatSettingItemBase>({ FText::FromString(TEXT("垂直速度")), MinCameraLookUpRate, MaxCameraLookUpRate, 0 }, USettingModule::Get().GetFloatSettingItemClass());
	AddSettingItem(FName("CameraLookUpRate"), SettingItem_CameraLookUpRate, FText::FromString(TEXT("旋转")));

	SettingItem_SmoothCameraRotate = CreateSubWidget<UWidgetBoolSettingItemBase>({ FText::FromString(TEXT("平滑旋转")) }, USettingModule::Get().GetBoolSettingItemClass());
	AddSettingItem(FName("SmoothCameraRotate"), SettingItem_SmoothCameraRotate, FText::FromString(TEXT("旋转")));

	SettingItem_CameraRotateSpeed = CreateSubWidget<UWidgetFloatSettingItemBase>({ FText::FromString(TEXT("平滑速度")), MinCameraRotateSpeed, MaxCameraRotateSpeed, 0 }, USettingModule::Get().GetFloatSettingItemClass());
	AddSettingItem(FName("CameraRotateSpeed"), SettingItem_CameraRotateSpeed, FText::FromString(TEXT("旋转")));

	SettingItem_CameraZoomRate = CreateSubWidget<UWidgetFloatSettingItemBase>({ FText::FromString(TEXT("缩放速度")), MinCameraZoomRate, MaxCameraZoomRate, 0 }, USettingModule::Get().GetFloatSettingItemClass());
	AddSettingItem(FName("CameraZoomRate"), SettingItem_CameraZoomRate, FText::FromString(TEXT("缩放")));

	SettingItem_SmoothCameraZoom = CreateSubWidget<UWidgetBoolSettingItemBase>({ FText::FromString(TEXT("平滑缩放")) }, USettingModule::Get().GetBoolSettingItemClass());
	AddSettingItem(FName("SmoothCameraZoom"), SettingItem_SmoothCameraZoom, FText::FromString(TEXT("缩放")));

	SettingItem_CameraZoomSpeed = CreateSubWidget<UWidgetFloatSettingItemBase>({ FText::FromString(TEXT("平滑速度")), MinCameraZoomSpeed, MaxCameraZoomSpeed, 0 }, USettingModule::Get().GetFloatSettingItemClass());
	AddSettingItem(FName("CameraZoomSpeed"), SettingItem_CameraZoomSpeed, FText::FromString(TEXT("缩放")));
}

void UWidgetCameraSettingPageBase::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	Super::OnOpen(InParams, bInstant);

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

void UWidgetCameraSettingPageBase::OnApply()
{
	Super::OnApply();

	UCameraModule::Get().SetCameraMoveRate(SettingItem_CameraMoveRate->GetValue().GetFloatValue());
	UCameraModule::Get().SetSmoothCameraMove(SettingItem_SmoothCameraMove->GetValue().GetBooleanValue());
	UCameraModule::Get().SetCameraMoveSpeed(SettingItem_CameraMoveSpeed->GetValue().GetFloatValue());
	UCameraModule::Get().SetReverseCameraPitch(SettingItem_ReverseCameraPitch->GetValue().GetBooleanValue());
	UCameraModule::Get().SetCameraTurnRate(SettingItem_CameraTurnRate->GetValue().GetFloatValue());
	UCameraModule::Get().SetCameraLookUpRate(SettingItem_CameraLookUpRate->GetValue().GetFloatValue());
	UCameraModule::Get().SetSmoothCameraRotate(SettingItem_SmoothCameraRotate->GetValue().GetBooleanValue());
	UCameraModule::Get().SetCameraRotateSpeed(SettingItem_CameraRotateSpeed->GetValue().GetFloatValue());
	UCameraModule::Get().SetCameraZoomRate(SettingItem_CameraZoomRate->GetValue().GetFloatValue());
	UCameraModule::Get().SetSmoothCameraZoom(SettingItem_SmoothCameraZoom->GetValue().GetBooleanValue());
	UCameraModule::Get().SetCameraZoomSpeed(SettingItem_CameraZoomSpeed->GetValue().GetFloatValue());
}

void UWidgetCameraSettingPageBase::OnReset(bool bForce)
{
	Super::OnReset(bForce);

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

void UWidgetCameraSettingPageBase::OnClose(bool bInstant)
{
	Super::OnClose(bInstant);
}

bool UWidgetCameraSettingPageBase::CanApply_Implementation() const
{
	return UCameraModule::Get().GetCameraMoveRate() != SettingItem_CameraMoveRate->GetValue().GetFloatValue() ||
		UCameraModule::Get().IsSmoothCameraMove() != SettingItem_SmoothCameraMove->GetValue().GetBooleanValue() ||
		UCameraModule::Get().GetCameraMoveSpeed() != SettingItem_CameraMoveSpeed->GetValue().GetFloatValue() ||
		UCameraModule::Get().IsReverseCameraPitch() != SettingItem_ReverseCameraPitch->GetValue().GetBooleanValue() ||
		UCameraModule::Get().GetCameraTurnRate() != SettingItem_CameraTurnRate->GetValue().GetFloatValue() ||
		UCameraModule::Get().GetCameraLookUpRate() != SettingItem_CameraLookUpRate->GetValue().GetFloatValue() ||
		UCameraModule::Get().IsSmoothCameraRotate() != SettingItem_SmoothCameraRotate->GetValue().GetBooleanValue() ||
		UCameraModule::Get().GetCameraRotateSpeed() != SettingItem_CameraRotateSpeed->GetValue().GetFloatValue() ||
		UCameraModule::Get().GetCameraZoomRate() != SettingItem_CameraZoomRate->GetValue().GetFloatValue() ||
		UCameraModule::Get().IsSmoothCameraZoom() != SettingItem_SmoothCameraZoom->GetValue().GetBooleanValue() ||
		UCameraModule::Get().GetCameraZoomSpeed() != SettingItem_CameraZoomSpeed->GetValue().GetFloatValue();
}

bool UWidgetCameraSettingPageBase::CanReset_Implementation() const
{
	return UCameraModule::Get().GetCameraMoveRate() != GetDefaultSaveData()->CastRef<FCameraModuleSaveData>().CameraMoveRate ||
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
	return &USaveGameModuleStatics::GetSaveGame<USettingSaveGame>()->GetDefaultDataRef<FSettingModuleSaveData>().CameraData;
}
