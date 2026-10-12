#pragma once

#include "EdGraph/EdGraph.h"
#include "TaskAssetGraph.generated.h"

class UTaskAsset;

UCLASS(Transient)
class WHFRAMEWORKEDITOR_API UTaskAssetGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UTaskAsset* TaskAsset;

	FSimpleDelegate OnTasksChanged;
	void RebuildFromAsset();
	class UTaskAssetGraphNode* FindNode(const FString& StableGUID) const;
};
