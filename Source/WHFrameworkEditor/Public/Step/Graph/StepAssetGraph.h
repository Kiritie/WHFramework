#pragma once

#include "EdGraph/EdGraph.h"
#include "StepAssetGraph.generated.h"

class UStepAsset;
class UStepAssetGraphNode;

UCLASS(Transient)
class WHFRAMEWORKEDITOR_API UStepAssetGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UStepAsset* StepAsset = nullptr;

	FSimpleDelegate OnStepsChanged;
	TArray<FText> Errors;

	void Rebuild();
	UStepAssetGraphNode* FindNode(const FString& InGUID) const;
};
