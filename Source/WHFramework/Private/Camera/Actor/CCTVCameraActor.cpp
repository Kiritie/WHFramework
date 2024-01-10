// Fill out your copyright notice in the Description page of Project Settings.

#include "Camera/Actor/CCTVCameraActor.h"

#include "Camera/CameraModuleStatics.h"
#include "Camera/Point/CameraPointBase.h"
#include "Kismet/GameplayStatics.h"

ACCTVCameraActor::ACCTVCameraActor()
{
	CameraName = FName("CCTVCamera");

	ChangePointInterval = 5.f;
	ChangePointIntervalDelta = 3.f;
	
	ChangePointViewPitch = -90.f;
	ChangePointViewDistance = 1000.f;
	
	ChangePointRemainTime = 0.f;

	CameraRotateSpeed = 2.f;
	CameraViewData = FCameraViewData();
	CameraPointQueue = TArray<ACameraPointBase*>();
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
	
	ChangePointRemainTime -= DeltaSeconds;
	if(ChangePointRemainTime <= 0.f)
	{
		if(CameraPointQueue.IsEmpty())
		{
			TArray<AActor*> CameraPoints;
			UGameplayStatics::GetAllActorsOfClass(this, ACameraPointBase::StaticClass(), CameraPoints);
			for(auto Iter : CameraPoints)
			{
				CameraPointQueue.Add(Cast<ACameraPointBase>(Iter));
			}
		}
		if(CameraPointQueue.Num() > 0)
		{
			if(ACameraPointBase* CameraPoint = Cast<ACameraPointBase>(CameraPointQueue[FMath::RandRange(0, CameraPointQueue.Num() - 1)]))
			{
				CameraViewData = CameraPoint->GetCameraViewData();
				UCameraModuleStatics::SetCameraTransform(CameraPoint->GetActorLocation(), CameraViewData.CameraViewParams.CameraViewYaw, ChangePointViewPitch, ChangePointViewDistance, true);
				UCameraModuleStatics::SetCameraView(CameraViewData, false);
				CameraPointQueue.Remove(CameraPoint);
			}
		}
		ChangePointRemainTime = FMath::RandRange(ChangePointInterval - ChangePointIntervalDelta, ChangePointInterval + ChangePointIntervalDelta);
	}
	else
	{
		if(!CameraViewData.bTrackTarget)
		{
			UCameraModuleStatics::AddCameraRotationInput(CameraRotateSpeed, 0.f);
		}
	}
}

void ACCTVCameraActor::OnSwitch_Implementation()
{
	Super::OnSwitch_Implementation();

	ChangePointRemainTime = 0.f;
}

void ACCTVCameraActor::OnUnSwitch_Implementation()
{
	Super::OnUnSwitch_Implementation();

	UCameraModuleStatics::ResetCameraView(ECameraResetMode::UseCachedData);
}
