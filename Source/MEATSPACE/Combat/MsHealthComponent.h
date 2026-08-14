#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MsHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMsOnDeath, AActor*, DeadActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMsOnHealthChanged, float, NewHealth, float, Delta);

/**
 * Health, damage and death for anything that can be killed.
 *
 * Lives in one place so clankers, bosses and eventually players all die the same way. Hooks
 * the owner's damage delegate rather than requiring every actor to override TakeDamage.
 *
 * Server-authoritative: only the server mutates Health. Clients receive it by replication.
 */
UCLASS(ClassGroup = (Meatspace), meta = (BlueprintSpawnableComponent))
class MEATSPACE_API UMsHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMsHealthComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 60.0f;

	UPROPERTY(ReplicatedUsing = OnRep_Health, BlueprintReadOnly, Category = "Meatspace|Health")
	float Health = 60.0f;

	/** Fires on the server the moment health reaches zero. */
	UPROPERTY(BlueprintAssignable, Category = "Meatspace|Health")
	FMsOnDeath OnDeath;

	/** Fires on both server and clients whenever health changes. */
	UPROPERTY(BlueprintAssignable, Category = "Meatspace|Health")
	FMsOnHealthChanged OnHealthChanged;

	UFUNCTION(BlueprintPure, Category = "Meatspace|Health")
	bool IsDead() const { return Health <= 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Meatspace|Health")
	float GetHealthPercent() const { return MaxHealth > 0.0f ? Health / MaxHealth : 0.0f; }

	/** Server-side heal/reset. */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Health")
	void ResetHealth();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float DamageAmount,
		const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void OnRep_Health();

private:
	float LastHealth = 0.0f;
};
