#pragma once

#include "CoreMinimal.h"

/** An original world suspension remains identifiable after its module is retired. */
class WHFRAMEWORK_API FVoxelSaveSuspension
{
public:
	FVoxelSaveSuspension(const FGuid& InToken, uint64 InEpoch) : Token(InToken), Epoch(InEpoch) {}
	bool IsActive() const { return Token.IsValid(); }
	const FGuid& GetToken() const { return Token; }
	uint64 GetEpoch() const { return Epoch; }
	bool Release(const FGuid& InToken, uint64 InEpoch, FString& OutError)
	{
		check(IsInGameThread());
		if (!Token.IsValid() && InToken.IsValid() && InToken == ReleasedToken && InEpoch == ReleasedEpoch) return true;
		if (!InToken.IsValid() || Token != InToken || Epoch != InEpoch)
		{
			OutError = TEXT("最终存档冻结释放的所有者或世界代次不匹配。");
			return false;
		}
		ReleasedToken = Token;
		ReleasedEpoch = Epoch;
		Token.Invalidate();
		Epoch = 0;
		return true;
	}
private:
	FGuid Token;
	uint64 Epoch = 0;
	FGuid ReleasedToken;
	uint64 ReleasedEpoch = 0;
};
