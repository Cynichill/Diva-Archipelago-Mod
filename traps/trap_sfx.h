#pragma once
#include "trap.h"

namespace TrapSFX
{
	class _TrapSFX : public Trap
	{
	public:
		void config(const toml::table& settings);
		void save(toml::table& settings);
		void resetSFX();
		void reset();
		void touch();
		void tick();
		void ImGuiConfig();
		void ImGuiStatus();
		void ImGuiExpose();

	private:
		float rerollInterval = 0.0f;
		float timestampRollNext = 0.0f;

		char prevButton[32] = "";
		char prevChance[32] = "";
		char prevSlide[32] = "";
		char prevChainFirst[32] = "";
		char prevChainSub[32] = "";
		char prevChainSuccess[32] = "";
		char prevChainFailure[32] = "";
	};

	extern _TrapSFX trap;
}
