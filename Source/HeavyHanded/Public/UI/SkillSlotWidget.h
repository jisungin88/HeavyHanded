#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"   // FGameplayTag · FGameplayTagContainer — 태그 콜백 시그니처와 멤버에 값으로 들어간다
#include "SkillSlotWidget.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UImage;
class UTextBlock;

/**
 * HUD 스킬 슬롯 한 칸 — 쿨다운 표시 (기획서 8장 "스킬").
 *
 * [스킬을 어떻게 찾나] 내 ASC 에 부여된 어빌리티 중 쿨다운 태그가 있는 첫 번째 것을
 *   이 칸의 스킬로 본다. 집기 · 던지기 · 놓기는 쿨다운이 없고 역할 스킬에만 있어서
 *   이것만으로 갈린다. 어빌리티 클래스 이름으로 찾지 않는 이유는 이름이 아직 자주
 *   바뀌기 때문이다 (GA_Mimic → GA_Mimic_BodySwap).
 *
 * [임시 — 한 칸만 된다] 지금은 캐릭터마다 쿨다운 스킬이 하나라 "첫 번째" 로 충분하다.
 *   액티브 B · 팀 시너지가 들어오면 어느 스킬이 몇 번 칸인지 이 방식으로는 알 수 없다.
 *   그때는 어빌리티에 Ability.Slot.* 태그를 붙여 그걸로 찾도록 바꾼다 (플레이어 파트와 합의 필요).
 *
 * [쿨다운 시간을 들고 있지 않는다] 남은 시간 · 전체 시간은 실제로 걸린 쿨다운 GE 에서 읽는다.
 *   GE 수치가 바뀌거나 은신처 강화로 줄어도 위젯은 고칠 것이 없다.
 *
 * [읽기만 한다] ASC 의 어빌리티 목록과 태그를 구독할 뿐 아무것도 바꾸지 않는다
 *   (규약 01 — UI 는 시스템 상태를 읽고 구독만 한다). 스킬 발동은 입력(IA_Active_A)이 한다.
 *
 * [클라에서도 보이는 이유] 쿨다운 GE 는 소유 클라이언트에 복제되고, 발동한 본인 클라에서는
 *   예측 적용으로 즉시 붙는다. 그래서 태그 이벤트가 클라에서도 온다.
 */
UCLASS(Abstract)
class HEAVYHANDED_API USkillSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 이 칸에 붙은 스킬을 찾았는가 */
	UFUNCTION(BlueprintPure, Category = "UI|Skill")
	bool IsSkillBound() const { return BoundAbility.IsValid(); }

	/** 지금 쓸 수 있는가. 스킬을 못 찾았으면 false */
	UFUNCTION(BlueprintPure, Category = "UI|Skill")
	bool IsReady() const { return IsSkillBound() && !bOnCooldown; }

	/** 쿨다운 남은 초. 쿨다운이 아니면 0 */
	UFUNCTION(BlueprintPure, Category = "UI|Skill")
	float GetRemainingSeconds() const { return bOnCooldown ? RemainingSeconds : 0.f; }

	/** 이번 쿨다운의 전체 초. 쿨다운이 아니면 0 */
	UFUNCTION(BlueprintPure, Category = "UI|Skill")
	float GetCooldownDuration() const { return bOnCooldown ? DurationSeconds : 0.f; }

	/**
	 * 쿨다운 진행률. 방금 썼으면 0, 다 돌았으면 1. 쿨다운이 아니면 1.
	 *
	 * 원형 채우기 머티리얼에 그대로 넣으면 "차오르면 사용 가능" 으로 읽힌다.
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Skill")
	float GetCooldownProgress01() const;

	/**
	 * 남은 비율. 방금 썼으면 1, 다 돌았으면 0. 쿨다운이 아니면 0.
	 *
	 * 덮개 세로 크기가 이 값이다. GetCooldownProgress01 의 반대지만 GE 시간을 아직 못 읽은
	 * 순간에 1 을 돌려준다는 점이 다르다 — 그때 덮개가 안 보이면 쿨다운이 없는 것처럼 읽힌다.
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Skill")
	float GetCooldownRemaining01() const;

	/**
	 * 화면에 쓸 스킬 이름.
	 *
	 * [임시] 어빌리티에 표시 이름이 아직 없어서 태그 마지막 마디를 쓴다 ("ShadowStep").
	 *   아이콘 · 표시 이름이 생기면 여기만 바꾼다.
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Skill")
	FText GetSkillName() const { return SkillName; }

protected:
	//~ UUserWidget
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End

	// ── WBP 가 배치해야 하는 위젯 ──
	//
	// 전부 Optional 이다. 이름만 맞춰 두면 C++ 이 채운다.
	// [WBP_HUD 에 넣을 때] 기존 Card_Skill1 이름을 재사용하지 말 것 —
	//   같은 이름의 BP 변수가 남아 있으면 BindWidget 포인터가 꼬인다 (HeistHUDWidget.h 소지 슬롯 주석).

	/**
	 * 쿨다운 중 카드를 덮는 어두운 이미지. 사용 가능하면 숨긴다.
	 *
	 * 남은 시간만큼 세로 크기가 줄어 위에서 아래로 내려간다. 크기 · 피벗 · 가시성은 C++ 이 매번 덮어쓰므로
	 * WBP 에서는 칸을 꽉 채우게(Fill) 배치만 한다. 렌더 트랜스폼을 애니메이션으로 건드리지 말 것.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Skill", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_CooldownDim;

	/** 남은 초 ("37"). 사용 가능하면 숨긴다 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Skill", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Cooldown;

	/**
	 * 스킬 아이콘. 그림은 UUISettings::SkillIcons 표에서 쿨다운 태그로 찾는다.
	 *
	 * 표에도 폴백에도 그림이 없으면 숨긴다 — 흰 네모가 뜨는 것보다 낫다.
	 * 쿨다운 덮개(Img_CooldownDim)보다 아래(계층구조에서 위)에 둘 것.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Skill", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_SkillIcon;

	/** 스킬 이름. GetSkillName() 과 같은 값 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Skill", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_SkillName;

	/**
	 * 어두운 덮개의 알파. 색은 UUISettings 의 BgBase 토큰을 쓰고 알파만 여기서 정한다.
	 *
	 * 기획서에 수치가 없어서 잡은 값이다. 뒤의 카드 글자가 비쳐 보일 정도로 둔다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Slot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DimOpacity = 0.65f;

	// ── BP 연출 훅 ──
	//
	// 표시는 C++ 이 이미 끝냈다. 여기는 번쩍임 · 사운드 자리다.
	// 훅은 클라에서도 돌기 때문에 게임 상태를 바꾸지 말 것.

	/** 이 칸의 스킬을 찾았다. 한 번만 온다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Skill")
	void OnSkillBound(const FText& InSkillName);

	/**
	 * 쿨다운이 시작됐다.
	 *
	 * 붙는 순간 이미 쿨다운 중이었으면(중간 참가) 오지 않는다 — 방금 쓴 것처럼 보이기 때문이다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Skill")
	void OnCooldownStarted(float Duration);

	/** 쿨다운 중 주기적으로 온다. 원형 채우기 머티리얼 파라미터를 여기서 갱신한다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Skill")
	void OnCooldownUpdated(float Remaining, float Progress01);

	/** 쿨다운이 끝났다. "다시 쓸 수 있다" 번쩍임을 여기에 건다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Skill")
	void OnCooldownEnded();

private:
	/** ASC 와 스킬을 찾을 때까지 재시도한다. 클라에는 PlayerState · 어빌리티 목록이 늦게 온다 */
	void TryBind();

	/** 부여된 어빌리티 중 쿨다운 태그가 있는 첫 번째 것. 없으면 null */
	static const UGameplayAbility* FindCooldownAbility(const UAbilitySystemComponent& ASC);

	/** 구독을 전부 뗀다 */
	void Unbind();

	void HandleCooldownTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** 쿨다운 상태를 ASC 에서 다시 읽는다. bFireHooks 가 false 면 훅 없이 상태만 맞춘다 */
	void RefreshCooldown(bool bFireHooks);

	/** GE 에서 남은 시간 · 전체 시간을 읽는다 */
	void ReadRemaining();

	/** 쿨다운 중 주기 콜백 */
	void TickCooldown();

	/** 지금 상태로 덮개 · 글자를 그린다 */
	void ApplyVisual();

	/**
	 * 표에서 아이콘을 찾아 Img_SkillIcon 에 칠한다. 바인딩 순간 한 번만 부른다.
	 *
	 * [캐시가 없는 이유] 칸 하나가 스킬 하나에 한 번 붙고 끝이라 같은 그림을 다시 찾을 일이 없다.
	 *   UHeistHUDWidget::ResolveHeldIcon 은 0.1초마다 도는 경로라 캐시가 필요했다.
	 */
	void ApplyIcon();

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> BoundASC;

	/** 찾은 스킬의 CDO. 부여 목록이 바뀌면 사라질 수 있어 약참조로 든다 */
	TWeakObjectPtr<const UGameplayAbility> BoundAbility;

	/** 구독 중인 쿨다운 태그와 핸들. 쿨다운 GE 하나가 태그를 여러 개 줄 수도 있다 */
	FGameplayTagContainer CooldownTags;
	TArray<TPair<FGameplayTag, FDelegateHandle>> CooldownTagHandles;

	FTimerHandle BindRetryHandle;
	FTimerHandle CooldownTickHandle;

	FText SkillName;

	bool bOnCooldown = false;
	float RemainingSeconds = 0.f;
	float DurationSeconds = 0.f;

	/** 마지막으로 Txt_Cooldown 에 쓴 정수 초. 같으면 SetText 를 건너뛴다 */
	int32 LastShownSeconds = INDEX_NONE;

	/** 바인딩 재시도에 쓴 누적 시간 */
	float BindElapsed = 0.f;

	/** 포기 · 지연 경고를 한 번만 남기기 위한 플래그 */
	bool bWarnedNoASC = false;
	bool bWarnedNoSkill = false;

	/** 재바인딩 시도 간격 */
	static constexpr float BindRetryInterval = 0.25f;

	/**
	 * ASC 를 이만큼 못 찾으면 포기한다. GAS 가 없는 테스트 맵에서 영원히 도는 타이머를 남기지 않는다.
	 *
	 * [스킬 탐색은 포기하지 않는다] ASC 는 있는데 스킬이 없으면 폰이 아직 스폰 · 빙의 전일 수 있다.
	 *   그 대기는 로딩 길이에 달려 있어 상한을 정할 수 없으므로, 이 시간이 지나면 경고만 한 번
	 *   남기고 계속 찾는다. 찾는 비용은 부여된 어빌리티 몇 개를 도는 것뿐이다.
	 */
	static constexpr float BindGiveUpSeconds = 10.f;

	/**
	 * 쿨다운 표시 갱신 주기(초).
	 *
	 * [0.1 에서 올린 값이다] 덮개가 위에서 아래로 내려가는 연출이 생기면서 0.1초 간격으로는
	 *   뚝뚝 끊겨 보였다. 쿨다운 중에만 돌고, 매 번 하는 일은 남은 시간 조회와 RenderScale 하나다.
	 *   숫자는 정수 초가 바뀔 때만 SetText 하므로 이 주기와 무관하다.
	 *   NativeTick 이 아닌 이유는 UAlertGaugeWidget::InterpInterval 주석에 있다.
	 */
	static constexpr float CooldownTickInterval = 1.f / 60.f;
};
