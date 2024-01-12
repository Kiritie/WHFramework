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

	ChangePointInterval = 5.f;
	ChangePointIntervalDelta = 3.f;
	
	ChangePointRemainTime = 0.f;

	CameraRotateSpeed = 0.1f;
	CameraZoomSpeed = 0.05f;
	
	DefaultCameraPoints = TArray<ACameraPointBase*>();
	CameraPointQueue = TArray<ACameraPointBase*>();
	CurrentCameraViewData = FCameraViewData();
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
	
	if(ChangePointRemainTime <= 0.f)
	{
		if(CameraPointQueue.IsEmpty())
		{
			if(DefaultCameraPoints.IsEmpty())
			{
				TArray<AActor*> CameraPoints;
				UGameplayStatics::GetAllActorsOfClass(this, ACameraPointBase::StaticClass(), CameraPoints);
				for(auto Iter : CameraPoints)
				{
					CameraPointQueue.Add(Cast<ACameraPointBase>(Iter));
				}
			}
			else
			{
				CameraPointQueue = DefaultCameraPoints;
			}
			UCommonStatics::ShuffleArray(CameraPointQueue);
		}
		if(CameraPointQueue.Num() > 0)
		{
			if(ACameraPointBase* CameraPoint = CameraPointQueue[0])
			{
				CurrentCameraViewData = CameraPoint->GetCameraViewData();
				if(CurrentCameraViewData.bTrackTarget)
				{
					CurrentCameraViewData.CameraViewParams.CameraViewYaw = FMath::RandRange(0.f, 360.f);
					CurrentCameraViewData.CameraViewParams.CameraViewPitch = FMath::RandRange(-65.f, -90.f);
				}
				if(ChangePointRemainTime == -1.f)
				{
					UCameraModuleStatics::ResetCameraView(ECameraResetMode::UseDefaultPoint, true);
					UCameraModuleStatics::SetCameraView(CurrentCameraViewData, false, true);
				}
				else
				{
					UAnimationModuleStatics::ExecuteWithTransition(2.f, [this]
					{
						UCameraModuleStatics::SetCameraView(CurrentCameraViewData, false, true);
					});
				}
			}
			CameraPointQueue.RemoveAt(0);
		}
		ChangePointRemainTime = FMath::RandRange(ChangePointInterval - ChangePointIntervalDelta, ChangePointInterval + ChangePointIntervalDelta);
	}
	else
	{
		if(!CurrentCameraViewData.bTrackTarget)
		{
			UCameraModuleStatics::AddCameraRotationInput(CameraRotateSpeed, 0.f);
		}
		UCameraModuleStatics::AddCameraDistanceInput(CameraZoomSpeed);
	}
	ChangePointRemainTime -= DeltaSeconds;
}

void ACCTVCameraActor::OnSwitch_Implementation()
{
	Super::OnSwitch_Implementation();

	ChangePointRemainTime = -1.f;
}

void ACCTVCameraActor::OnUnSwitch_Implementation()
{
	Super::OnUnSwitch_Implementation();

	UCameraModuleStatics::ResetCameraView(ECameraResetMode::UseCachedData);
}
