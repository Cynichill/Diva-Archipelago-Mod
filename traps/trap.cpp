#include "trap.h"

void Trap::config(const toml::table& settings)
{
}

void Trap::save(toml::table& settings)
{
}

void Trap::reset()
{
	running = false;
	timestamp = 0.0f;
}

bool Trap::isRunning()
{
	return running;
}

void Trap::touch()
{
}

void Trap::tick()
{
}

void Trap::ImGuiConfig()
{
}

void Trap::ImGuiStatus()
{
}

void Trap::ImGuiExpose()
{
}
