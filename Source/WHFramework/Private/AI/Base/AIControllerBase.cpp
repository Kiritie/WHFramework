#include "AI/Base/AIControllerBase.h"

#include "AI/Base/AIAgentInterface.h"
#include "AI/Base/AIBlackboardBase.h"
#include "AI/Base/AIBlackboardContext.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Sight.h"

AAIControllerBase::AAIControllerBase()
{
	bAttachToPawn = true;
	PrimaryActorTick.bCanEverTick = true;
	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
	PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AAIControllerBase::OnTargetPerceptionUpdated);
	UAISenseConfig_Sight* Sight = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightSenseConfig"));
	Sight->SightRadius = 1000.f;
	Sight->LoseSightRadius = 1200.f;
	Sight->PeripheralVisionAngleDegrees = 90.f;
	Sight->DetectionByAffiliation.bDetectEnemies = true;
	Sight->DetectionByAffiliation.bDetectFriendlies = true;
	Sight->DetectionByAffiliation.bDetectNeutrals = true;
	Sight->SetMaxAge(5.f);
	PerceptionComponent->ConfigureSense(*Sight);
	PerceptionComponent->SetDominantSense(*Sight->GetSenseImplementation());
	UAISenseConfig_Damage* Damage = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("DamageSenseConfig"));
	PerceptionComponent->ConfigureSense(*Damage);
}

void AAIControllerBase::OnInitialize_Implementation()
{
	bInitialized = true;
}

void AAIControllerBase::OnPreparatory_Implementation()
{
}

void AAIControllerBase::OnRefresh_Implementation(float DeltaSeconds)
{
	if (!HasAuthority() || !CurrentBlackboard)
	{
		return;
	}
	BlackboardRefreshRemaining -= DeltaSeconds;
	if (BlackboardRefreshRemaining <= 0.f)
	{
		BlackboardRefreshRemaining = FMath::Max(0.05f, BlackboardRefreshInterval);
		CurrentBlackboard->Refresh();
	}
}

void AAIControllerBase::OnTermination_Implementation()
{
	StopBehaviorTree();
	ReleaseBlackboardContext();
	CurrentBehaviorTree = nullptr;
	bInitialized = false;
}

void AAIControllerBase::BeginPlay()
{
	Super::BeginPlay();
	if (Execute_IsUseDefaultLifecycle(this))
	{
		if (!Execute_IsInitialized(this))
		{
			Execute_OnInitialize(this);
		}
		Execute_OnPreparatory(this);
	}
}

void AAIControllerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnTermination(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AAIControllerBase::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ReleaseBlackboardContext();
	if (PerceptionComponent)
	{
		PerceptionComponent->ForgetAll();
		PerceptionComponent->RequestStimuliListenerUpdate();
	}
	BlackboardRefreshRemaining = 0.f;
	InitBehaviorTree(true);
}

void AAIControllerBase::OnUnPossess()
{
	StopBehaviorTree();
	ReleaseBlackboardContext();
	CurrentBehaviorTree = nullptr;
	if (PerceptionComponent)
	{
		PerceptionComponent->ForgetAll();
	}
	Super::OnUnPossess();
}

void AAIControllerBase::ReleaseBlackboardContext()
{
	if (CurrentBlackboard)
	{
		CurrentBlackboard->Uninitialize();
	}
	CurrentBlackboard = nullptr;
	SourceBlackboard = nullptr;
	BlackboardPawn.Reset();
}

void AAIControllerBase::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	APawn* Target = Cast<APawn>(Actor);
	if (!HasAuthority() || !CurrentBlackboard || !Target || Target == GetPawn())
	{
		return;
	}
	if (Stimulus.WasSuccessfullySensed())
	{
		CurrentBlackboard->SetTargetAgent(Target);
		CurrentBlackboard->SetTargetLocation(Target->GetActorLocation());
		CurrentBlackboard->SetIsLostTarget(false);
	}
	else if (CurrentBlackboard->GetTargetAgent() == Target)
	{
		CurrentBlackboard->SetIsLostTarget(true);
	}
}

void AAIControllerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnRefresh(this, DeltaSeconds);
	}
}

void AAIControllerBase::InitBehaviorTree(bool bAutoRun)
{
	const IAIAgentInterface* Agent = Cast<IAIAgentInterface>(GetPawn());
	if (HasAuthority() && Agent)
	{
		SetBehaviorTree(Agent->GetBehaviorTreeAsset(), bAutoRun);
	}
}

bool AAIControllerBase::SetBehaviorTree(UBehaviorTree* InAsset, bool bAutoRun)
{
	if (!HasAuthority() || !GetPawn())
	{
		return false;
	}
	if (!InAsset)
	{
		StopBehaviorTree();
		ReleaseBlackboardContext();
		CurrentBehaviorTree = nullptr;
		return false;
	}
	UAIBlackboardBase* Definition = Cast<UAIBlackboardBase>(InAsset->BlackboardAsset);
	if (!Definition)
	{
		UE_LOG(LogTemp, Error, TEXT("行为树必须使用AIBlackboardBase定义：%s"), *GetNameSafe(InAsset));
		return false;
	}
	if (CurrentBehaviorTree != InAsset || BlackboardPawn.Get() != GetPawn())
	{
		StopBehaviorTree();
	}
	if (!CurrentBlackboard || SourceBlackboard != Definition || BlackboardPawn.Get() != GetPawn())
	{
		ReleaseBlackboardContext();
		UBlackboardComponent* BlackboardComponent = nullptr;
		if (!UseBlackboard(Definition, BlackboardComponent) || !BlackboardComponent)
		{
			return false;
		}
		if (!BlackboardComponent->InitializeBlackboard(*Definition))
		{
			return false;
		}
		SourceBlackboard = Definition;
		BlackboardPawn = GetPawn();
		CurrentBlackboard = Definition->CreateContext(this, BlackboardComponent);
		if (!CurrentBlackboard)
		{
			ReleaseBlackboardContext();
			return false;
		}
	}
	CurrentBehaviorTree = InAsset;
	return !bAutoRun || IsRunningBehaviorTree() || Super::RunBehaviorTree(InAsset);
}

bool AAIControllerBase::RunBehaviorTree(UBehaviorTree* BTAsset)
{
	return SetBehaviorTree(BTAsset, true);
}

bool AAIControllerBase::RunBehaviorTree(bool bForce)
{
	if (!HasAuthority() || !CurrentBehaviorTree)
	{
		return false;
	}
	if (bForce)
	{
		StopBehaviorTree();
	}
	return IsRunningBehaviorTree() || Super::RunBehaviorTree(CurrentBehaviorTree);
}

void AAIControllerBase::StopBehaviorTree()
{
	if (UBehaviorTreeComponent* Behavior = GetBehaviorTreeComponent())
	{
		Behavior->StopTree(EBTStopMode::Forced);
	}
}

bool AAIControllerBase::IsRunningBehaviorTree() const
{
	const UBehaviorTreeComponent* Behavior = GetBehaviorTreeComponent();
	return Behavior && Behavior->IsRunning();
}

UBehaviorTreeComponent* AAIControllerBase::GetBehaviorTreeComponent() const
{
	return Cast<UBehaviorTreeComponent>(GetBrainComponent());
}
