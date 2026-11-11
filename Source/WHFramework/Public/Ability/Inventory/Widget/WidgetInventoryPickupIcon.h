#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "WidgetInventoryPickupIcon.generated.h"

class UCommonTextBlockN;
class UImage;
class UTextBlock;

UCLASS()
class WHFRAMEWORK_API UWidgetInventoryPickupIcon : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetItemPresentation(UObject* InIcon, const FText& InFallbackName, int32 InCount,
        FGameplayTag InCountStyleTag = FGameplayTag());

protected:
    virtual void NativeOnInitialized() override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UImage> Image;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Label;

    UPROPERTY(Transient)
    TObjectPtr<UCommonTextBlockN> CountLabel;

    UPROPERTY(Transient)
    TObjectPtr<UObject> Icon;

    FText ItemName;
    FGameplayTag CountStyleTag;
    int32 Count = 0;
    void Refresh();
};
