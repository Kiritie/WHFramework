#pragma once

#include "Main/Base/ModuleNetworkComponentBase.h"
#include "CharacterModuleNetworkComponent.generated.h"

class UAnimMontage;

// 只保留服务器代码/BP 可调用的广播入口，不提供任意角色客户端 RPC。
UCLASS()
class WHFRAMEWORK_API UCharacterModuleNetworkComponent : public UModuleNetworkComponentBase
{
    GENERATED_BODY()

public:
    UCharacterModuleNetworkComponent();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
    bool PlayMontageAuthority(AActor* InCharacter, UAnimMontage* InMontage);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
    bool StopMontageAuthority(AActor* InCharacter, UAnimMontage* InMontage);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
    bool TransformAuthority(AActor* InCharacter, FTransform InTransform, float InDuration = 1.f);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
    bool RotateAuthority(AActor* InCharacter, FRotator InRotation, float InDuration = 1.f);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
    bool MoveAIAuthority(AActor* InCharacter, FVector InLocation, float InStopDistance);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
    bool StopAIAuthority(AActor* InCharacter);

private:
    bool CanOperate(AActor* InCharacter) const;
};
