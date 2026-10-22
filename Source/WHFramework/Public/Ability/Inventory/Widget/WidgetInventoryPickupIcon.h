#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WidgetInventoryPickupIcon.generated.h"

class UImage;
class UTextBlock;

UCLASS()
class WHFRAMEWORK_API UWidgetInventoryPickupIcon : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetItemPresentation(UObject* InIcon, const FText& InFallbackName, int32 InCount);

protected:
    virtual void NativeOnInitialized() override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UImage> Image;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> Label;

    UPROPERTY(Transient)
    TObjectPtr<UObject> Icon;

    FText ItemName;
    int32 Count = 0;
    void Refresh();
};
