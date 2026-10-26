#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "WidgetBlueprintEditorLibrary.generated.h"

class UBlueprint;
class UWidget;
class UWidgetTree;
class UPanelWidget;

// 编辑源控件模板，并只读检查已存在的 Designer 预览；不创建运行时实例。
UCLASS()
class WHFRAMEWORKEDITOR_API UWidgetBlueprintEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "WidgetModule")
	static UWidgetTree* GetWidgetTree(UBlueprint* InBlueprint);

	UFUNCTION(BlueprintPure, Category = "WidgetModule")
	static UWidget* GetRootWidget(UBlueprint* InBlueprint);

	UFUNCTION(BlueprintPure, Category = "WidgetModule")
	static UWidget* FindWidget(UBlueprint* InBlueprint, FName InName);

	UFUNCTION(BlueprintPure, Category = "WidgetModule")
	static TArray<UWidget*> GetWidgets(UBlueprint* InBlueprint);

	// 导出源图及节点原文，保留引脚类型、默认值和连线；不创建运行时控件。
	UFUNCTION(BlueprintPure, Category = "WidgetModule")
	static FString ExportGraphData(UBlueprint* InBlueprint);

	UFUNCTION(BlueprintPure, Category = "WidgetModule")
	static FString GetPreviewData(UBlueprint* InBlueprint, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "WidgetModule")
	static UWidget* CreateWidgetTemplate(UBlueprint* InBlueprint, TSubclassOf<UWidget> InClass, FName InName, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "WidgetModule")
	static bool SetRootWidget(UBlueprint* InBlueprint, UWidget* InRoot, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "WidgetModule")
	static bool MoveWidget(UBlueprint* InBlueprint, UWidget* InWidget, UPanelWidget* InParent, int32 InChildIndex, FString& OutError);

	// 同继承链替换保留名称、GUID、槽位和图引用；组装结束后显式编译。
	UFUNCTION(BlueprintCallable, Category = "WidgetModule")
	static bool ReplaceWidget(UBlueprint* InBlueprint, UWidget* InWidget, TSubclassOf<UWidget> InClass, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "WidgetModule")
	static bool RemoveWidget(UBlueprint* InBlueprint, UWidget* InWidget, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "WidgetModule")
	static bool RenameWidget(UBlueprint* InBlueprint, UWidget* InWidget, const FString& InName, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "WidgetModule")
	static bool RetargetWidgetEvent(UBlueprint* InBlueprint, FName InComponentName, UClass* InDelegateOwnerClass, FName InDelegateName, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "WidgetModule")
	static bool ShareWidgetAnimationBinding(UBlueprint* InBlueprint, FName InAnimationName, FName InSourceWidget, FName InTargetWidget, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "WidgetModule")
	static bool RemoveWidgetAnimationBinding(UBlueprint* InBlueprint, FName InAnimationName, FName InWidgetName, FString& OutError);

	UFUNCTION(BlueprintPure, Category = "WidgetModule")
	static bool ValidateWidgetIdentifiers(UBlueprint* InBlueprint, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "WidgetModule")
	static bool CompileWidgetBlueprint(UBlueprint* InBlueprint, FString& OutError);
};
