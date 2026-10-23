#include "Character/Components/CharacterMovementTypes.h"

bool FCharacterMovementConstraint::IsValid() const
{
	return FMath::IsFinite(SpeedMultiplier) && SpeedMultiplier >= 0.f && SpeedMultiplier <= 3.f &&
		FMath::IsFinite(RotationMultiplier) && RotationMultiplier >= 0.f && RotationMultiplier <= 3.f &&
		static_cast<uint8>(Traversal) <= static_cast<uint8>(ECharacterTraversalMode::Glide);
}

bool FCharacterMovementConstraint::Matches(const FCharacterMovementConstraint& InOther) const
{
	return SpeedMultiplier == InOther.SpeedMultiplier && RotationMultiplier == InOther.RotationMultiplier &&
		bBlockMovement == InOther.bBlockMovement && bBlockRotation == InOther.bBlockRotation &&
		bSprint == InOther.bSprint && Traversal == InOther.Traversal && Priority == InOther.Priority;
}
