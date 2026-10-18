#pragma once

#include "CommonBorder.h"
#include "GameplayTagContainer.h"
#include "CommonBorderN.generated.h"

UCLASS(BlueprintType, Blueprintable)
class WHFRAMEWORK_API UCommonBorderN : public UCommonBorder
{
	GENERATED_BODY()

public:
	UCommonBorderN(const FObjectInitializer& ObjectInitializer);
	virtual void SynchronizeProperties() override;

	UFUNCTION(BlueprintCallable)
	void SetStyleTag(FGameplayTag InStyleTag);

	UFUNCTION(BlueprintCallable)
	void SetInheritedStyleTag(FGameplayTag InStyleTag);

	UFUNCTION(BlueprintPure)
	FGameplayTag GetStyleTag() const;

protected:
	virtual void BeginDestroy() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "UI.Style.Border", ToolTip = "填写时使用当前 Tag；留空时使用外层传入的 Tag，外层未提供时使用类型默认样式。"))
	FGameplayTag StyleTag;

	UPROPERTY(Transient)
	FGameplayTag InheritedStyleTag;

private:
	void ApplyStyleTag();
	FDelegateHandle ThemeChangedHandle;
};
