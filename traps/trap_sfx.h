#pragma once
#include "trap.h"

class TrapSFX : public Trap
{
public:
	TrapSFX() : Trap("SFX") {}

	void config(const toml::table& settings);
	void save(toml::table& settings);
	void reset();
	void touch();
	void tick();
	void ImGuiConfig();
	void ImGuiStatus();

private:
	float rerollInterval = 0.0f;
	float timestampRollNext = 0.0f;

	char prevButton[32];
	char prevSlide[32];
	char prevChainFirst[32];
	char prevChainSub[32];
	char prevChainSuccess[32];
	char prevChainFailure[32];
};
