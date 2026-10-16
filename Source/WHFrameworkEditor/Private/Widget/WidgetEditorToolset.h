#pragma once

#include "ToolsetRegistry/ToolsetDefinition.h"
#include "WidgetEditorToolset.generated.h"

class UBlueprint;
class UWidget;
class UWidgetTheme;
class UPanelWidget;

// MCP复用控件模板与托管主题正式接口，不开放任意脚本执行。
UCLASS()
class UWidgetEditorToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	UFUNCTION(meta = (AICallable))
	static FString GetBlueprintGraphData(UBlueprint* Blueprint);

	UFUNCTION(meta = (AICallable))
	static UWidget* CreateWidgetTemplate(UBlueprint* Blueprint, TSubclassOf<UWidget> WidgetClass, FName WidgetName);

	UFUNCTION(meta = (AICallable))
	static bool SetRootWidget(UBlueprint* Blueprint, UWidget* Root);

	UFUNCTION(meta = (AICallable))
	static bool MoveWidget(UBlueprint* Blueprint, UWidget* Widget, UPanelWidget* Parent, int32 ChildIndex = -1);

	UFUNCTION(meta = (AICallable))
	static bool ReplaceWidget(UBlueprint* Blueprint, UWidget* Widget, TSubclassOf<UWidget> WidgetClass);

	UFUNCTION(meta = (AICallable))
	static bool RemoveWidget(UBlueprint* Blueprint, UWidget* Widget);

	UFUNCTION(meta = (AICallable))
	static bool RetargetWidgetEvent(UBlueprint* Blueprint, FName ComponentName, UClass* DelegateOwnerClass, FName DelegateName);

	UFUNCTION(meta = (AICallable))
	static bool CompileWidgetBlueprint(UBlueprint* Blueprint);

	UFUNCTION(meta = (AICallable))
	static UBlueprint* CreateStyle(UWidgetTheme* Theme, const FString& Tag, UClass* ParentClass, const FString& DestinationRoot);

	UFUNCTION(meta = (AICallable))
	static bool SetStyleProperty(UWidgetTheme* Theme, const FString& Tag, FName Property, const FString& Value);

	UFUNCTION(meta = (AICallable))
	static bool SaveTheme(UWidgetTheme* Theme);

	UFUNCTION(meta = (AICallable))
	static bool SetProjectDefault(UWidgetTheme* Theme);
};
