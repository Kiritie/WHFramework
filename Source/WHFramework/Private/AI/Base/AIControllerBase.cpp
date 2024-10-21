// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Base/AIControllerBase.h"

#include "AI/Base/AIAgentInterface.h"
#include "AI/Base/AIBlackboardBase.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Sight.h"

AAIControllerBase::AAIControllerBase()
{
	bAttachToPawn = true;

	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
	PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AAIControllerBase::OnTargetPerceptionUpdated);

	FAISenseAffiliationFilter AffiliationFilter;
	AffiliationFilter.bDetectEnemies = true;
	AffiliationFilter.bDetectFriendlies = true;
	AffiliationFilter.bDetectNeutrals = true;

	const auto SightSenseConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightSenseConfig"));
	SightSenseConfig->SightRadius = 1000;
	SightSenseConfig->LoseSightRadius = 1200;
	SightSenseConfig->PeripheralVisionAngleDegrees = 90;
	SightSenseConfig->DetectionByAffiliation = AffiliationFilter;
	SightSenseConfig->SetMaxAge(5);
	PerceptionComponent->ConfigureSense(*SightSenseConfig);
	PerceptionComponent->SetDominantSense(*SightSenseConfig->GetSenseImplementation());

	const auto DamageSenseConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("DamageSenseConfig"));
	PerceptionComponent->ConfigureSense(*DamageSenseConfig);
}

void AAIControllerBase::OnInitialize_Implementation()
{
	bWHActorInitialized = true;
}

void AAIControllerBase::OnPreparatory_Implementation(EPhase InPhase)
{
	
}

void AAIControllerBase::OnRefresh_Implementation(float DeltaSeconds)
{
	IAIAgentInterface* OwnerAgent = GetPawn<IAIAgentInterface>();

	if(!OwnerAgent) return;

	if(IsRunningBehaviorTree())
	{
		if(CurrentBlackboard)
		{
			CurrentBlackboard->Refresh();
		}
	}
}

void AAIControllerBase::OnTermination_Implementation(EPhase InPhase)
{
	
}

void AAIControllerBase::BeginPlay()
{
	Super::BeginPlay();

	if(Execute_IsDefaultLifecycle(this))
	{
		if(!bWHActorInitialized)
		{
			Execute_OnInitialize(this);
		}
		Execute_OnPreparatory(this, EPhase::All);
	}
}

void AAIControllerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnTermination(this, EPhase::All);
	}
}

void AAIControllerBase::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	InitBehaviorTree(true);
}

void AAIControllerBase::OnUnPossess()
{
	Super::OnUnPossess();

	StopBehaviorTree();
}

void AAIControllerBase::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	const APawn* OwnerAgent = GetPawn();
	APawn* TargetAgent = Cast<APawn>(Actor);

	if (!OwnerAgent || !TargetAgent || TargetAgent == OwnerAgent) return;
	
	if (Stimulus.WasSuccessfullySensed())
	{
		if (!GetBlackboard()->GetTargetAgent())
		{
			GetBlackboard()->SetTargetAgent(TargetAgent);
		}
		else if(TargetAgent == GetBlackboard()->GetTargetAgent())
		{
			GetBlackboard()->SetIsLostTarget(false);
		}
	}
	else if(TargetAgent == GetBlackboard()->GetTargetAgent())
	{
		GetBlackboard()->SetIsLostTarget(true);
	}
}

void AAIControllerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnRefresh(this, DeltaSeconds);
	}
}

void AAIControllerBase::InitBehaviorTree(bool bAutoRun)
{
	IAIAgentInterface* OwnerAgent = GetPawn<IAIAgentInterface>();
	
	if(!OwnerAgent) return;

	UBehaviorTree* BehaviorTreeAsset = OwnerAgent->GetBehaviorTreeAsset();
	
	if(!BehaviorTreeAsset) return;

	if(SourceBehaviorTree != BehaviorTreeAsset)
	{
		SourceBehaviorTree = BehaviorTreeAsset;
		CurrentBehaviorTree = DuplicateObject<UBehaviorTree>(SourceBehaviorTree, this);

		if(SourceBlackboard != CurrentBehaviorTree->BlackboardAsset)
		{
			SourceBlackboard = Cast<UAIBlackboardBase>(CurrentBehaviorTree->BlackboardAsset);
			CurrentBlackboard = DuplicateObject<UAIBlackboardBase>(SourceBlackboard, this);

			UBlackboardComponent* BlackboardComponent;
			if(UseBlackboard(CurrentBlackboard, BlackboardComponent))
			{
				CurrentBlackboard->Initialize(BlackboardComponent);
			}
		}
		CurrentBehaviorTree->BlackboardAsset = CurrentBlackboard;
	}
	
	if(bAutoRun)
	{
		RunBehaviorTree();
	}
}

bool AAIControllerBase::RunBehaviorTree(UBehaviorTree* BTAsset)
{
	if(!IsRunningBehaviorTree())
	{
		return Super::RunBehaviorTree(BTAsset);
	}
	return true;
}

bool AAIControllerBase::RunBehaviorTree()
{
	return RunBehaviorTree(CurrentBehaviorTree);
}

void AAIControllerBase::StopBehaviorTree()
{
	if(IsRunningBehaviorTree())
	{
		GetBehaviorTreeComponent()->StopTree();
	}
}

UBehaviorTreeComponent* AAIControllerBase::GetBehaviorTreeComponent() const
{
	return Cast<UBehaviorTreeComponent>(GetBrainComponent());
}

bool AAIControllerBase::IsRunningBehaviorTree() const
{
	return GetBehaviorTreeComponent() && GetBehaviorTreeComponent()->IsRunning();
}
