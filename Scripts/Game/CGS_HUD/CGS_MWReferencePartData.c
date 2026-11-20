// Read-only part health adapter for the opt-in reference HUD.
// Reuses the SAME part scanning/section classification as CGS_MechPartsPanel
// without allocating the old lower-left built-in HUD widgets.
class CGS_MWReferencePartData : CGS_MechPartsPanel
{
    void Poll(IEntity mech)
    {
        if (!mech)
            return;

        float now = Now();
        if (m_pMech != mech || now >= m_fNextList)
        {
            m_pMech = mech;
            m_fNextList = now + 2.0;
            m_fNextHealth = 0;
            CollectParts(mech);
        }

        if (m_aHas.Count() != SECTIONS)
        {
            m_aHas.Clear();
            m_aHealth.Clear();
            m_aDead.Clear();
            for (int i = 0; i < SECTIONS; i++)
            {
                m_aHas.Insert(false);
                m_aHealth.Insert(1.0);
                m_aDead.Insert(false);
            }
        }

        if (now < m_fNextHealth)
            return;

        m_fNextHealth = now + 0.25;
        ReadHealth(mech);
    }

    bool HasPart(int section)
    {
        return section >= 0 && section < m_aHas.Count() && m_aHas[section];
    }

    float PartHealth(int section)
    {
        if (section < 0 || section >= m_aHealth.Count())
            return -1;
        return m_aHealth[section];
    }

    bool PartDead(int section)
    {
        return section >= 0 && section < m_aDead.Count() && m_aDead[section];
    }
}
