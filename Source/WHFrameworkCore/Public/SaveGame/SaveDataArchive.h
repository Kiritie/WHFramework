#pragma once

#include "CoreMinimal.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

/*
 * FSaveDataArchive
 */
struct WHFRAMEWORKCORE_API FSaveDataArchive : public FObjectAndNameAsStringProxyArchive
{
	FSaveDataArchive(FArchive& InInnerArchive) : FObjectAndNameAsStringProxyArchive(InInnerArchive, false)
	{
		ArIsSaveGame = true;
	}
};
