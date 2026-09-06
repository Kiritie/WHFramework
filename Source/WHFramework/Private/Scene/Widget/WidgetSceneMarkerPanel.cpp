#include "Scene/Widget/WidgetSceneMarkerPanel.h"

#include "Camera/CameraModuleStatics.h"
#include "Common/CommonModuleStatics.h"
#include "GameFramework/PlayerController.h"
#include "Scene/SceneModule.h"
#include "Scene/Capture/MiniMapCapture.h"

UWidgetSceneMarkerPanel::UWidgetSceneMarkerPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWidgetTickAble = true;
	WidgetRefreshType = EWidgetRefreshType::Tick;
}

void UWidgetSceneMarkerPanel::OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnInitialize(InOwner, InParams);
	OnRefresh();
}

void UWidgetSceneMarkerPanel::OnRefresh()
{
	Super::OnRefresh();
	if(!USceneModule::IsValid())
	{
		Markers.Reset();
		K2_OnMarkersUpdated(Markers);
		return;
	}
	Markers = USceneModule::Get().GetMarkerViews(MarkerChannel, GetMarkerViewLocation(), GetMarkerViewYaw());
	K2_OnMarkersUpdated(Markers);
}

FVector UWidgetSceneMarkerPanel::GetMarkerViewLocation() const
{
	if(MarkerChannel == ESceneMarkerChannel::Map && USceneModule::IsValid())
	{
		return FVector(USceneModule::Get().GetWorldMapCenter(), 0.f);
	}
	if(MarkerChannel == ESceneMarkerChannel::MiniMap && USceneModule::IsValid() && USceneModule::Get().GetMiniMapCapture())
	{
		return USceneModule::Get().GetMiniMapCapture()->GetActorLocation();
	}
	if(const APlayerController* PlayerController = UCommonModuleStatics::GetPlayerController())
	{
		if(const AActor* ViewTarget = PlayerController->GetViewTarget()) return ViewTarget->GetActorLocation();
	}
	return UCameraModuleStatics::GetCameraLocation(true);
}

float UWidgetSceneMarkerPanel::GetMarkerViewYaw() const
{
	if(MarkerChannel == ESceneMarkerChannel::Map) return 0.f;
	if(MarkerChannel == ESceneMarkerChannel::MiniMap && USceneModule::IsValid() && USceneModule::Get().GetMiniMapCapture())
	{
		return USceneModule::Get().GetMiniMapCapture()->GetActorRotation().Yaw;
	}
	if(const APlayerController* PlayerController = UCommonModuleStatics::GetPlayerController())
	{
		return PlayerController->GetControlRotation().Yaw;
	}
	return 0.f;
}
