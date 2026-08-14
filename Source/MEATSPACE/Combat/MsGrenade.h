#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MsGrenade.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/**
 * Electrical grenade. Arcs out, sticks its fuse, then discharges in a radius.
 *
 * Radial damage with falloff rather than a flat blast, so positioning matters: catching a
 * swarm dead-centre should be meaningfully better than clipping its edge. That is what makes
 * it an answer to being surrounded rather than just extra damage.
 *
 * Does not hurt the thrower by default. Friendly fire on your own grenade is a punishment
 * mechanic, and this weapon exists to relieve pressure, not add it.
 */
UCLASS()
class MEATSPACE_API AMsGrenade : public AActor
{
	GENERATED_BODY()

public:
	AMsGrenade();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

	/** Server-side. Applies the damage and tells everyone to draw the discharge. */
	void Explode();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastExplodeFX(const FVector_NetQuantize& Location);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meatspace|Grenade")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meatspace|Grenade")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meatspace|Grenade")
	TObjectPtr<UProjectileMovementComponent> Movement;

	/** Damage at the centre of the blast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Grenade", meta = (ClampMin = "0.0"))
	float Damage = 70.0f;

	/** Full damage out to here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Grenade", meta = (ClampMin = "0.0"))
	float InnerRadius = 250.0f;

	/** Damage falls off to MinDamageFraction by here, and stops entirely beyond it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Grenade", meta = (ClampMin = "1.0"))
	float OuterRadius = 800.0f;

	/** Damage at the outer edge, as a fraction of the centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Grenade", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinDamageFraction = 0.35f;

	/** Seconds before it goes off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Grenade", meta = (ClampMin = "0.0"))
	float FuseTime = 1.4f;

	/** Detonate the moment it touches anything, instead of waiting for the fuse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Grenade")
	bool bExplodeOnImpact = false;

	/** Whether the thrower can be caught in their own blast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Grenade")
	bool bDamageThrower = false;

	/** How many lightning arcs to draw to nearby victims. Cosmetic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Grenade", meta = (ClampMin = "0"))
	int32 MaxVisualArcs = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Grenade|Debug")
	bool bDrawDebugBlast = true;

private:
	FTimerHandle FuseTimer;
	bool bExploded = false;
};
