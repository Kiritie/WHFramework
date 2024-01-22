// Fill out your copyright notice in the Description page of Project Settings.

#include "Camera/Actor/CCTVCameraActor.h"

#include "Animation/AnimationModuleStatics.h"
#include "Camera/CameraModuleStatics.h"
#include "Camera/Point/CameraPointBase.h"
#include "Common/CommonStatics.h"
#include "Kismet/GameplayStatics.h"

ACCTVCameraActor::ACCTVCameraActor()
{
	CameraName = FName("CCTVCamera");

	CameraShotMinInterval = 7.f;
	CameraShotMaxInterval = 10.f;
	
	CameraShotRemainTime = 0.f;

	CameraShotMinPitch = -30.f;
	CameraShotMaxPitch = -90.f;

	CameraShotZoomSpeed = 0.05f;
	
	CameraShotRotateSpeed = 0.1f;

	CameraShotFadeTime = 2.f;
	
	DefaultCameraShotPoints = TArray<ACameraPointBase*>();
	CurrentCameraShotPoints = TArray<ACameraPointBase*>();
	CurrentCameraShotData = FCCTVCameraShotData();
}

void ACCTVCameraActor::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void ACCTVCameraActor::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);
}

void ACCTVCameraActor::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	if(!IsCurrent()) return;
	
	if(CameraShotRemainTime <= 0.f)
	{
		if(CurrentCameraShotPoints.IsEmpty())
		{
			if(DefaultCameraShotPoints.IsEmpty())
			{
				TArray<AActor*> CameraPoints;
				UGameplayStatics::GetAllActorsOfClass(this, ACameraPointBase::StaticClass(), CameraPoints);
				for(auto Iter : CameraPoints)
				{
					if(ACameraPointBase* CameraPoint = Cast<ACameraPointBase>(Iter))
					{
						if(!CameraPoint->IsDefault())
						{
							CurrentCameraShotPoints.Add(CameraPoint);
						}
					}
				}
			}
			else
			{
				CurrentCameraShotPoints = DefaultCameraShotPoints;
			}
			UCommonStatics::ShuffleArray(CurrentCameraShotPoints);
		}
		if(CurrentCameraShotPoints.Num() > 0)
		{
			if(ACameraPointBase* CameraPoint = CurrentCameraShotPoints[0])
			{
				CurrentCameraShotData.CameraViewData = CameraPoint->GetCameraViewData();
				CurrentCameraShotData.CameraShotMode = ECCTVCameraShotMode::None;
				CurrentCameraShotData.CameraViewData.CameraViewParams.CameraViewYaw = FMath::RandRange(0.f, 360.f);
				CurrentCameraShotData.CameraViewData.CameraViewParams.CameraViewPitch = FMath::RandRange(CameraShotMinPitch, CameraShotMaxPitch);
				CurrentCameraShotData.CameraMinDistance = CurrentCameraShotData.CameraViewData.CameraViewParams.CameraViewDistance * 0.5f;
				CurrentCameraShotData.CameraMaxDistance = CurrentCameraShotData.CameraViewData.CameraViewParams.CameraViewDistance * 2.5f;
				if(CurrentCameraShotData.CameraViewData.TrackTargetMode > ECameraTrackMode::LocationOnly)
				{
					CurrentCameraShotData.CameraViewData.TrackTargetMode = ECameraTrackMode::LocationOnly;
				}
				UAnimationModuleStatics::ExecuteWithTransition(CameraShotFadeTime, [this]
				{
					CurrentCameraShotData.CameraShotMode = (ECCTVCameraShotMode)FMath::RandRange(2, 4);
					switch(CurrentCameraShotData.CameraShotMode)
					{
						case ECCTVCameraShotMode::Zoom:
						{
							CurrentCameraShotData.CameraZoomSpeed = FMath::RandBool() ? -CameraShotZoomSpeed : CameraShotZoomSpeed;
							CurrentCameraShotData.CameraViewData.CameraViewParams.CameraViewDistance = CurrentCameraShotData.CameraZoomSpeed > 0.f ? CurrentCameraShotData.CameraMinDistance : CurrentCameraShotData.CameraMaxDistance;
							break;
						}
						case ECCTVCameraShotMode::Rotate:
						{
							CurrentCameraShotData.CameraRotateSpeed = FMath::RandBool() ? -CameraShotRotateSpeed : CameraShotRotateSpeed;
							CurrentCameraShotData.CameraViewData.CameraViewParams.CameraViewDistance = FMath::RandRange(CurrentCameraShotData.CameraMinDistance, CurrentCameraShotData.CameraMaxDistance);
							break;
						}
						case ECCTVCameraShotMode::ZoomAndRotate:
						{
							CurrentCameraShotData.CameraZoomSpeed = FMath::RandBool() ? -CameraShotZoomSpeed : CameraShotZoomSpeed;
							CurrentCameraShotData.CameraRotateSpeed = FMath::RandBool() ? -CameraShotRotateSpeed : CameraShotRotateSpeed;
							CurrentCameraShotData.CameraViewData.CameraViewParams.CameraViewDistance = CurrentCameraShotData.CameraZoomSpeed > 0.f ? CurrentCameraShotData.CameraMinDistance : CurrentCameraShotData.CameraMaxDistance;
							break;
						}
						default: break;
					}
					UCameraModuleStatics::SetCameraView(CurrentCameraShotData.CameraViewData, false, true);
					if(CurrentCameraShotData.CameraViewData.bTrackTarget)
					{
						UCameraModuleStatics::SetCameraRotationAndDistance(CurrentCameraShotData.CameraViewData.CameraViewParams.CameraViewYaw, CurrentCameraShotData.CameraViewData.CameraViewParams.CameraViewPitch, CurrentCameraShotData.CameraViewData.CameraViewParams.CameraViewDistance, true);
					}
				});
			}
			CurrentCameraShotPoints.RemoveAt(0);
		}
		CameraShotRemainTime = FMath::RandRange(CameraShotMinInterval, CameraShotMaxInterval);
	}
	else
	{
		switch(CurrentCameraShotData.CameraShotMode)
		{
			case ECCTVCameraShotMode::Zoom:
			{
				if(CurrentCameraShotData.CameraZoomSpeed < 0.f ? UCameraModuleStatics::GetCameraDistance() > CurrentCameraShotData.CameraMinDistance : UCameraModuleStatics::GetCameraDistance() < CurrentCameraShotData.CameraMaxDistance)
				{
					UCameraModuleStatics::AddCameraDistanceInput(CurrentCameraShotData.CameraZoomSpeed);
				}
				break;
			}
			case ECCTVCameraShotMode::Rotate:
			{
				UCameraModuleStatics::AddCameraRotationInput(CurrentCameraShotData.CameraRotateSpeed, 0.f);
				break;
			}
			case ECCTVCameraShotMode::ZoomAndRotate:
			{
				if(CurrentCameraShotData.CameraZoomSpeed < 0.f ? UCameraModuleStatics::GetCameraDistance() > CurrentCameraShotData.CameraMinDistance : UCameraModuleStatics::GetCameraDistance() < CurrentCameraShotData.CameraMaxDistance)
				{
					UCameraModuleStatics::AddCameraDistanceInput(CurrentCameraShotData.CameraZoomSpeed);
				}
				UCameraModuleStatics::AddCameraRotationInput(CurrentCameraShotData.CameraRotateSpeed, 0.f);
				break;
			}
			default: break;
		}
	}
	CameraShotRemainTime -= DeltaSeconds;
}

void ACCTVCameraActor::OnSwitch_Implementation()
{
	Super::OnSwitch_Implementation();

	CameraShotRemainTime = -1.f;
}

void ACCTVCameraActor::OnUnSwitch_Implementation()
{
	Super::OnUnSwitch_Implementation();

	UCameraModuleStatics::EndTrackTarget();
}
