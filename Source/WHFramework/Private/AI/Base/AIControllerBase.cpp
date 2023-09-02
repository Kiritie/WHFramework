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

	FAISenseAffiliationFilter AffiliationFilter;
	AffiliationFilter.bDetectEnemies = true;
	AffiliationFilter.bDetectFriendlies = true;
	AffiliationFilter.bDetectNeutrals = true;

	auto SightSenseConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightSenseConfig"));
	SightSenseConfig->SightRadius = 1000;
	SightSenseConfig->LoseSightRadius = 1200;
	SightSenseConfig->PeripheralVisionAngleDegrees = 90;
	SightSenseConfig->DetectionByAffiliation = AffiliationFilter;
	SightSenseConfig->SetMaxAge(5);
	PerceptionComponent->ConfigureSense(*SightSenseConfig);
	PerceptionComponent->SetDominantSense(*SightSenseConfig->GetSenseImplementation());

	auto damageSenseConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("DamageSenseConfig"));
	PerceptionComponent->ConfigureSense(*damageSenseConfig);
}

void AAIControllerBase::OnInitialize_Implementation()
{
	IWHActorInterface::OnInitialize_Implementation();
}

void AAIControllerBase::OnPreparatory_Implementation(EPhase InPhase)
{
	IWHActorInterface::OnPreparatory_Implementation(InPhase);
}

void AAIControllerBase::OnRefresh_Implementation(float DeltaSeconds)
{
	IWHActorInterface::OnRefresh_Implementation(DeltaSeconds);
	
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

void AAIControllerBase::OnTermination_Implementation(EPhase InPhase)
{
	IWHActorInterface::OnTermination_Implementation(InPhase);
}

void AAIControllerBase::BeginPlay()
{
	Super::BeginPlay();

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnInitialize(this);
		Execute_OnPreparatory(this, EPhase::None);
	}
}

void AAIControllerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnTermination(this, EPhase::None);
	}
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

void AAIControllerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnRefresh(this, DeltaSeconds);
	}
}

void AAIControllerBase::InitBehaviorTree(UBehaviorTree* InBehaviorTreeAsset)
{
	if(BehaviorTreeAsset) return;
	
	BehaviorTreeAsset = DuplicateObject<UBehaviorTree>(InBehaviorTreeAsset, this);
	if(BehaviorTreeAsset)
	{
		BlackboardAsset = DuplicateObject<UAIBlackboardBase>(Cast<UAIBlackboardBase>(InBehaviorTreeAsset->BlackboardAsset), nullptr);
		if(BlackboardAsset)
		{
			BehaviorTreeAsset->BlackboardAsset = BlackboardAsset;
		}
	}
}

bool AAIControllerBase::RunBehaviorTree(UBehaviorTree* BTAsset)
{
	const bool bSuccess = Super::RunBehaviorTree(BTAsset);
	if(BlackboardAsset)
	{
		BlackboardAsset->Initialize(GetBlackboardComponent(), GetPawn<ACharacterBase>());
	}
	return bSuccess;
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
