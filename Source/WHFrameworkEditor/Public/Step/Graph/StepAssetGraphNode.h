#pragma once

#include "EdGraph/EdGraphNode.h"
#include "StepAssetGraphNode.generated.h"

class UStepBase;

UCLASS(Transient)
class WHFRAMEWORKEDITOR_API UStepAssetGraphNode : public UEdGraphNode
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UStepBase* Step = nullptr;

	UPROPERTY()
	FString StableGUID;

	bool IsStart() const { return Step == nullptr; }
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual bool CanUserDeleteNode() const override;
	virtual bool CanDuplicateNode() const override;
};

TSharedRef<struct FGraphPanelNodeFactory> CreateStepGraphNodeFactory();
