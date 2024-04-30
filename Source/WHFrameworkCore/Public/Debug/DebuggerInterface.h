#pragma once

#include "DebuggerInterface.generated.h"

UINTERFACE()
class WHFRAMEWORKCORE_API UDebuggerInterface : public UInterface
{
	GENERATED_BODY()
};

class WHFRAMEWORKCORE_API IDebuggerInterface
{
	GENERATED_BODY()

public:
	void Register();

	void UnRegister();
	
	void DrawDebug(UCanvas* InCanvas, APlayerController* InPC);

protected:
	virtual void OnDrawDebug(UCanvas* InCanvas, APlayerController* InPC) { }

public:
	virtual bool IsDrawDebug() { return true; }
	
	virtual FString GetDebuggerName() { return TEXT("Game"); }

private:
	FDelegateHandle DrawDebugHandle;
};