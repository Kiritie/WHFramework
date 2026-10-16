#include "Widget/Style/CommonRangeStyle.h"
#include "Components/Slider.h"
#include "Components/SpinBox.h"

UCommonRangeStyle::UCommonRangeStyle()
{
	SliderStyle = GetDefault<USlider>()->GetWidgetStyle();
	SpinBoxStyle = GetDefault<USpinBox>()->GetWidgetStyle();
}
