#include "Character/CharacterModuleNetworkComponent.h"

#include "Animation/AnimMontage.h"
#include "Character/Base/CharacterInterface.h"
#include "Engine/World.h"

UCharacterModuleNetworkComponent::UCharacterModuleNetworkComponent()
{
}

bool UCharacterModuleNetworkComponent::CanOperate(AActor* InCharacter) const
{
    return IsValid(GetOwner()) && GetOwner()->HasAuthority() && IsValid(InCharacter) &&
        InCharacter->HasAuthority() && GetWorld() && !GetWorld()->bIsTearingDown &&
        GetWorld()->GetNetMode() != NM_Client && InCharacter->GetWorld() == GetWorld() &&
        Cast<ICharacterInterface>(InCharacter) != nullptr;
}

bool UCharacterModuleNetworkComponent::PlayMontageAuthority(AActor* InCharacter, UAnimMontage* InMontage)
{
    if (!CanOperate(InCharacter) || !IsValid(InMontage))
    {
        return false;
    }
    Cast<ICharacterInterface>(InCharacter)->PlayMontage(InMontage, true);
    return true;
}

bool UCharacterModuleNetworkComponent::StopMontageAuthority(AActor* InCharacter, UAnimMontage* InMontage)
{
    if (!CanOperate(InCharacter) || (InMontage && !IsValid(InMontage)))
    {
        return false;
    }
    Cast<ICharacterInterface>(InCharacter)->StopMontage(InMontage, true);
    return true;
}

bool UCharacterModuleNetworkComponent::TransformAuthority(AActor* InCharacter, FTransform InTransform, float InDuration)
{
    if (!CanOperate(InCharacter) || InTransform.ContainsNaN() || !InTransform.GetRotation().IsNormalized() ||
        !FMath::IsFinite(InDuration) || InDuration < 0.f)
    {
        return false;
    }
    Cast<ICharacterInterface>(InCharacter)->TransformTowards(InTransform, InDuration, true);
    return true;
}

bool UCharacterModuleNetworkComponent::RotateAuthority(AActor* InCharacter, FRotator InRotation, float InDuration)
{
    if (!CanOperate(InCharacter) || InRotation.ContainsNaN() || !FMath::IsFinite(InDuration) || InDuration < 0.f)
    {
        return false;
    }
    Cast<ICharacterInterface>(InCharacter)->RotationTowards(InRotation, InDuration, true);
    return true;
}

bool UCharacterModuleNetworkComponent::MoveAIAuthority(AActor* InCharacter, FVector InLocation, float InStopDistance)
{
    if (!CanOperate(InCharacter) || InLocation.ContainsNaN() || !FMath::IsFinite(InStopDistance) || InStopDistance < 0.f)
    {
        return false;
    }
    Cast<ICharacterInterface>(InCharacter)->AIMoveTo(InLocation, InStopDistance, true);
    return true;
}

bool UCharacterModuleNetworkComponent::StopAIAuthority(AActor* InCharacter)
{
    if (!CanOperate(InCharacter))
    {
        return false;
    }
    Cast<ICharacterInterface>(InCharacter)->StopAIMove(true);
    return true;
}
