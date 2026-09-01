SKSE_PLUGIN_PRELOAD(const SKSE::PreLoadInterface* a_skse)
{
	SKSE::Init(a_skse);

	REX::INFO("Hello PreLoaded World!");

	return true;
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);

	REX::INFO("Hello Loaded World!");

	return true;
}
