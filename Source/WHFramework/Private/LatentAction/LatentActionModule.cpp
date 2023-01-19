// Fill out your copyright notice in the Description page of Project Settings.

#include "LatentAction/LatentActionModule.h"

#include "Debug/DebugModuleTypes.h"
#include "Engine/LatentActionManager.h"
#include "Engine/TargetPoint.h"
#include "Main/MainModule.h"
		
MODULE_INSTANCE_IMPLEMENTATION(ALatentActionModule, false)

ALatentActionModule::ALatentActionModule()
{
	ModuleName = FName("LatentActionModule");
}

#if WITH_EDITOR
void ALatentActionModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void ALatentActionModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void ALatentActionModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void ALatentActionModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);
}

void ALatentActionModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void ALatentActionModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void ALatentActionModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void ALatentActionModule::MoveActorTo(AActor* InActor, ATargetPoint* InTargetPoint, FTransform InTargetTransform, bool bUseRotator, bool bUseScale, float ApplicationTime, bool bEaseIn, bool bEaseOut,float BlendExp, bool bForceShortestRotationPath, EMoveActorAction::Type MoveAction, FLatentActionInfo LatentInfo)
{
	if (UWorld* World  = GEngine->GetWorldFromContextObject(InActor, EGetWorldErrorMode::LogAndReturnNull))
	{
		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		FMoveActorToAction* Action = LatentActionManager.FindExistingAction<FMoveActorToAction>(LatentInfo.CallbackTarget, LatentInfo.UUID);
	
		const FVector ActorLocation = (InActor != nullptr) ? InActor->GetActorLocation() : FVector::ZeroVector;
		const FRotator ActorRotation = (InActor != nullptr) ? InActor->GetActorRotation() : FRotator::ZeroRotator;
		const FVector ActorScale = (InActor != nullptr) ? InActor->GetActorScale() : FVector::ZeroVector;
	
		if (Action == nullptr)
		{
			if (MoveAction == EMoveActorAction::Start)
			{
				Action = new FMoveActorToAction(ApplicationTime, LatentInfo, InActor, bUseRotator, bUseScale, bEaseIn, bEaseOut, BlendExp, bForceShortestRotationPath);
	
				if (InTargetPoint)
				{
					Action->TargetLocation = InTargetPoint->GetActorLocation();
					Action->TargetRotation = InTargetPoint->GetActorRotation();
					Action->TargetScale = InTargetPoint->GetActorScale3D();
				}
				else
				{
					Action->TargetLocation = InTargetTransform.GetLocation();
					Action->TargetRotation = InTargetTransform.Rotator();
					Action->TargetScale = InTargetTransform.GetScale3D();
				}
	 			
				Action->InitialLocation = ActorLocation;
				Action->InitialRotation = ActorRotation;
				Action->InitialScale = ActorScale;
	 			
				LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, Action);
			}
		}
		else
		{
			if (MoveAction == EMoveActorAction::Start)
			{
				if(!Action->bInterpolating)
				{
					Action->TotalTime = ApplicationTime;
					Action->TimeElapsed = 0.0f;
	
					if (InTargetPoint)
					{
						Action->TargetLocation = InTargetTransform.GetLocation();
						Action->TargetRotation = InTargetTransform.Rotator();
						Action->TargetScale = InTargetTransform.GetScale3D();	
					}
					else
					{
						Action->TargetLocation = InTargetTransform.GetLocation();
						Action->TargetRotation = InTargetTransform.Rotator();
						Action->TargetScale = InTargetTransform.GetScale3D();
					}
					Action->InitialLocation = ActorLocation;
					Action->InitialRotation = ActorRotation;
					Action->InitialScale = ActorScale;
				}
				else
				{
					WHLog(LogTemp, Warning, TEXT("请求执行失败 ,因为当前的Actor正在进行插值变换"));
				}
			}
			else if (MoveAction == EMoveActorAction::Stop)
			{
				Action->bInterpolating = false;
			}
			else if (MoveAction == EMoveActorAction::Return)
			{
				Action->TotalTime = Action->TimeElapsed;
				Action->TimeElapsed = 0.0f;
	
				Action->TargetLocation = Action->InitialLocation;
				Action->TargetRotation = Action->InitialRotation;
				Action->TargetScale = Action->InitialScale;
	
				Action->InitialLocation = ActorLocation;
				Action->InitialRotation = ActorRotation;
				Action->InitialScale = ActorScale;
			}
		}
	}
}

void ALatentActionModule::RotatorActorTo(AActor* InActor, FRotator InRotator, float ApplicationTime, EMoveActorAction::Type MoveAction, FLatentActionInfo LatentInfo)
{
	if (UWorld* World  = GEngine->GetWorldFromContextObject(InActor, EGetWorldErrorMode::LogAndReturnNull))
	{
		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		FRotatorActorToAction* Action = LatentActionManager.FindExistingAction<FRotatorActorToAction>(LatentInfo.CallbackTarget, LatentInfo.UUID);
		
		const FRotator ActorRotation = (InActor != nullptr) ? InActor->GetActorRotation() : FRotator::ZeroRotator;
	
		if (Action == nullptr)
		{
			if (MoveAction == EMoveActorAction::Start)
			{
				Action = new FRotatorActorToAction(ApplicationTime, LatentInfo, InActor);

				Action->TargetRotation = InRotator;	 			
				Action->InitialRotation = ActorRotation;
	 			
				LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, Action);
			}
		}
		else
		{
			if (MoveAction == EMoveActorAction::Start)
			{
				if(!Action->bInterpolating)
				{
					Action->TotalTime = ApplicationTime;
					Action->TimeElapsed = 0.0f;
	
					Action->TargetRotation = InRotator;
					Action->InitialRotation = ActorRotation;
				}
				else
				{
					WHLog(LogTemp, Warning, TEXT("请求执行失败 ,因为当前的Actor正在进行插值变换"));
				}
			}
			
			else if (MoveAction == EMoveActorAction::Stop)
			{
				Action->bInterpolating = false;
			}
			
			else if (MoveAction == EMoveActorAction::Return)
			{
				Action->TotalTime = Action->TimeElapsed;
				Action->TimeElapsed = 0.0f;
	
				Action->TargetRotation = Action->InitialRotation;
				Action->InitialRotation = ActorRotation;
			}
		}
	}
}

void ALatentActionModule::ScaleActorTo(AActor* InActor, FVector InScale, float ApplicationTime, EMoveActorAction::Type MoveAction, FLatentActionInfo LatentInfo)
{
	if (UWorld* World  = GEngine->GetWorldFromContextObject(InActor, EGetWorldErrorMode::LogAndReturnNull))
	{
		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		FScaleActorToAction* Action = LatentActionManager.FindExistingAction<FScaleActorToAction>(LatentInfo.CallbackTarget, LatentInfo.UUID);
		
		const FVector ActorScale = (InActor != nullptr) ? InActor->GetActorScale() : FVector::OneVector;
	
		if (Action == nullptr)
		{
			if (MoveAction == EMoveActorAction::Start)
			{
				Action = new FScaleActorToAction(ApplicationTime,LatentInfo,InActor);

				Action->TargetScale = InScale;	 			
				Action->InitialScale = ActorScale;
	 			
				LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, Action);
			}
		}
		else
		{
			if (MoveAction == EMoveActorAction::Start)
			{
				if(!Action->bInterpolating)
				{
					Action->TotalTime = ApplicationTime;
					Action->TimeElapsed = 0.0f;
	
					Action->TargetScale = InScale;
					Action->InitialScale = ActorScale;
				}
				else
				{
					WHLog(LogTemp, Warning, TEXT("请求执行失败 ,因为当前的Actor正在进行插值变换"));
				}
			}
			
			else if (MoveAction == EMoveActorAction::Stop)
			{
				Action->bInterpolating = false;
			}
			
			else if (MoveAction == EMoveActorAction::Return)
			{
				Action->TotalTime = Action->TimeElapsed;
				Action->TimeElapsed = 0.0f;
	
				Action->TargetScale = Action->InitialScale;
				Action->InitialScale = ActorScale;
			}
		}
	}	
}

void ALatentActionModule::CancelableDelay(UObject* InWorldContext, float Duration, ECancelableDelayAction::Type CancelableDelayAction, FLatentActionInfo LatentInfo)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(InWorldContext, EGetWorldErrorMode::LogAndReturnNull))
	{
		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		if (LatentActionManager.FindExistingAction<FCancelableDelayAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == NULL)
		{
			if(CancelableDelayAction == ECancelableDelayAction::None)
			{
				LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FCancelableDelayAction(Duration, LatentInfo));
			}
		}
		else
		{
			if(CancelableDelayAction == ECancelableDelayAction::Cancel)
			{
				LatentActionManager.FindExistingAction<FCancelableDelayAction>(LatentInfo.CallbackTarget, LatentInfo.UUID)->bExecute = false;
			}
		}
	}
}



