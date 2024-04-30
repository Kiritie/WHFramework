#include "Debug/DebuggerInterface.h"

#include "Debug/DebugDrawService.h"

void IDebuggerInterface::Register()
{
	DrawDebugHandle = UDebugDrawService::Register(*GetDebuggerName(), FDebugDrawDelegate::CreateRaw(this, &IDebuggerInterface::DrawDebug));
}

void IDebuggerInterface::UnRegister()
{
	if(DrawDebugHandle.IsValid())
	{
		UDebugDrawService::Unregister(DrawDebugHandle);
	}
}

void IDebuggerInterface::DrawDebug(UCanvas* InCanvas, APlayerController* InPC)
{
	if(IsDrawDebug())
	{
		OnDrawDebug(InCanvas, InPC);
	}
}
