#include "Camera/Director/AutoCameraDirector.h"
#include "Camera/Anchor/CameraShotAnchor.h"
#include "Camera/CameraModule.h"
#include "Camera/Manager/CameraManagerBase.h"

void UAutoCameraDirector::Start(ACameraManagerBase* InManager)
{
	Super::Start(InManager);
	SelectNextShot();
}

void UAutoCameraDirector::Stop()
{
	if(CameraManager && CurrentShotOverrideHandle.IsValid())
	{
		CameraManager->PopCameraConfigOverride(CurrentShotOverrideHandle);
		CurrentShotOverrideHandle.Reset();
	}

	CurrentAnchor = nullptr;
	Cooldowns.Empty();
	Super::Stop();
}

bool UAutoCameraDirector::IsOnCooldown(const ACameraShotAnchor* Anchor) const
{
	const float* Remaining = Cooldowns.Find(Anchor);
	return Remaining && *Remaining > 0.f;
}

float UAutoCameraDirector::ScoreCandidate(const ACameraShotAnchor* Anchor) const
{
	if(!Anchor || IsOnCooldown(Anchor))
	{
		return -FLT_MAX;
	}

	float Score = Anchor->BaseScore;
	if(const ACameraShotAnchor* Current = CurrentAnchor.Get())
	{
		const float Angle = FMath::Abs(FMath::FindDeltaAngleDegrees(Current->GetActorRotation().Yaw, Anchor->GetActorRotation().Yaw));
		Score += FMath::GetMappedRangeValueClamped(FVector2D(180.f, 0.f), FVector2D(-1.f, 1.f), Angle);
		if(Current == Anchor)
		{
			Score -= 2.f;
		}
	}

	return Score + FMath::FRandRange(-0.05f, 0.05f);
}

void UAutoCameraDirector::SelectNextShot()
{
	ACameraShotAnchor* Best = nullptr;
	float BestScore = -FLT_MAX;
	for(const TWeakObjectPtr<ACameraShotAnchor>& Candidate : UCameraModule::Get().GetShotAnchors())
	{
		const float Score = ScoreCandidate(Candidate.Get());
		if(Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate.Get();
		}
	}
	if(!Best || !CameraManager)
	{
		return;
	}
	if(ACameraShotAnchor* Previous = CurrentAnchor.Get())
	{
		Cooldowns.FindOrAdd(Previous) = Previous->Cooldown;
	}
	if(CurrentShotOverrideHandle.IsValid())
	{
		CameraManager->PopCameraConfigOverride(CurrentShotOverrideHandle);
		CurrentShotOverrideHandle.Reset();
	}

	CurrentAnchor = Best;
	ShotElapsed = 0.f;
	ShotDuration = FMath::FRandRange(Best->MinDuration, FMath::Max(Best->MinDuration, Best->MaxDuration));
	FCameraModeContext Context;
	Context.Anchor = Best;
	Context.Transition = FCameraTransitionParams::Smooth();
	CurrentShotOverrideHandle = CameraManager->PushCameraConfigOverride(Best->CameraOverride, 30);
	CameraManager->SetMode(Best->ModeClass, Context);
}

void UAutoCameraDirector::Update(float DeltaTime)
{
	for(auto Iter = Cooldowns.CreateIterator(); Iter; ++Iter)
	{
		Iter.Value() -= DeltaTime;
		if(Iter.Value() <= 0.f)
		{
			Iter.RemoveCurrent();
		}
	}

	ShotElapsed += DeltaTime;
	if(!CurrentAnchor.IsValid() || ShotElapsed >= ShotDuration)
	{
		SelectNextShot();
	}
}
