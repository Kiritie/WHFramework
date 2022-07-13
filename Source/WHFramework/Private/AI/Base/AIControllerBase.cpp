// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Base/AIControllerBase.h"

#include "AI/Base/AIAgentInterface.h"
#include "AI/Base/AIBlackboardBase.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Character/Base/CharacterBase.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Sight.h"

AAIControllerBase::AAIControllerBase()
{
	bAttachToPawn = true;

	AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
	AIPerception->OnTargetPerceptionUpdated.AddDynamic(this, &AAIControllerBase::OnTargetPerceptionUpdated);

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
	AIPerception->ConfigureSense(*sightSenseConfig);
	AIPerception->SetDominantSense(*sightSenseConfig->GetSenseImplementation());

	const auto damageSenseConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("DamageSenseConfig"));
	AIPerception->ConfigureSense(*damageSenseConfig);
}

void AAIControllerBase::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if(const IAIAgentInterface* AIAgent = Cast<IAIAgentInterface>(InPawn))
	{
		InitBehaviorTree(AIAgent->GetBehaviorTreeAsset(), Cast<ACharacterBase>(InPawn));
	
		RunBehaviorTree();
	}
}

void AAIControllerBase::OnUnPossess()
{
	Super::OnUnPossess();

	StopBehaviorTree();
}

void AAIControllerBase::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!GetPawn<ACharacterBase>()) return;
}

void AAIControllerBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

bool AAIControllerBase::RunBehaviorTree()
{
	return RunBehaviorTree(BehaviorTreeAsset);
}

bool AAIControllerBase::RunBehaviorTree(UBehaviorTree* BTAsset)
{
	return Super::RunBehaviorTree(BTAsset);
}

void AAIControllerBase::InitBehaviorTree(UBehaviorTree* InBehaviorTreeAsset, ACharacterBase* InCharacter)
{
	BehaviorTreeAsset = DuplicateObject<UBehaviorTree>(InBehaviorTreeAsset, this);
	if(BehaviorTreeAsset)
	{
		BlackboardAsset = DuplicateObject<UAIBlackboardBase>(Cast<UAIBlackboardBase>(InBehaviorTreeAsset->BlackboardAsset), nullptr);
		BlackboardAsset->Initialize(GetBlackboardComponent(), InCharacter);
		BehaviorTreeAsset->BlackboardAsset = BlackboardAsset;
	}
}

void AAIControllerBase::StopBehaviorTree()
{
	if (GetBehaviorTreeComponent() && GetBehaviorTreeComponent()->IsRunning())
	{
		GetBehaviorTreeComponent()->StopTree();
	}
}

UBehaviorTreeComponent* AAIControllerBase::GetBehaviorTreeComponent() const
{
	return Cast<UBehaviorTreeComponent>(GetBrainComponent());
}
