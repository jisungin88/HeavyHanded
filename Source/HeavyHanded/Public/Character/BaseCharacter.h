// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "InputActionValue.h"
#include "BaseCharacter.generated.h"

USTRUCT(BlueprintType)
struct FAbilityInputBinding
{
	GENERATED_BODY()

public:
	// 에디터에서 지정할 입력 액션 (예: IA_Interact)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Input")
	TObjectPtr<class UInputAction> InputAction;

	// 그 입력에 대응할 어빌리티 클래스 (예: GAB_Interact)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Input")
	TSubclassOf<class UGameplayAbility> AbilityClass;
};

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;

UCLASS()
class HEAVYHANDED_API ABaseCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABaseCharacter();

	// IAbilitySystemInterface 필수 구현
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;

	// --- 카메라 및 시점 컴포넌트 ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	// --- Enhanced Input Actions ---
	// 에디터에서 블루프린트로 생성한 Input Action 에셋을 할당할 수 있습니다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_MoveForward;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_MoveRight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Turn;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_LookUp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Sprint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Crouch;

	UPROPERTY(EditAnywhere, Category = "Input") 
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	// --- 입력 처리 함수 (Enhanced Input 방식) ---
	// 2차원 축 입력 (WASD 이동 등)
	void MoveForward(const FInputActionValue& Value);
	void MoveRight(const FInputActionValue& Value);

	// 마우스 회전 입력 등
	void Turn(const FInputActionValue& Value);
	void LookUp(const FInputActionValue& Value);


protected:
	// 클라이언트가 입력했을 때 서버로 요청을 보내는 함수
	UFUNCTION(Server, Reliable)
	void Server_ApplyGameplayEffect(TSubclassOf<class UGameplayEffect> EffectClass, bool bApply);

	// 2. ★ [필수] 언리얼 빌드 시스템이 찾는 내부 구현체 함수 (_Implementation 필수 붙이기)
	void Server_ApplyGameplayEffect_Implementation(TSubclassOf<class UGameplayEffect> EffectClass, bool bApply);

	// 기존 입력 함수 수정
	virtual void StartCrouch(const FInputActionValue& Value);
	virtual void StopCrouch(const FInputActionValue& Value);
	virtual void StartSprint(const FInputActionValue& Value);
	virtual void StopSprint(const FInputActionValue& Value);

protected:
	// MovementSpeed 속성이 변할 때 호출될 콜백 함수
	virtual void OnMovementSpeedChanged(const struct FOnAttributeChangeData& Data);

	// ASC 초기화 시 속성 바인딩을 수행할 함수
	void BindAttributeDelegates();

	// 입력에 따라 적용할 달리기/앉기 Gameplay Effect 클래스 (블루프린트에서 지정)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Effects")
	TSubclassOf<class UGameplayEffect> SprintGameplayEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Effects")
	TSubclassOf<class UGameplayEffect> CrouchGameplayEffectClass;

protected:
	void ApplyGameplayEffectToSelf(TSubclassOf<class UGameplayEffect> EffectClass);
	void RemoveGameplayEffectFromSelf(TSubclassOf<class UGameplayEffect> EffectClass);

protected:
	// ★ [통합됨] 에디터 디테일 패널에서 입력과 스킬을 1:1로 매핑하는 리스트 (DefaultAbilities 삭제됨)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS|Abilities")
	TArray<FAbilityInputBinding> AbilityInputBindings;

	// 인프라 입력 감지 시 실행될 콜백 함수
	//void AbilityInputPressed(TSubclassOf<class UGameplayAbility> AbilityClass);
	void AbilityInputPressed(int32 InputID);
	void AbilityInputReleased(int32 InputID);

protected:
	// 현재 손에 들고 있는 액터
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> HeldActor = nullptr;

public:
	// HeldActor를 반환하는 함수
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	AActor* GetHeldActor() const { return HeldActor; }

	// HeldActor를 설정하는 함수
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetHeldActor(AActor* NewHeldActor) { HeldActor = NewHeldActor; }

	// 손에 든 아이템을 내려놓는 함수 (Q키 등에 바인딩용)
	//UFUNCTION(BlueprintCallable, Category = "Interaction")
	//void DropItem();

	// 손에 든 아이템을 던지는 함수 (Impulse 방향/크기로 던짐)
	//UFUNCTION(BlueprintCallable, Category = "Interaction")
	//void ReleaseHeldActor(const FVector& Impulse = FVector::ZeroVector);
public:
	// 서버가 아이템을 집었음을 모든 클라이언트에게 알려주는 함수
	//UFUNCTION(NetMulticast, Reliable)
	//void Multicast_AttachItem(AActor* ItemToAttach);

	//아이템을 내려놓는 과정을 모든 클라이언트에게 동기화하는 함수
	/*UFUNCTION(NetMulticast, Reliable)
	void Multicast_DropItem();*/

	//클라이언트가 서버에게 "아이템 버려줘!"라고 요청하는 서버 RPC
	/*UFUNCTION(Server, Reliable)
	void Server_DropItem();*/

protected:
	/*UFUNCTION(Server, Reliable)
	void Server_ReleaseHeldActor(const FVector& Impulse);*/

	//UFUNCTION(NetMulticast, Reliable)
	//void Multicast_ReleaseHeldActor(const FVector& Impulse);

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
