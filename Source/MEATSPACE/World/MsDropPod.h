#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MsDropPod.generated.h"

class AMsClankerBase;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMsOnClankerDelivered, AMsClankerBase*, Clanker);

/**
 * A mini dropship. Falls from the sky, slams into the ground, opens, and unloads clankers.
 *
 * Solves a readability problem as much as a fiction one: clankers appearing out of nowhere
 * gives the player no warning and no reason. A pod falling for a second and a half telegraphs
 * exactly where the next group lands and buys them a moment to reposition - which turns
 * escalation from something that happens TO you into something you can respond to.
 *
 * The delay between impact and unloading matters too. It is the beat where you can see the
 * thing has landed and know what is coming.
 */
UCLASS()
class MEATSPACE_API AMsDropPod : public AActor
{
	GENERATED_BODY()

public:
	AMsDropPod();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Sends the pod in. Call immediately after spawning: it lifts itself to DropHeight above
	 * the impact point and falls from there.
	 */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|DropPod")
	void Deliver(const TArray<TSubclassOf<AMsClankerBase>>& Payload, const FVector& ImpactLocation);

	/** Fires per clanker unloaded, so the encounter that sent it can track them. */
	UPROPERTY(BlueprintAssignable, Category = "Meatspace|DropPod")
	FMsOnClankerDelivered OnClankerDelivered;

protected:
	void Impact();
	void ReleasePayload();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meatspace|DropPod")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** How high above the impact point it starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|DropPod", meta = (ClampMin = "100.0"))
	float DropHeight = 4500.0f;

	/** Fall speed in cm/s. Fast enough to feel like a drop, slow enough to see coming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|DropPod", meta = (ClampMin = "100.0"))
	float DropSpeed = 3800.0f;

	/** Beat between the slam and the doors opening. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|DropPod", meta = (ClampMin = "0.0"))
	float OpenDelay = 0.7f;

	/** How far from the pod the clankers appear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|DropPod", meta = (ClampMin = "0.0"))
	float PayloadSpread = 220.0f;

	/** Camera trauma on impact, falling off with distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|DropPod", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ImpactShake = 0.55f;

	/** Beyond this distance the impact is not felt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|DropPod", meta = (ClampMin = "100.0"))
	float ShakeFalloffDistance = 3500.0f;

	/** How long the empty pod stays before disappearing. 0 leaves it forever. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|DropPod", meta = (ClampMin = "0.0"))
	float DespawnDelay = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|DropPod|Debug")
	bool bDrawDebugImpact = true;

private:
	UPROPERTY(Transient)
	TArray<TSubclassOf<AMsClankerBase>> PendingPayload;

	FVector TargetLocation = FVector::ZeroVector;
	bool bFalling = false;
	bool bLanded = false;

	FTimerHandle OpenTimer;
	FTimerHandle DespawnTimer;
};
