//------------------------------------------------------------------------------------------------
//! MechCore logging. Info messages are OFF by default so servers and players get a clean log.
//! Warnings and errors (real setup problems) always print.
//!
//! Turn info logging on with ANY of:
//!   - launch parameter  -bfaMechDebug
//!   - BFA_MechCoreComponent > m_bDebugLogging on any mech
//!   - code: BFA_MechLog.SetEnabled(true)
//! Components with their own m_bDebug / m_bDebugAim switches still print when those are on.
class BFA_MechLog
{
	protected static bool s_bChecked;
	protected static bool s_bEnabled;

	//------------------------------------------------------------------------------------------------
	static void SetEnabled(bool enabled)
	{
		s_bChecked = true;
		s_bEnabled = enabled;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsEnabled()
	{
		if (!s_bChecked)
		{
			s_bChecked = true;
			s_bEnabled = System.IsCLIParam("bfaMechDebug");
		}

		return s_bEnabled;
	}

	//------------------------------------------------------------------------------------------------
	static void Log(string message, LogLevel level = LogLevel.NORMAL)
	{
		if (level >= LogLevel.WARNING || IsEnabled())
			Print(message, level);
	}
}
