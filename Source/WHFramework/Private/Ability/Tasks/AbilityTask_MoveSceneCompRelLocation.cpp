// Copyright 2020 Dan Kestranek.


#include "Ability/Tasks/AbilityTask_MoveSceneCompRelLocation.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"

UAbilityTask_MoveSceneCompRelLocation::UAbilityTask_MoveSceneCompRelLocation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
	bIsFinished = false;
}

UAbilityTask_MoveSceneCompRelLocation* UAbilityTask_MoveSceneCompRelLocation::MoveSceneComponentRelativeLocation(UGameplayAbility* OwningAbility, FName TaskInstanceName, USceneComponent* SceneComponent, FVector Location, float Duration, UCurveFloat* OptionalInterpolationCurve, UCurveVector* OptionalVectorInterpolationCurve)
{
	UAbilityTask_MoveSceneCompRelLocation* MyObj = NewAbilityTask<UAbilityTask_MoveSceneCompRelLocation>(OwningAbility, TaskInstanceName);

	MyObj->Component = SceneComponent;
	MyObj->StartLocation = SceneComponent->GetRelativeLocation();
	MyObj->TargetLocation = Location;
	MyObj->DurationOfMovement = FMath::Max(Duration, 0.001f);		// Avoid negative or divide-by-zero cases
	MyObj->TimeMoveStarted = MyObj->GetWorld()->GetTimeSeconds();
	MyObj->TimeMoveWillEnd = MyObj->TimeMoveStarted + MyObj->DurationOfMovement;
	MyObj->LerpCurve = OptionalInterpolationCurve;
	MyObj->LerpCurveVector = OptionalVectorInterpolationCurve;

	return MyObj;
}

void UAbilityTask_MoveSceneCompRelLocation::Activate()
{
}

// Epic's comment
//TODO: This is still an awful way to do this and we should scrap this task or do it right.
void UAbilityTask_MoveSceneCompRelLocation::TickTask(float DeltaTime)
{
	if (bIsFinished)
	{
		return;
	}

	Super::TickTask(DeltaTime);
	AActor* MyActor = GetAvatarActor();
	if (MyActor)
	{
		if (!IsValid(Component))
		{
			EndTask();
		}

		float CurrentTime = GetWorld()->GetTimeSeconds();

		if (CurrentTime >= TimeMoveWillEnd)
		{
			bIsFinished = true;

			Component->SetRelativeLocation(TargetLocation);

			if (!bIsSimulating)
			{
				if (ShouldBroadcastAbilityTaskDelegates())
				{
					OnFinishMove.Broadcast();
				}
				EndTask();
			}
		}
		else
		{
			FVector NewLocation;
			float MoveFraction = (CurrentTime - TimeMoveStarted) / DurationOfMovement;
			if (LerpCurveVector)
			{
				const FVector ComponentInterpolationFraction = LerpCurveVector->GetVectorValue(MoveFraction);
				NewLocation = FMath::Lerp<FVector, FVector>(StartLocation, TargetLocation, ComponentInterpolationFraction);
			}
			else
			{
				if (LerpCurve)
				{
					MoveFraction = LerpCurve->GetFloatValue(MoveFraction);
				}
				NewLocation = FMath::Lerp<FVector, float>(StartLocation, TargetLocation, MoveFraction);
			}
			Component->SetRelativeLocation(NewLocation);
		}
	}
	else
	{
		bIsFinished = true;
		EndTask();
	}
}

void UAbilityTask_MoveSceneCompRelLocation::OnDestroy(bool AbilityIsEnding)
{
	// Can check bIsFinished to reset back to starting location if desired

	Super::OnDestroy(AbilityIsEnding);
}
