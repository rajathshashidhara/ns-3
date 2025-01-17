/*
 * Copyright (c) 2014 Piotr Gawlowicz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Piotr Gawlowicz <gawlowicz.p@gmail.com>
 *
 */

#ifndef LTE_FR_STRICT_ALGORITHM_H
#define LTE_FR_STRICT_ALGORITHM_H

#include <ns3/lte-ffr-algorithm.h>
#include <ns3/lte-ffr-rrc-sap.h>
#include <ns3/lte-ffr-sap.h>
#include <ns3/lte-rrc-sap.h>

#include <map>

namespace ns3
{

/**
 * \brief Strict Frequency Reuse algorithm implementation
 */
class LteFrStrictAlgorithm : public LteFfrAlgorithm
{
  public:
    /**
     * \brief Creates a trivial ffr algorithm instance.
     */
    LteFrStrictAlgorithm();

    ~LteFrStrictAlgorithm() override;

    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    // inherited from LteFfrAlgorithm
    void SetLteFfrSapUser(LteFfrSapUser* s) override;
    LteFfrSapProvider* GetLteFfrSapProvider() override;

    void SetLteFfrRrcSapUser(LteFfrRrcSapUser* s) override;
    LteFfrRrcSapProvider* GetLteFfrRrcSapProvider() override;

    /// let the forwarder class access the protected and private members
    friend class MemberLteFfrSapProvider<LteFrStrictAlgorithm>;
    /// let the forwarder class access the protected and private members
    friend class MemberLteFfrRrcSapProvider<LteFrStrictAlgorithm>;

  protected:
    // inherited from Object
    void DoInitialize() override;
    void DoDispose() override;

    void Reconfigure() override;

    // FFR SAP PROVIDER IMPLEMENTATION
    std::vector<bool> DoGetAvailableDlRbg() override;
    bool DoIsDlRbgAvailableForUe(int i, uint16_t rnti) override;
    std::vector<bool> DoGetAvailableUlRbg() override;
    bool DoIsUlRbgAvailableForUe(int i, uint16_t rnti) override;
    void DoReportDlCqiInfo(
        const struct FfMacSchedSapProvider::SchedDlCqiInfoReqParameters& params) override;
    void DoReportUlCqiInfo(
        const struct FfMacSchedSapProvider::SchedUlCqiInfoReqParameters& params) override;
    void DoReportUlCqiInfo(std::map<uint16_t, std::vector<double>> ulCqiMap) override;
    uint8_t DoGetTpc(uint16_t rnti) override;
    uint16_t DoGetMinContinuousUlBandwidth() override;

    // FFR SAP RRC PROVIDER IMPLEMENTATION
    void DoReportUeMeas(uint16_t rnti, LteRrcSap::MeasResults measResults) override;
    void DoRecvLoadInformation(EpcX2Sap::LoadInformationParams params) override;

  private:
    /**
     * Set downlink configuration
     *
     * \param cellId the cell ID
     * \param bandwidth the bandwidth
     */
    void SetDownlinkConfiguration(uint16_t cellId, uint8_t bandwidth);
    /**
     * Set uplink configuration
     *
     * \param cellId the cell ID
     * \param bandwidth the bandwidth
     */
    void SetUplinkConfiguration(uint16_t cellId, uint8_t bandwidth);
    /**
     * Initialize downlink RBG maps
     */
    void InitializeDownlinkRbgMaps();
    /**
     * Initialize uplink RBG maps
     */
    void InitializeUplinkRbgMaps();

    // FFR SAP
    LteFfrSapUser* m_ffrSapUser;         ///< FFR SAP user
    LteFfrSapProvider* m_ffrSapProvider; ///< FFR SAP provider

    // FFR RRF SAP
    LteFfrRrcSapUser* m_ffrRrcSapUser;         ///< FFR RRC SAP user
    LteFfrRrcSapProvider* m_ffrRrcSapProvider; ///< FFR RRC SAP provider

    uint8_t m_dlCommonSubBandwidth; ///< DL common subbandwidth
    uint8_t m_dlEdgeSubBandOffset;  ///< DL edge subband offset
    uint8_t m_dlEdgeSubBandwidth;   ///< DL edge subbandwidth

    uint8_t m_ulCommonSubBandwidth; ///< UL common subbandwidth
    uint8_t m_ulEdgeSubBandOffset;  ///< UL edge subband offset
    uint8_t m_ulEdgeSubBandwidth;   ///< UL edge subbandwidth

    std::vector<bool> m_dlRbgMap; ///< DL RBG map
    std::vector<bool> m_ulRbgMap; ///< UL RBG map

    std::vector<bool> m_dlEdgeRbgMap; ///< DL edge RBG map
    std::vector<bool> m_ulEdgeRbgMap; ///< UL edge RBG map

    /// SubBand enumeration
    enum SubBand
    {
        AreaUnset,
        CellCenter,
        CellEdge
    };

    std::map<uint16_t, uint8_t> m_ues; ///< UEs
    std::vector<uint16_t> m_edgeUes;   ///< Edge UEs

    uint8_t m_edgeSubBandThreshold; ///< Edge subband threshold

    uint8_t m_centerAreaPowerOffset; ///< center area power offset
    uint8_t m_edgeAreaPowerOffset;   ///< edge area power offset

    uint8_t m_centerAreaTpc; ///< center area tpc
    uint8_t m_edgeAreaTpc;   ///< edge area tpc

    /// The expected measurement identity
    uint8_t m_measId;

}; // end of class LteFrStrictAlgorithm

} // end of namespace ns3

#endif /* LTE_FR_STRICT_ALGORITHM_H */
