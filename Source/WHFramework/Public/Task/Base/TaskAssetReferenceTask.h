#pragma once

#include "Task/Base/TaskBase.h"
#include "TaskAssetReferenceTask.generated.h"

class UTaskAsset;

/**
 * A composite task whose runtime children are instantiated from another task asset.
 * The referenced definition is never modified; each include site receives stable,
 * path-derived task identities so the same asset can safely be reused more than once.
 */
UCLASS(BlueprintType, DisplayName = "Task Asset Reference")
class WHFRAMEWORK_API UTaskAssetReferenceTask : public UTaskBase
{
	GENERATED_BODY()

public:
	UTaskAssetReferenceTask();

	/** Reusable task definition expanded below this node when the owning runtime asset is created. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task Asset Reference")
	TSoftObjectPtr<UTaskAsset> ReferencedAsset;

	/** Enter eligible referenced root tasks when this composite enters and while it is active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task Asset Reference")
	bool bAutoEnterReferencedRoots = true;

	virtual void OnEnter() override;
	virtual void OnRefresh() override;
	virtual bool AreSubTasksReadyToComplete() const override;

	bool IsRuntimeExpanded() const { return bRuntimeExpanded; }

private:
	friend class UTaskAsset;

	void EnterReferencedRoots();

	UPROPERTY(Transient)
	bool bRuntimeExpanded = false;
};
