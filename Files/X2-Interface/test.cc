/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2012 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
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
 * Author: Nicola Baldo <nbaldo@cttc.es>
 */

#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/mobility-module.h>
#include <ns3/internet-module.h>
#include <ns3/lte-module.h>
#include <ns3/config-store-module.h>
#include <ns3/buildings-module.h>
#include <ns3/point-to-point-helper.h>
#include <ns3/applications-module.h>
#include <ns3/log.h>
#include <iomanip>
#include <ios>
#include <string>
#include <vector>
#include "ns3/netanim-module.h"

// The topology of this simulation program is inspired from 
// 3GPP R4-092042, Section 4.2.1 Dual Stripe Model
// note that the term "apartments" used in that document matches with
// the term "room" used in the BuildingsMobilityModel 

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DualTest");

void 
NotifyConnectionEstablishedUe (std::string context, 
                               uint64_t imsi, 
                               uint16_t cellid, 
                               uint16_t rnti)
{
  std::cout << context 
            << " UE IMSI " << imsi 
            << ": connected to CellId " << cellid 
            << " with RNTI " << rnti 
            << std::endl;
}

void 
NotifyHandoverStartUe (std::string context, 
                       uint64_t imsi, 
                       uint16_t cellid, 
                       uint16_t rnti, 
                       uint16_t targetCellId)
{
  std::cout << context 
            << " UE IMSI " << imsi 
            << ": previously connected to CellId " << cellid 
            << " with RNTI " << rnti 
            << ", doing handover to CellId " << targetCellId 
            << std::endl;
}

void 
NotifyHandoverEndOkUe (std::string context, 
                       uint64_t imsi, 
                       uint16_t cellid, 
                       uint16_t rnti)
{
  std::cout << context 
            << " UE IMSI " << imsi 
            << ": successful handover to CellId " << cellid 
            << " with RNTI " << rnti 
            << std::endl;
}

void 
NotifyConnectionEstablishedEnb (std::string context, 
                                uint64_t imsi, 
                                uint16_t cellid, 
                                uint16_t rnti)
{
  std::cout << context 
            << " eNB CellId " << cellid 
            << ": successful connection of UE with IMSI " << imsi 
            << " RNTI " << rnti 
            << std::endl;
}

void 
NotifyHandoverStartEnb (std::string context, 
                        uint64_t imsi, 
                        uint16_t cellid, 
                        uint16_t rnti, 
                        uint16_t targetCellId)
{
  std::cout << context 
            << " eNB CellId " << cellid 
            << ": start handover of UE with IMSI " << imsi 
            << " RNTI " << rnti 
            << " to CellId " << targetCellId 
            << std::endl;
}

void 
NotifyHandoverEndOkEnb (std::string context, 
                        uint64_t imsi, 
                        uint16_t cellid, 
                        uint16_t rnti)
{
  std::cout << context 
            << " eNB CellId " << cellid 
            << ": completed handover of UE with IMSI " << imsi 
            << " RNTI " << rnti 
            << std::endl;
}

bool AreOverlapping (Box a, Box b)
{
        return !((a.xMin > b.xMax) || (b.xMin > a.xMax) || (a.yMin > b.yMax) || (b.yMin > a.yMax));
}

class FemtocellBlockAllocator
{
public:
        FemtocellBlockAllocator (Box area, uint32_t nApartmentsX, uint32_t nFloors);
        void Create (uint32_t n);
        void Create ();

private:
        bool OverlapsWithAnyPrevious (Box);
        Box m_area;
        uint32_t m_nApartmentsX;
        uint32_t m_nFloors;
        std::list<Box> m_previousBlocks;
        double m_xSize;
        double m_ySize;
        Ptr<UniformRandomVariable> m_xMinVar;
        Ptr<UniformRandomVariable> m_yMinVar;
};

FemtocellBlockAllocator::FemtocellBlockAllocator (Box area, uint32_t nApartmentsX, uint32_t nFloors)
  : m_area (area),
    m_nApartmentsX (nApartmentsX),
    m_nFloors (nFloors),
    m_xSize (nApartmentsX*10 + 20),
    m_ySize (70)
{
    m_xMinVar = CreateObject<UniformRandomVariable> ();
    m_xMinVar->SetAttribute ("Min", DoubleValue (area.xMin));
    m_xMinVar->SetAttribute ("Max", DoubleValue (area.xMax - m_xSize));
    m_yMinVar = CreateObject<UniformRandomVariable> ();
    m_yMinVar->SetAttribute ("Min", DoubleValue (area.yMin));
    m_yMinVar->SetAttribute ("Max", DoubleValue (area.yMax - m_ySize));
}

void 
FemtocellBlockAllocator::Create (uint32_t n)
{
  for (uint32_t i = 0; i < n; ++i)
    {
      Create ();
    }
}

void FemtocellBlockAllocator::Create ()
{
  Box box;
  uint32_t attempt = 0;
  do 
    {
      NS_ASSERT_MSG (attempt < 100, "Too many failed attemtps to position apartment block. Too many blocks? Too small area?");
      box.xMin = m_xMinVar->GetValue ();
      box.xMax = box.xMin + m_xSize;
      box.yMin = m_yMinVar->GetValue ();
      box.yMax = box.yMin + m_ySize;      
      ++attempt;
    }
  while (OverlapsWithAnyPrevious (box));  
  NS_LOG_LOGIC ("allocated non overlapping block " << box);
  m_previousBlocks.push_back (box);
  Ptr<GridBuildingAllocator>  gridBuildingAllocator;
  gridBuildingAllocator = CreateObject<GridBuildingAllocator> ();
  gridBuildingAllocator->SetAttribute ("GridWidth", UintegerValue (1));
  gridBuildingAllocator->SetAttribute ("LengthX", DoubleValue (10*m_nApartmentsX));   //length of the wall of each building along X-axis.
  gridBuildingAllocator->SetAttribute ("LengthY", DoubleValue (10*2));
  gridBuildingAllocator->SetAttribute ("DeltaX", DoubleValue (10));          // x space between buildings
  gridBuildingAllocator->SetAttribute ("DeltaY", DoubleValue (10));          // y space between buildings
  gridBuildingAllocator->SetAttribute ("Height", DoubleValue (3*m_nFloors));
  gridBuildingAllocator->SetBuildingAttribute ("NRoomsX", UintegerValue (m_nApartmentsX));
  gridBuildingAllocator->SetBuildingAttribute ("NRoomsY", UintegerValue (2));
  gridBuildingAllocator->SetBuildingAttribute ("NFloors", UintegerValue (m_nFloors));
  gridBuildingAllocator->SetAttribute ("MinX", DoubleValue (box.xMin + 10));          // Initial X-Cord of grid position allocator
  gridBuildingAllocator->SetAttribute ("MinY", DoubleValue (box.yMin + 10));
  gridBuildingAllocator->Create (2);
}

bool 
FemtocellBlockAllocator::OverlapsWithAnyPrevious (Box box)
{
  for (std::list<Box>::iterator it = m_previousBlocks.begin (); it != m_previousBlocks.end (); ++it)
    {
      if (AreOverlapping (*it, box))
        {
          return true;
        }
    }
  return false;
}

void 
PrintGnuplottableBuildingListToFile (std::string filename)
{
  std::ofstream outFile;
  outFile.open (filename.c_str (), std::ios_base::out | std::ios_base::trunc);
  if (!outFile.is_open ())
    {
      NS_LOG_ERROR ("Can't open file " << filename);
      return;
    }
  uint32_t index = 0;
  for (BuildingList::Iterator it = BuildingList::Begin (); it != BuildingList::End (); ++it)
    {
      ++index;
      Box box = (*it)->GetBoundaries ();
      outFile << "set object " << index
              << " rect from " << box.xMin  << "," << box.yMin  
              << " to "   << box.xMax  << "," << box.yMax
              << " front fs empty "
              << std::endl;
    }
}

void 
PrintGnuplottableUeListToFile (std::string filename)
{
  std::ofstream outFile;
  outFile.open (filename.c_str (), std::ios_base::out | std::ios_base::trunc);
  if (!outFile.is_open ())
    {
      NS_LOG_ERROR ("Can't open file " << filename);
      return;
    }
  for (NodeList::Iterator it = NodeList::Begin (); it != NodeList::End (); ++it)
    {
      Ptr<Node> node = *it;
      int nDevs = node->GetNDevices ();
      for (int j = 0; j < nDevs; j++)
        {
          Ptr<LteUeNetDevice> uedev = node->GetDevice (j)->GetObject <LteUeNetDevice> ();
          if (uedev)
            {
              Vector pos = node->GetObject<MobilityModel> ()->GetPosition ();
              outFile << "set label \"" << uedev->GetImsi ()
                      << "\" at "<< pos.x << "," << pos.y << " left font \"Helvetica,4\" textcolor rgb \"grey\" front point pt 1 ps 0.3 lc rgb \"grey\" offset 0,0"  
                      << std::endl;
            }
        }
    }
}

void 
PrintGnuplottableEnbListToFile (std::string filename)
{
  std::ofstream outFile;
  outFile.open (filename.c_str (), std::ios_base::out | std::ios_base::trunc);
  if (!outFile.is_open ())
    {
      NS_LOG_ERROR ("Can't open file " << filename);
      return;
    }
  for (NodeList::Iterator it = NodeList::Begin (); it != NodeList::End (); ++it)
    {
      Ptr<Node> node = *it;
      int nDevs = node->GetNDevices ();
      for (int j = 0; j < nDevs; j++)
        {
          Ptr<LteEnbNetDevice> enbdev = node->GetDevice (j)->GetObject <LteEnbNetDevice> ();
          if (enbdev)
            {
              Vector pos = node->GetObject<MobilityModel> ()->GetPosition ();
              outFile << "set label \"" << enbdev->GetCellId ()
                      << "\" at "<< pos.x << "," << pos.y << " left font \"Helvetica,4\" textcolor rgb \"white\" front  point pt 2 ps 0.3 lc rgb \"white\" offset 0,0"  
                      << std::endl;
            }
        }
    }
}

static ns3::GlobalValue g_nBlocks ("nBlocks", 
                                   "Number of femtocell blocks", 
                                   ns3::UintegerValue (10),
                                   ns3::MakeUintegerChecker<uint32_t> ());
static ns3::GlobalValue g_nApartmentsX ("nApartmentsX", 
                                        "Number of apartments along the X axis in a femtocell block",  
                                        ns3::UintegerValue (5),
                                        ns3::MakeUintegerChecker<uint32_t> ());
static ns3::GlobalValue g_nFloors ("nFloors", 
                                   "Number of floors",  
                                   ns3::UintegerValue (1),
                                   ns3::MakeUintegerChecker<uint32_t> ());
static ns3::GlobalValue g_nMacroEnbSites ("nMacroEnbSites", 
                                          "How many macro sites there are",  
                                          ns3::UintegerValue (1),
                                          ns3::MakeUintegerChecker<uint32_t> ());
static ns3::GlobalValue g_nMacroEnbSitesX ("nMacroEnbSitesX", 
                                           "(minimum) number of sites along the X-axis of the hex grid",  
                                           ns3::UintegerValue (1),
                                           ns3::MakeUintegerChecker<uint32_t> ());
static ns3::GlobalValue g_interSiteDistance ("interSiteDistance", 
                                             "min distance between two nearby macro cell sites",  
                                             ns3::DoubleValue (500),
                                             ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue g_areaMarginFactor ("areaMarginFactor", 
                                            "how much the UE area extends outside the macrocell grid, "
                                            "expressed as fraction of the interSiteDistance",   
                                            ns3::DoubleValue (0.5),
                                            ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue g_macroUeDensity ("macroUeDensity", 
                                          "How many macrocell UEs there are per square meter",   
                                          ns3::DoubleValue (0.000001),
                                          ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue g_homeEnbDeploymentRatio ("homeEnbDeploymentRatio", 
                                                  "The HeNB deployment ratio as per 3GPP R4-092042",   
                                                  ns3::DoubleValue (0.1),
                                                  ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue g_homeEnbActivationRatio ("homeEnbActivationRatio", 
                                                  "The HeNB activation ratio as per 3GPP R4-092042",   
                                                  ns3::DoubleValue (0.1),
                                                  ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue g_homeUesHomeEnbRatio ("homeUesHomeEnbRatio", 
                                               "How many (on average) home UEs per HeNB there are in the simulation",         
                                               ns3::DoubleValue (1.0),
                                               ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue g_macroEnbTxPowerDbm ("macroEnbTxPowerDbm", 
                                              "TX power [dBm] used by macro eNBs",   
                                              ns3::DoubleValue (46.0),
                                              ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue g_homeEnbTxPowerDbm ("homeEnbTxPowerDbm", 
                                             "TX power [dBm] used by HeNBs",   
                                             ns3::DoubleValue (20.0),
                                             ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue g_macroEnbDlEarfcn ("macroEnbDlEarfcn", 
                                            "DL EARFCN used by macro eNBs", 
                                            ns3::UintegerValue (100),
                                            ns3::MakeUintegerChecker<uint16_t> ());
static ns3::GlobalValue g_homeEnbDlEarfcn ("homeEnbDlEarfcn", 
                                           "DL EARFCN used by HeNBs",  
                                           ns3::UintegerValue (100),
                                           ns3::MakeUintegerChecker<uint16_t> ());
static ns3::GlobalValue g_macroEnbBandwidth ("macroEnbBandwidth", 
                                             "bandwdith [num RBs] used by macro eNBs",  
                                             ns3::UintegerValue (25),
                                             ns3::MakeUintegerChecker<uint16_t> ());
static ns3::GlobalValue g_homeEnbBandwidth ("homeEnbBandwidth", 
                                            "bandwdith [num RBs] used by HeNBs",  
                                            ns3::UintegerValue (25),
                                            ns3::MakeUintegerChecker<uint16_t> ());
static ns3::GlobalValue g_simTime ("simTime", 
                                   "Total duration of the simulation [s]",  
                                   ns3::DoubleValue (1),
                                   ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue g_generateRem ("generateRem", 
                                       "if true, will generate a REM and then abort the simulation;"
                                       "if false, will run the simulation normally (without generating any REM)",  
                                       ns3::BooleanValue (false),
                                       ns3::MakeBooleanChecker ());
static ns3::GlobalValue g_epc ("epc", 
                               "If true, will setup the EPC to simulate an end-to-end topology, "
                               "with real IP applications over PDCP and RLC UM (or RLC AM by changing "
                               "the default value of EpsBearerToRlcMapping e.g. to RLC_AM_ALWAYS). "
                               "If false, only the LTE radio access will be simulated with RLC SM. ",  
                               ns3::BooleanValue (true),
                               ns3::MakeBooleanChecker ());
static ns3::GlobalValue g_epcDl ("epcDl", 
                                 "if true, will activate data flows in the downlink when EPC is being used. "
                                 "If false, downlink flows won't be activated. "
                                 "If EPC is not used, this parameter will be ignored.",  
                                 ns3::BooleanValue (true),
                                 ns3::MakeBooleanChecker ());
static ns3::GlobalValue g_epcUl ("epcUl", 
                                 "if true, will activate data flows in the uplink when EPC is being used. "
                                 "If false, uplink flows won't be activated. "
                                 "If EPC is not used, this parameter will be ignored.",  
                                 ns3::BooleanValue (true),
                                 ns3::MakeBooleanChecker ());
static ns3::GlobalValue g_useUdp ("useUdp", 
                                  "if true, the UdpClient application will be used. "
                                  "Otherwise, the BulkSend application will be used over a TCP connection. "
                                  "If EPC is not used, this parameter will be ignored.",  
                                  ns3::BooleanValue (true),
                                  ns3::MakeBooleanChecker ());
static ns3::GlobalValue g_fadingTrace ("fadingTrace", 
                                           "The path of the fading trace (by default no fading trace "
                                           "is loaded, i.e., fading is not considered)",  
                                           ns3::StringValue (""),
                                           ns3::MakeStringChecker ());
static ns3::GlobalValue g_numBearersPerUe ("numBearersPerUe",
                                               "How many bearers per UE there are in the simulation",
                                               ns3::UintegerValue (1),
                                               ns3::MakeUintegerChecker<uint16_t> ());

static ns3::GlobalValue g_srsPeriodicity ("srsPeriodicity",
                                               "SRS Periodicity (has to be at least greater than the number of UEs per eNB)",
                                               ns3::UintegerValue (80),
                                               ns3::MakeUintegerChecker<uint16_t> ());

int 
main (int argc,char *argv[])
{
  fflush(stdin);
  // change some default attributes so that they are reasonable for
  // this scenario, but do this before processing command line
  // arguments, so that the user is allowed to override these settings 
  Config::SetDefault ("ns3::UdpClient::Interval", TimeValue (MilliSeconds(1)));
  Config::SetDefault ("ns3::UdpClient::MaxPackets", UintegerValue(1000000));
  
  CommandLine cmd;
  cmd.Parse (argc, argv);
  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults ();
  // parse again so you can override input file default values via command line
  cmd.Parse (argc, argv); 

  // the scenario parameters get their values from the global attributes defined above
  UintegerValue uintegerValue;
  DoubleValue doubleValue;
  BooleanValue booleanValue;
  StringValue stringValue;
  GlobalValue::GetValueByName ("nBlocks", uintegerValue);
  uint32_t nBlocks = uintegerValue.Get ();
  GlobalValue::GetValueByName ("nApartmentsX", uintegerValue);
  uint32_t nApartmentsX = uintegerValue.Get ();
  GlobalValue::GetValueByName ("nFloors", uintegerValue);
  uint32_t nFloors = uintegerValue.Get ();
  GlobalValue::GetValueByName ("nMacroEnbSites", uintegerValue);
  uint32_t nMacroEnbSites = uintegerValue.Get ();
  GlobalValue::GetValueByName ("nMacroEnbSitesX", uintegerValue);
  uint32_t nMacroEnbSitesX = uintegerValue.Get ();
  GlobalValue::GetValueByName ("interSiteDistance", doubleValue);
  double interSiteDistance = doubleValue.Get ();
  GlobalValue::GetValueByName ("areaMarginFactor", doubleValue);
  double areaMarginFactor = doubleValue.Get ();
  GlobalValue::GetValueByName ("macroUeDensity", doubleValue);
  double macroUeDensity = doubleValue.Get ();
  GlobalValue::GetValueByName ("homeEnbDeploymentRatio", doubleValue);
  double homeEnbDeploymentRatio = doubleValue.Get ();
  GlobalValue::GetValueByName ("homeEnbActivationRatio", doubleValue);
  double homeEnbActivationRatio = doubleValue.Get ();
  GlobalValue::GetValueByName ("homeUesHomeEnbRatio", doubleValue);
  double homeUesHomeEnbRatio = doubleValue.Get ();
  GlobalValue::GetValueByName ("macroEnbTxPowerDbm", doubleValue);
  double macroEnbTxPowerDbm = doubleValue.Get ();
  GlobalValue::GetValueByName ("homeEnbTxPowerDbm", doubleValue);
  double homeEnbTxPowerDbm = doubleValue.Get ();
  GlobalValue::GetValueByName ("macroEnbDlEarfcn", uintegerValue);
  uint16_t macroEnbDlEarfcn = uintegerValue.Get ();
  GlobalValue::GetValueByName ("homeEnbDlEarfcn", uintegerValue);
  uint16_t homeEnbDlEarfcn = uintegerValue.Get ();
  GlobalValue::GetValueByName ("macroEnbBandwidth", uintegerValue);
  uint16_t macroEnbBandwidth = uintegerValue.Get ();
  GlobalValue::GetValueByName ("homeEnbBandwidth", uintegerValue);
  uint16_t homeEnbBandwidth = uintegerValue.Get ();
  GlobalValue::GetValueByName ("simTime", doubleValue);
  double simTime = doubleValue.Get ();
  GlobalValue::GetValueByName ("epc", booleanValue);
  bool epc = booleanValue.Get ();
  GlobalValue::GetValueByName ("epcDl", booleanValue);
  bool epcDl = booleanValue.Get ();
  GlobalValue::GetValueByName ("epcUl", booleanValue);
  bool epcUl = booleanValue.Get ();
  GlobalValue::GetValueByName ("useUdp", booleanValue);
  bool useUdp = booleanValue.Get ();
  GlobalValue::GetValueByName ("generateRem", booleanValue);
  bool generateRem = booleanValue.Get ();
  GlobalValue::GetValueByName ("fadingTrace", stringValue);
  std::string fadingTrace = stringValue.Get ();
  GlobalValue::GetValueByName ("numBearersPerUe", uintegerValue);
  uint16_t numBearersPerUe = uintegerValue.Get ();
  GlobalValue::GetValueByName ("srsPeriodicity", uintegerValue);
  //uint16_t srsPeriodicity = uintegerValue.Get ();

  //Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue(srsPeriodicity));

  Box macroUeBox;

  if (nMacroEnbSites > 0)
  {
      uint32_t currentSite = nMacroEnbSites -1;
      uint32_t biRowIndex = (currentSite / (nMacroEnbSitesX + nMacroEnbSitesX + 1));
      uint32_t biRowRemainder = currentSite % (nMacroEnbSitesX + nMacroEnbSitesX + 1);
      uint32_t rowIndex = biRowIndex*2 + 1;
      if (biRowRemainder >= nMacroEnbSitesX)
        {
          ++rowIndex;
        }
      uint32_t nMacroEnbSitesY = rowIndex;
      NS_LOG_LOGIC ("nMacroEnbSitesY = " << nMacroEnbSitesY);

      macroUeBox = Box (-areaMarginFactor*interSiteDistance, 
                        (nMacroEnbSitesX + areaMarginFactor)*interSiteDistance, 
                        -areaMarginFactor*interSiteDistance, 
                        (nMacroEnbSitesY -1)*interSiteDistance*sqrt(0.75) + areaMarginFactor*interSiteDistance,
                        1.0, 2.0);
  }
  else
  {
        // still need the box to place femtocell blocks
        macroUeBox = Box (0, 150, 0, 150, 1.0, 2.0);
  }
  FemtocellBlockAllocator blockAllocator (macroUeBox, nApartmentsX, nFloors);
  blockAllocator.Create (nBlocks);

  uint32_t nHomeEnbs = round (4 * nApartmentsX * nBlocks * nFloors * homeEnbDeploymentRatio * homeEnbActivationRatio);
  NS_LOG_LOGIC ("nHomeEnbs = " << nHomeEnbs);
  uint32_t nHomeUes = round (nHomeEnbs * homeUesHomeEnbRatio);
  NS_LOG_LOGIC ("nHomeUes = " << nHomeUes);
  double macroUeAreaSize = (macroUeBox.xMax - macroUeBox.xMin) * (macroUeBox.yMax - macroUeBox.yMin);
  uint32_t nMacroUes = round (macroUeAreaSize * macroUeDensity) ;
  NS_LOG_LOGIC ("nMacroUes = " << nMacroUes << " (density=" << macroUeDensity << ")");
  
  std::cout<<"Femto's: "<<nHomeEnbs<<"\n";
  std::cout<<"Inside UE's: "<<nHomeUes<<"\n";
  std::cout<<"Outside UE's: "<<nMacroUes<<"\n";
  NodeContainer homeEnbs;
  homeEnbs.Create (nHomeEnbs);
  NodeContainer macroEnbs;
  macroEnbs.Create (3 * nMacroEnbSites);
  NodeContainer homeUes;
  homeUes.Create (nHomeUes);
  NodeContainer macroUes;
  macroUes.Create (nMacroUes);
  std::cout<<"Macro Enb's: "<<macroEnbs.GetN()<<"\n";
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::BuildingsMobilityModel");

  Ptr <LteHelper> lteHelper = CreateObject<LteHelper> ();
  lteHelper->SetAttribute ("PathlossModel", StringValue ("ns3::HybridBuildingsPropagationLossModel"));
  lteHelper->SetPathlossModelAttribute ("ShadowSigmaExtWalls", DoubleValue (0));
  lteHelper->SetPathlossModelAttribute ("ShadowSigmaOutdoor", DoubleValue (1));
  lteHelper->SetPathlossModelAttribute ("ShadowSigmaIndoor", DoubleValue (1.5));
  // use always LOS model
  lteHelper->SetPathlossModelAttribute ("Los2NlosThr", DoubleValue (1e6));
  lteHelper->SetSpectrumChannelType ("ns3::MultiModelSpectrumChannel");
 
//   lteHelper->EnableLogComponents ();
//   LogComponentEnable ("PfFfMacScheduler", LOG_LEVEL_ALL);
  
  if (!fadingTrace.empty ())
    {
      lteHelper->SetAttribute ("FadingModel", StringValue ("ns3::TraceFadingLossModel"));
      lteHelper->SetFadingModelAttribute("TraceFilename", StringValue (fadingTrace));
    }

  Ptr<EpcHelper> epcHelper;
  if (epc)
    {
      NS_LOG_LOGIC ("enabling EPC");
      epcHelper = CreateObject<EpcHelper> ();
      lteHelper->SetEpcHelper (epcHelper);      
    }

  // Macro eNBs in 3-sector hex grid
  int i;
  Ptr<BuildingsMobilityModel> mm1;
  mobility.Install (macroEnbs);
  for(i=0;i<(int)macroEnbs.GetN();i++){
        mm1 = macroEnbs.Get (i)->GetObject<BuildingsMobilityModel> ();
        mm1->m_vel=Vector(0,0,0);
        mm1->constraint=false;
  }
  Ptr<LteHexGridEnbTopologyHelper> lteHexGridEnbTopologyHelper = CreateObject<LteHexGridEnbTopologyHelper> ();
  lteHexGridEnbTopologyHelper->SetLteHelper (lteHelper);
  lteHexGridEnbTopologyHelper->SetAttribute ("InterSiteDistance", DoubleValue (interSiteDistance));
  lteHexGridEnbTopologyHelper->SetAttribute ("MinX", DoubleValue (interSiteDistance/2));
  lteHexGridEnbTopologyHelper->SetAttribute ("GridWidth", UintegerValue (nMacroEnbSitesX));
  Config::SetDefault ("ns3::LteEnbPhy::TxPower", DoubleValue (macroEnbTxPowerDbm));
  lteHelper->SetEnbAntennaModelType ("ns3::ParabolicAntennaModel");
  lteHelper->SetEnbAntennaModelAttribute ("Beamwidth",   DoubleValue (70));
  lteHelper->SetEnbAntennaModelAttribute ("MaxAttenuation",     DoubleValue (20.0));
  lteHelper->SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (macroEnbDlEarfcn));
  lteHelper->SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (macroEnbDlEarfcn + 18000));
  lteHelper->SetEnbDeviceAttribute ("DlBandwidth", UintegerValue (macroEnbBandwidth));
  lteHelper->SetEnbDeviceAttribute ("UlBandwidth", UintegerValue (macroEnbBandwidth));
  NetDeviceContainer macroEnbDevs = lteHexGridEnbTopologyHelper->SetPositionAndInstallEnbDevice (macroEnbs);
  
  // HomeEnbs randomly indoor
    
  Ptr<PositionAllocator> positionAlloc = CreateObject<RandomRoomPositionAllocator> ();
  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (homeEnbs);
   
  Vector pos;
  //NS_LOG_LOGIC ("installing mobility for HomeEnbs"<<mm1);
  for(i=0;i<(int)homeEnbs.GetN();i++){
        mm1 = homeEnbs.Get (i)->GetObject<BuildingsMobilityModel> ();
        mm1->m_vel=Vector(0,0,0);
        mm1->constraint=false;
  }
  Config::SetDefault ("ns3::LteEnbPhy::TxPower", DoubleValue (homeEnbTxPowerDbm));
  lteHelper->SetEnbAntennaModelType ("ns3::IsotropicAntennaModel");
  lteHelper->SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (homeEnbDlEarfcn));
  lteHelper->SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (homeEnbDlEarfcn + 18000));
  lteHelper->SetEnbDeviceAttribute ("DlBandwidth", UintegerValue (homeEnbBandwidth));
  lteHelper->SetEnbDeviceAttribute ("UlBandwidth", UintegerValue (homeEnbBandwidth));
  NetDeviceContainer homeEnbDevs  = lteHelper->InstallEnbDevice (homeEnbs);
    
  // macro Ues 
  NS_LOG_LOGIC ("randomly allocating macro UEs in " << macroUeBox);
 // positionAlloc = CreateObject<RandomBoxPositionAllocator> ();
  Ptr<UniformRandomVariable> xVal = CreateObject<UniformRandomVariable> ();
  xVal->SetAttribute ("Min", DoubleValue (macroUeBox.xMin));
  xVal->SetAttribute ("Max", DoubleValue (macroUeBox.xMax));
  //positionAlloc->SetAttribute ("X", PointerValue (xVal));
  Ptr<UniformRandomVariable> yVal = CreateObject<UniformRandomVariable> ();
  yVal->SetAttribute ("Min", DoubleValue (macroUeBox.yMin));
  yVal->SetAttribute ("Max", DoubleValue (macroUeBox.yMax));
  //positionAlloc->SetAttribute ("Y", PointerValue (yVal));
  Ptr<UniformRandomVariable> zVal = CreateObject<UniformRandomVariable> ();
  zVal->SetAttribute ("Min", DoubleValue (macroUeBox.zMin));
  zVal->SetAttribute ("Max", DoubleValue (macroUeBox.zMax));
  //positionAlloc->SetAttribute ("Z", PointerValue (zVal));
  //mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (macroUes);
  NetDeviceContainer macroUeDevs = lteHelper->InstallUeDevice (macroUes);
  //std::cout<<"kjdkjf\n";
  NS_LOG_LOGIC ("installing mobility for MacroUes");
  for(i=0;i<(int)macroUes.GetN();i++){
        mm1 = macroUes.Get (i)->GetObject<BuildingsMobilityModel> ();
        mm1->constraint=false;
        mm1->m_vel=Vector(10,10,10);
        pos=Vector(xVal->GetValue(),yVal->GetValue(),zVal->GetValue()); 
        std::cout<<"\n"<<pos.x<<".."<<pos.y<<".."<<pos.z<<"\n";      
        mm1->SetPosition (pos);
  }
  

  // home UEs located in the same apartment in which there are the Home eNBs
  positionAlloc = CreateObject<SameRoomPositionAllocator> (homeEnbs);
  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (homeUes);
  for(i=0;i<(int)homeUes.GetN();i++){
        mm1 = homeUes.Get (i)->GetObject<BuildingsMobilityModel> ();
        mm1->constraint=true;
        mm1->m_vel=Vector(25,50,0);
  }
  NetDeviceContainer homeUeDevs = lteHelper->InstallUeDevice (homeUes);
  
  Ipv4Address remoteHostAddr;
  NodeContainer ues;
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ipv4InterfaceContainer ueIpIfaces;
  Ptr<Node> remoteHost;
  NetDeviceContainer ueDevs;
  if (epc)
  {
      NS_LOG_LOGIC ("setting up internet and remote host");
  
      // Create a single RemoteHost
      NodeContainer remoteHostContainer;
      remoteHostContainer.Create (1);
      remoteHost = remoteHostContainer.Get (0);
      InternetStackHelper internet;
      internet.Install (remoteHostContainer);
      MobilityHelper mobility1;
      mobility1.SetMobilityModel ("ns3::BuildingsMobilityModel");
      mobility1.Install(remoteHostContainer);
      mm1 = remoteHostContainer.Get (0)->GetObject<BuildingsMobilityModel> ();
      mm1->m_vel=Vector(0,-1,1);
      mm1->constraint=false;
       mm1->SetPosition (Vector (2,6,5));
      // Create the Internet
      PointToPointHelper p2ph;
      p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
      p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
      p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
      Ptr<Node> pgw = epcHelper->GetPgwNode ();
      mobility1.SetMobilityModel ("ns3::BuildingsMobilityModel");
      mobility1.Install(pgw);
      mm1 = pgw->GetObject<BuildingsMobilityModel> ();
      mm1->m_vel=Vector(0,0,0);
      mm1->constraint=false;
      mm1->SetPosition (Vector (100,50,5));
      NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
      Ipv4AddressHelper ipv4h;
      ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
      Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
      // in this container, interface 0 is the pgw, 1 is the remoteHost
      remoteHostAddr = internetIpIfaces.GetAddress (1);

      Ipv4StaticRoutingHelper ipv4RoutingHelper;
      Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
      remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

      // for internetworking purposes, consider together home UEs and macro UEs
      ues.Add (homeUes);
      ues.Add (macroUes);
      ueDevs.Add (homeUeDevs);
      ueDevs.Add (macroUeDevs);      

      // Install the IP stack on the UEs      
      internet.Install (ues);
      ueIpIfaces = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueDevs));
    }

  // attachment (needs to be done after IP stack configuration)
  // macro UEs attached to the closest macro eNB
  lteHelper->AttachToClosestEnb (macroUeDevs, macroEnbDevs);
  // each home UE is ttach explicitly to its home eNB
  NetDeviceContainer::Iterator ueDevIt;
  NetDeviceContainer::Iterator enbDevIt = homeEnbDevs.Begin ();

  for (ueDevIt = homeUeDevs.Begin ();
       ueDevIt != homeUeDevs.End ();
       ++ueDevIt, ++enbDevIt)
    {
      // this because of the order in which SameRoomPositionAllocator
      // will place the UEs
      if (enbDevIt == homeEnbDevs.End ())
        {
          enbDevIt = homeEnbDevs.Begin ();
        }
      lteHelper->Attach (*ueDevIt, *enbDevIt);
    }

  if (epc)
    {
      NS_LOG_LOGIC ("setting up applications");
    
      // Install and start applications on UEs and remote host
      uint16_t dlPort = 10000;
      uint16_t ulPort = 20000;

      // randomize a bit start times to avoid simulation artifacts
      // (e.g., buffer overflows due to packet transmissions happening
      // exactly at the same time) 
      Ptr<UniformRandomVariable> startTimeSeconds = CreateObject<UniformRandomVariable> ();
      startTimeSeconds->SetAttribute ("Min", DoubleValue (0));
      startTimeSeconds->SetAttribute ("Max", DoubleValue (0.010));
     
      for (uint32_t u = 0; u < ues.GetN (); ++u)
        {
          Ptr<Node> ue = ues.Get (u);
          // Set the default gateway for the UE
          Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ue->GetObject<Ipv4> ());
          ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

          for (uint32_t b = 0; b < numBearersPerUe; ++b)
            {
              ++dlPort;
              ++ulPort;

              ApplicationContainer clientApps;
              ApplicationContainer serverApps;

              if (useUdp)
                {              
                  if (epcDl)
                    {
                      NS_LOG_LOGIC ("installing UDP DL app for UE " << u);
                      UdpClientHelper dlClientHelper (ueIpIfaces.GetAddress (u), dlPort);
                      clientApps.Add (dlClientHelper.Install (remoteHost));
                      PacketSinkHelper dlPacketSinkHelper ("ns3::UdpSocketFactory", 
                                                           InetSocketAddress (Ipv4Address::GetAny (), dlPort));
                      serverApps.Add (dlPacketSinkHelper.Install (ue));
                    }
                  if (epcUl)
                    {      
                      NS_LOG_LOGIC ("installing UDP UL app for UE " << u);
                      UdpClientHelper ulClientHelper (remoteHostAddr, ulPort);
                      clientApps.Add (ulClientHelper.Install (ue));
                      PacketSinkHelper ulPacketSinkHelper ("ns3::UdpSocketFactory", 
                                                           InetSocketAddress (Ipv4Address::GetAny (), ulPort));
                      serverApps.Add (ulPacketSinkHelper.Install (remoteHost));  
                    }            
                }                    
              else // use TCP
                {
                  if (epcDl)
                    {
                      NS_LOG_LOGIC ("installing TCP DL app for UE " << u);
                      BulkSendHelper dlClientHelper ("ns3::TcpSocketFactory",
                                                     InetSocketAddress (ueIpIfaces.GetAddress (u), dlPort));
                      dlClientHelper.SetAttribute ("MaxBytes", UintegerValue (0));
                      clientApps.Add (dlClientHelper.Install (remoteHost));
                      PacketSinkHelper dlPacketSinkHelper ("ns3::TcpSocketFactory", 
                                                           InetSocketAddress (Ipv4Address::GetAny (), dlPort));
                      serverApps.Add (dlPacketSinkHelper.Install (ue));
                    }
                  if (epcUl)
                    {     
                      NS_LOG_LOGIC ("installing TCP UL app for UE " << u);              
                      BulkSendHelper ulClientHelper ("ns3::TcpSocketFactory",
                                                     InetSocketAddress (remoteHostAddr, ulPort));
                      ulClientHelper.SetAttribute ("MaxBytes", UintegerValue (0));                  
                      clientApps.Add (ulClientHelper.Install (ue));
                      PacketSinkHelper ulPacketSinkHelper ("ns3::TcpSocketFactory", 
                                                           InetSocketAddress (Ipv4Address::GetAny (), ulPort));
                      serverApps.Add (ulPacketSinkHelper.Install (remoteHost));
                    }
                } // end if (useUdp)

              Ptr<EpcTft> tft = Create<EpcTft> ();
              if (epcDl)
                {
                  EpcTft::PacketFilter dlpf;
                  dlpf.localPortStart = dlPort;
                  dlpf.localPortEnd = dlPort;
                  tft->Add (dlpf); 
                }
              if (epcUl)
                {
                  EpcTft::PacketFilter ulpf;
                  ulpf.remotePortStart = ulPort;
                  ulpf.remotePortEnd = ulPort;
                  tft->Add (ulpf);
                }

              if (epcDl || epcUl)
                {
                  EpsBearer bearer (EpsBearer::NGBR_VIDEO_TCP_DEFAULT);
                  lteHelper->ActivateDedicatedEpsBearer (ueDevs.Get (u), bearer, tft);
                }
              Time startTime = Seconds (startTimeSeconds->GetValue ());
              serverApps.Start (startTime);
              clientApps.Start (startTime);

            } // end for b
        }

    } 
  else // (epc == false)
    {
      // for radio bearer activation purposes, consider together home UEs and macro UEs
      NetDeviceContainer ueDevs;
      ueDevs.Add (homeUeDevs);
      ueDevs.Add (macroUeDevs);  
      for (uint32_t u = 0; u < ueDevs.GetN (); ++u)
        {
          Ptr<NetDevice> ueDev = ueDevs.Get (u);
          for (uint32_t b = 0; b < numBearersPerUe; ++b)
            {
              enum EpsBearer::Qci q = EpsBearer::NGBR_VIDEO_TCP_DEFAULT;
              EpsBearer bearer (q);
              //lteHelper->ActivateDataRadioBearer (ueDev, bearer);
            }
        }
    }

  BuildingsHelper::MakeMobilityModelConsistent ();
  /*for(uint32_t i=0;i < nHomeEnbs;i++)
  {
   for(uint32_t j=0;i < nHomeEnbs && i!=j;j++)
  lteHelper->AddX2Interface (homeEnbs.Get(i),homeEnbs.Get(j));
}*/
 lteHelper->AddX2Interface (homeEnbs.Get(0),homeEnbs.Get(1));
 lteHelper->HandoverRequest (Seconds (0.30), homeUeDevs.Get (0), homeEnbDevs.Get (0), homeEnbDevs.Get (1));

  Ptr<RadioEnvironmentMapHelper> remHelper;
  if (generateRem)
    {
      PrintGnuplottableBuildingListToFile ("buildings.txt");
      PrintGnuplottableEnbListToFile ("enbs.txt");
      PrintGnuplottableUeListToFile ("ues.txt");

      remHelper = CreateObject<RadioEnvironmentMapHelper> ();
      remHelper->SetAttribute ("ChannelPath", StringValue ("/ChannelList/0"));
      remHelper->SetAttribute ("OutputFile", StringValue ("lena-dual-stripe.rem"));
      remHelper->SetAttribute ("XMin", DoubleValue (macroUeBox.xMin));
      remHelper->SetAttribute ("XMax", DoubleValue (macroUeBox.xMax));
      remHelper->SetAttribute ("YMin", DoubleValue (macroUeBox.yMin));
      remHelper->SetAttribute ("YMax", DoubleValue (macroUeBox.yMax));
      remHelper->SetAttribute ("Z", DoubleValue (1.5));
      remHelper->Install ();
      // simulation will stop right after the REM has been generated
    }
  else
    {
      Simulator::Stop (Seconds (simTime));  
    }
  AnimationInterface anim ("animation.xml");
  lteHelper->EnableMacTraces ();
  lteHelper->EnableRlcTraces ();
  if (epc)
  {
      lteHelper->EnablePdcpTraces ();
  }
  Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/ConnectionEstablished",
                   MakeCallback (&NotifyConnectionEstablishedEnb));
  Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/ConnectionEstablished",
                   MakeCallback (&NotifyConnectionEstablishedUe));
  Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/HandoverStart",
                   MakeCallback (&NotifyHandoverStartEnb));
  Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/HandoverStart",
                   MakeCallback (&NotifyHandoverStartUe));
  Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/HandoverEndOk",
                   MakeCallback (&NotifyHandoverEndOkEnb));
  Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/HandoverEndOk",
                   MakeCallback (&NotifyHandoverEndOkUe));

  Simulator::Stop (Seconds(5));
  
  Simulator::Run ();

  //GtkConfigStore config;
  //config.ConfigureAttributes ();

  lteHelper = 0;
  Simulator::Destroy ();
  return 0;
}
