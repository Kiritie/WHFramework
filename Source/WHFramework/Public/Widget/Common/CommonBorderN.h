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

	UFUNCTION(BlueprintCallable)
	void ClearStyleOverride();

	UFUNCTION(BlueprintPure)
	FGameplayTag GetStyleTag() const;

protected:
	virtual void BeginDestroy() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Style.Border"))
	FGameplayTag StyleTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bOverrideStyleTag = false;

	UPROPERTY(Transient)
	FGameplayTag InheritedStyleTag;

private:
	void ApplyStyleTag();
	FDelegateHandle ThemeChangedHandle;
};
