// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Base/AIControllerBase.h"

#include "AI/Base/AIAgentInterface.h"
#include "AI/Base/AIBlackboardBase.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Character/Base/CharacterBase.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Character/Base/CharacterBase.h"

AAIControllerBase::AAIControllerBase()
{
	bAttachToPawn = true;

	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
	PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AAIControllerBase::OnTargetPerceptionUpdated);

	FAISenseAffiliationFilter affiliationFilter;
	affiliationFilter.bDetectEnemies = true;
	affiliationFilter.bDetectFriendlies = true;
	affiliationFilter.bDetectNeutrals = true;

	auto sightSenseConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightSenseConfig"));
	sightSenseConfig->SightRadius = 1000;
	sightSenseConfig->LoseSightRadius = 1200;
	sightSenseConfig->PeripheralVisionAngleDegrees = 90;
	sightSenseConfig->DetectionByAffiliation = affiliationFilter;
	sightSenseConfig->SetMaxAge(5);
	PerceptionComponent->ConfigureSense(*sightSenseConfig);
	PerceptionComponent->SetDominantSense(*sightSenseConfig->GetSenseImplementation());

	auto damageSenseConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("DamageSenseConfig"));
	PerceptionComponent->ConfigureSense(*damageSenseConfig);
}

void AAIControllerBase::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if(const IAIAgentInterface* AIAgent = Cast<IAIAgentInterface>(InPawn))
	{
		InitBehaviorTree(AIAgent->GetBehaviorTreeAsset());
		RunBehaviorTree(BehaviorTreeAsset);
	}
}

void AAIControllerBase::OnUnPossess()
{
	Super::OnUnPossess();

	StopBehaviorTree();
}

void AAIControllerBase::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	ACharacterBase* OwnerCharacter = GetPawn<ACharacterBase>();
	ACharacterBase* TargetCharacter = Cast<ACharacterBase>(Actor);

	if (!OwnerCharacter || !TargetCharacter || TargetCharacter == OwnerCharacter) return;
	
	if (Stimulus.WasSuccessfullySensed())
	{
		if (!GetBlackboard()->GetTargetCharacter())
		{
			GetBlackboard()->SetTargetCharacter(TargetCharacter);
			GetBlackboard()->SetIsLostTarget(false);
		}
		else if(TargetCharacter == GetBlackboard()->GetTargetCharacter())
		{
			GetBlackboard()->SetIsLostTarget(false);
		}
	}
	else if(TargetCharacter == GetBlackboard()->GetTargetCharacter())
	{
		GetBlackboard()->SetIsLostTarget(true);
	}
}

void AAIControllerBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	ACharacterBase* OwnerCharacter = GetPawn<ACharacterBase>();

	if(!OwnerCharacter) return;

	if(IsRunningBehaviorTree())
	{
		if(BlackboardAsset)
		{
			BlackboardAsset->Refresh();
		}
	}
}

void AAIControllerBase::InitBehaviorTree(UBehaviorTree* InBehaviorTreeAsset)
{
	BehaviorTreeAsset = DuplicateObject<UBehaviorTree>(InBehaviorTreeAsset, this);
	if(BehaviorTreeAsset)
	{
		BlackboardAsset = DuplicateObject<UAIBlackboardBase>(Cast<UAIBlackboardBase>(InBehaviorTreeAsset->BlackboardAsset), nullptr);
		BehaviorTreeAsset->BlackboardAsset = BlackboardAsset;
	}
}

bool AAIControllerBase::RunBehaviorTree(UBehaviorTree* BTAsset)
{
	const bool ReturnValue = Super::RunBehaviorTree(BTAsset);
	if(BlackboardAsset)
	{
		BlackboardAsset->Initialize(GetBlackboardComponent(), GetPawn<ACharacterBase>());
	}
	return ReturnValue;
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
