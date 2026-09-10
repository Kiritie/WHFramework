#pragma once

#include "K2Node.h"
#include "K2Node_BroadcastEvent.generated.h"

UCLASS()
class WHFRAMEWORKDEVELOPER_API UK2Node_BroadcastEvent : public UK2Node
{
	GENERATED_BODY()

public:
	virtual void AllocateDefaultPins() override;

	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	virtual FText GetTooltipText() const override;

	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;

	virtual FText GetMenuCategory() const override;

protected:
	static bool CanUseStruct(const UScriptStruct* InStruct);

	UPROPERTY(EditAnywhere, Category = "Event")
	TObjectPtr<UScriptStruct> EventStruct;
};
