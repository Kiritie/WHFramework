#pragma once

#include "EdGraph/EdGraphNode.h"
#include "ProcedureAssetGraphNode.generated.h"

class UProcedureBase;

UCLASS(Transient)
class WHFRAMEWORKEDITOR_API UProcedureAssetGraphNode : public UEdGraphNode
{
    GENERATED_BODY()

public:
    UPROPERTY()
    TObjectPtr<UProcedureBase> Procedure;
    UPROPERTY()
    bool bStart = false;

    virtual void AllocateDefaultPins() override;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetTooltipText() const override;
    virtual FLinearColor GetNodeTitleColor() const override;
    virtual bool CanUserDeleteNode() const override;
    virtual bool CanDuplicateNode() const override;
};

TSharedRef<struct FGraphPanelNodeFactory> CreateProcedureGraphNodeFactory();
