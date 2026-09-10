#pragma once

#include "K2Node.h"
#include "K2Node_MakeParameter.generated.h"

UCLASS()
class WHFRAMEWORKDEVELOPER_API UK2Node_MakeParameter : public UK2Node
{
	GENERATED_BODY()

public:
	virtual void AllocateDefaultPins() override;

	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

	virtual bool IsNodePure() const override { return true; }

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	virtual FText GetTooltipText() const override;

	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;

	virtual FText GetMenuCategory() const override;

protected:
	static bool CanUseStruct(const UScriptStruct* InStruct, bool bForInternalUse);

	UEdGraphPin* GetValuePin() const;
	UEdGraphPin* GetParameterPin() const;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UScriptStruct> ParameterStruct;
};
