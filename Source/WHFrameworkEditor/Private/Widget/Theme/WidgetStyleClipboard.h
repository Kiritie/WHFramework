#pragma once

#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"

namespace WidgetStyleClipboard
{
	inline void Read(FString& Text)
	{
		for (int32 Attempt = 0; Attempt < 5; ++Attempt)
		{
			FPlatformApplicationMisc::ClipboardPaste(Text);
			if (!Text.IsEmpty())
			{
				return;
			}
			if (Attempt < 4)
			{
				FPlatformProcess::Sleep(0.005f);
			}
		}
	}

	inline bool Write(const FString& Text)
	{
		for (int32 Attempt = 0; Attempt < 5; ++Attempt)
		{
			FPlatformApplicationMisc::ClipboardCopy(*Text);
			FString Readback;
			Read(Readback);
			if (Readback == Text)
			{
				return true;
			}
			if (Attempt < 4)
			{
				FPlatformProcess::Sleep(0.005f);
			}
		}
		return false;
	}
}
