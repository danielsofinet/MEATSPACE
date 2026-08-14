#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MsTargetDummy.generated.h"

class UStaticMeshComponent;

/**
 * A cube that takes damage and dies. Nothing more.
 *
 * This exists so we can see and feel the gun working before any clanker exists. It is the
 * cheapest possible answer to "did the shot actually register, on the server, for everyone?"
 */
UCLASS()
class MEATSPACE_API AMsTargetDummy : public AActor
{
	GENERATED_BODY()

public:
	AMsTargetDummy();

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	/**
	 * Tick this on to turn a static cube into a moving one. Handy for testing whether the
	 * sword and gun feel different against something that will not hold still.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Target|Movement")
	bool bChasePlayer = true;

	/** Slower than the small clankers - these read as the heavy ones. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Target|Movement", meta = (ClampMin = "0.0"))
	float ChaseSpeed = 260.0f;

	/** Stops closing once this near the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Target|Movement", meta = (ClampMin = "0.0"))
	float ChaseStopDistance = 150.0f;

	/** Ride the floor rather than sitting flush in it - see MsClankerBase for why. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Target|Movement")
	bool bSnapToGround = true;

	/** Height of the actor origin above the traced floor. The engine cube is 100 tall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Target|Movement", meta = (ClampMin = "0.0"))
	float GroundOffset = 52.0f;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meatspace|Target")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Target", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	/** Replicated so every client sees the same health, not just the shooter. */
	UPROPERTY(ReplicatedUsing = OnRep_Health, BlueprintReadOnly, Category = "Meatspace|Target")
	float Health = 100.0f;

	/** Destroy on death, or reset and keep taking hits. Handy for tuning feel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Target")
	bool bRespawnOnDeath = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Target", meta = (ClampMin = "0.1"))
	float RespawnDelay = 2.0f;

	UFUNCTION()
	void OnRep_Health();

	void ShowHealthFeedback();

private:
	void Revive();

	FTimerHandle RespawnTimer;
};
