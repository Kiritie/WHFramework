#include "Setting/SettingRegistry.h"

#include "Setting/SettingModule.h"

void USettingRegistry::GenerateSnapshot()
{
#if WITH_EDITOR
	USettingModule::Get().GenerateRegistrySnapshot(this);
#endif
}
