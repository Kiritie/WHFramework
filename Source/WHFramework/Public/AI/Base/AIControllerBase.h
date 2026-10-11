#pragma once

#include "AIController.h"
#include "Common/Base/WHActor.h"
#include "Perception/AIPerceptionTypes.h"
#include "AIControllerBase.generated.h"

class UBehaviorTreeComponent;
class UBehaviorTree;
class UAIBlackboardBase;
class UAIBlackboardContext;

UCLASS()
class WHFRAMEWORK_API AAIControllerBase : public AAIController, public IWHActorInterface
{
	GENERATED_BODY()

public:
	AAIControllerBase();
	virtual void OnInitialize_Implementation() override;
	virtual void OnPreparatory_Implementation() override;
	virtual void OnRefresh_Implementation(float DeltaSeconds) override;
	virtual void OnTermination_Implementation() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void InitBehaviorTree(bool bAutoRun = false);
	virtual bool RunBehaviorTree(UBehaviorTree* BTAsset) override;
	virtual bool RunBehaviorTree(bool bForce = false);
	virtual void StopBehaviorTree();
	bool SetBehaviorTree(UBehaviorTree* InAsset, bool bAutoRun);
	bool IsRunningBehaviorTree() const;
	UBehaviorTree* GetBehaviorTree() const { return CurrentBehaviorTree; }
	UAIBlackboardContext* GetBlackboard() const { return CurrentBlackboard; }
	UBehaviorTreeComponent* GetBehaviorTreeComponent() const;

	template<class T>
	T* GetBlackboard() const
	{
		return Cast<T>(CurrentBlackboard);
	}

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UFUNCTION()
	virtual void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	virtual bool IsInitialized_Implementation() const override { return bInitialized; }
	virtual bool IsUseDefaultLifecycle_Implementation() const override { return true; }
	void ReleaseBlackboardContext();

	UPROPERTY(Transient)
	bool bInitialized = false;

	UPROPERTY(Transient)
	TObjectPtr<UBehaviorTree> CurrentBehaviorTree;

	UPROPERTY(Transient)
	TObjectPtr<UAIBlackboardBase> SourceBlackboard;

	UPROPERTY(Transient)
	TObjectPtr<UAIBlackboardContext> CurrentBlackboard;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> BlackboardPawn;

	UPROPERTY(EditDefaultsOnly)
	float BlackboardRefreshInterval = 0.2f;

	float BlackboardRefreshRemaining = 0.f;
};
