#pragma once

#include "Components/ScrollBox.h"
#include "GameplayTagContainer.h"
#include "CommonScroll.generated.h"

UCLASS(BlueprintType, Blueprintable)
class WHFRAMEWORK_API UCommonScroll : public UScrollBox
{
	GENERATED_BODY()

public:
	UCommonScroll(const FObjectInitializer& ObjectInitializer);
	virtual void SynchronizeProperties() override;

	UFUNCTION(BlueprintCallable)
	void SetStyleTag(FGameplayTag InStyleTag);

	UFUNCTION(BlueprintCallable)
	void SetInheritedStyleTag(FGameplayTag InStyleTag);

	UFUNCTION(BlueprintCallable)
	void ClearStyleOverride();

	UFUNCTION(BlueprintPure)
	FGameplayTag GetStyleTag() const;

protected:
	virtual void BeginDestroy() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Style.Scroll"))
	FGameplayTag StyleTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bOverrideStyleTag = false;

	UPROPERTY(Transient)
	FGameplayTag InheritedStyleTag;

private:
	void ApplyStyleTag();
	FDelegateHandle ThemeChangedHandle;
};
