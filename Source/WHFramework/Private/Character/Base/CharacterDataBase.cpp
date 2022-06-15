#include "Character/Base/CharacterDataBase.h"

UCharacterDataBase::UCharacterDataBase()
{
	AnimMontages = TMap<FName, UAnimMontage*>();
}
