#include "Widget/Theme/WidgetThemeSettings.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#if WITH_EDITOR
void UWidgetThemeSettings::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	FWidgetThemeResolver::InvalidateEditorDefault();
}
#endif
