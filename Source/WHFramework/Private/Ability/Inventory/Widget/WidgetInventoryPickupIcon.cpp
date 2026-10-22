#include "Ability/Inventory/Widget/WidgetInventoryPickupIcon.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"

void UWidgetInventoryPickupIcon::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("PickupWidgetTree"));
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;
    Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ItemIcon"));
    Root->AddChildToOverlay(Image)->SetHorizontalAlignment(HAlign_Fill);
    Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ItemLabel"));
    Label->SetJustification(ETextJustify::Center);
    Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    Label->SetShadowColorAndOpacity(FLinearColor::Black);
    Label->SetShadowOffset(FVector2D(1.f));
    UOverlaySlot* TextSlot = Root->AddChildToOverlay(Label);
    TextSlot->SetHorizontalAlignment(HAlign_Center);
    TextSlot->SetVerticalAlignment(VAlign_Bottom);
    Refresh();
}

void UWidgetInventoryPickupIcon::SetItemPresentation(UObject* InIcon, const FText& InFallbackName, int32 InCount)
{
    Icon = InIcon;
    ItemName = InFallbackName;
    Count = FMath::Max(0, InCount);
    Refresh();
}

void UWidgetInventoryPickupIcon::Refresh()
{
    if (!Image || !Label) return;
    bool bHasIcon = false;
    if (UTexture2D* Texture = Cast<UTexture2D>(Icon))
    {
        Image->SetBrushFromTexture(Texture, false);
        bHasIcon = true;
    }
    else if (UMaterialInterface* Material = Cast<UMaterialInterface>(Icon))
    {
        Image->SetBrushFromMaterial(Material);
        bHasIcon = true;
    }
    Image->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    Label->SetText(bHasIcon ? (Count > 1 ? FText::AsNumber(Count) : FText::GetEmpty()) : ItemName);
    SetVisibility(ESlateVisibility::HitTestInvisible);
}
