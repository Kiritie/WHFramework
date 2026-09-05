#pragma once

#include "EdGraph/EdGraphNode.h"
#include "TaskAssetGraphNode.generated.h"

class UTaskBase;

UCLASS(Transient)
class WHFRAMEWORKEDITOR_API UTaskAssetGraphNode : public UEdGraphNode
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UTaskBase* Task;

	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
};

TSharedRef<struct FGraphPanelNodeFactory> CreateTaskGraphNodeFactory();
