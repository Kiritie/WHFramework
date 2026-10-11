#pragma once

#include "Ability/Item/AbilityItemDataBase.h"
#include "Asset/Primary/PrimaryEntityInterface.h"
#include "Common/Base/WHActor.h"
#include "AbilityItemBase.generated.h"

UCLASS()
class WHFRAMEWORK_API AAbilityItemBase : public AWHActor, public IPrimaryEntityInterface
{
    GENERATED_BODY()

public:
    AAbilityItemBase();
    virtual void OnSpawn_Implementation(const FParameter& InParam) override;
    virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;

    bool InitializeProjection(const FPrimaryAssetId& InItemId, int32 InLevel, AActor* InAvatar);
    const UAbilityItemDataBase* GetItemDefinition() const;

    template<class T>
    const T& GetItemData() const
    {
        return *CastChecked<T>(GetItemDefinition());
    }

    const UAbilityItemDataBase& GetItemData() const;

    virtual FPrimaryAssetId GetAssetID_Implementation() const override { return ItemId; }
    virtual void SetAssetID_Implementation(const FPrimaryAssetId& InId) override { ItemId = InId; }

    UFUNCTION(BlueprintPure)
    int32 GetItemLevel() const { return ItemLevel; }

    template<class T>
    T* GetOwnerActor() const
    {
        return Cast<T>(OwnerActor);
    }

    UFUNCTION(BlueprintPure)
    AActor* GetOwnerActor() const { return OwnerActor; }

protected:
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    FPrimaryAssetId ItemId;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 ItemLevel = 0;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    TObjectPtr<AActor> OwnerActor;
};
