#include "UI/SpectateOverlayWidget.h"

#include "Components/TextBlock.h"
#include "GameFramework/PlayerState.h"
#include "Core/PlayerControllers/HeistPlayerController.h"
#include "Core/Spectate/HeistSpectatorComponent.h"

void USpectateOverlayWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Bind();
}

void USpectateOverlayWidget::NativeDestruct()
{
	if (Bound)
	{
		Bound->OnSpectateStateChanged.RemoveDynamic(this, &USpectateOverlayWidget::HandleStateChanged);
		Bound->OnViewedChanged.RemoveDynamic(this, &USpectateOverlayWidget::HandleViewedChanged);
		Bound = nullptr;
	}
	Super::NativeDestruct();
}

void USpectateOverlayWidget::Bind()
{
	const AHeistPlayerController* PC = Cast<AHeistPlayerController>(GetOwningPlayer());
	Bound = PC ? PC->GetSpectatorComponent() : nullptr;
	if (!Bound)
	{
		// 관전 컴포넌트가 없는 맵(은신처·타이틀)이다. 여기서 그냥 돌아가면
		// WBP 디자이너에 찍힌 가시성이 그대로 남아 관전이 아닌데도 오버레이가 떠 있다
		Refresh(false, nullptr);
		return;
	}

	Bound->OnSpectateStateChanged.AddDynamic(this, &USpectateOverlayWidget::HandleStateChanged);
	Bound->OnViewedChanged.AddDynamic(this, &USpectateOverlayWidget::HandleViewedChanged);

	Refresh(Bound->IsSpectating(), Bound->GetViewedPlayer());
}

void USpectateOverlayWidget::HandleStateChanged(bool bSpectating)
{
	Refresh(bSpectating, Bound ? Bound->GetViewedPlayer() : nullptr);
}

void USpectateOverlayWidget::HandleViewedChanged(APlayerState* NewViewed)
{
	Refresh(Bound && Bound->IsSpectating(), NewViewed);;
}

void USpectateOverlayWidget::Refresh(bool bSpectating, APlayerState* Viewed)
{
	SetVisibility(bSpectating ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (Txt_ViewedName)
	{
		Txt_ViewedName->SetText(Viewed ? FText::FromString(Viewed->GetPlayerName()) : FText::GetEmpty());
	}
}
