#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MsHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMsOnDeath, AActor*, DeadActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMsOnHealthChanged, float, NewHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMsOnShieldChanged, float, NewShield, float, Delta);

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

	// --- Shield ---
	//
	// A partial buffer, not an invulnerability layer. Incoming damage is SPLIT: the shield
	// eats a fraction of it and the rest still reaches health. So a shield buys you time and
	// softens spikes, but standing in a swarm still kills you - which keeps the pressure the
	// contact-damage design depends on.
	//
	// Set MaxShield to 0 to disable entirely. That is the default, so clankers are unaffected
	// unless a type is deliberately given one.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Shield", meta = (ClampMin = "0.0"))
	float MaxShield = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_Shield, BlueprintReadOnly, Category = "Meatspace|Shield")
	float Shield = 0.0f;

	/** Fraction of incoming damage the shield absorbs while it lasts. 1.0 would be immunity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Shield", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float ShieldAbsorbFraction = 0.6f;

	/** Seconds of not being hit before the shield starts coming back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Shield", meta = (ClampMin = "0.0"))
	float ShieldRegenDelay = 3.0f;

	/** Shield points restored per second once regeneration starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Shield", meta = (ClampMin = "0.0"))
	float ShieldRegenRate = 12.0f;

	UPROPERTY(BlueprintAssignable, Category = "Meatspace|Shield")
	FMsOnShieldChanged OnShieldChanged;

	UFUNCTION(BlueprintPure, Category = "Meatspace|Shield")
	float GetShieldPercent() const { return MaxShield > 0.0f ? Shield / MaxShield : 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Meatspace|Shield")
	bool HasShield() const { return MaxShield > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Meatspace|Health")
	bool IsDead() const { return Health <= 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Meatspace|Health")
	float GetHealthPercent() const { return MaxHealth > 0.0f ? Health / MaxHealth : 0.0f; }

	/** Server-side heal/reset. */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Health")
	void ResetHealth();

protected:
	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float DamageAmount,
		const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void OnRep_Health();

	UFUNCTION()
	void OnRep_Shield();

private:
	float LastHealth = 0.0f;
	float LastShield = 0.0f;

	/** When damage last landed, for the regeneration delay. */
	float LastDamageTime = -1000.0f;
};
