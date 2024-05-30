/*
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
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WLANInfraStructureMode");

void
CourseChange (std::string context, Ptr<const MobilityModel> model)
{
Vector position = model->GetPosition ();
NS_LOG_UNCOND (context <<
" x = " << position.x << ", y = " << position.y);
}

int
main(int argc, char* argv[])
{
    bool verbose = true;
    uint32_t nWifi = 6;
    bool tracing = false;
    bool useRts = true;
    CommandLine cmd(__FILE__);
    
    cmd.AddValue("nWifi", "Number of wifi STA devices", nWifi);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);

    cmd.Parse(argc, argv);
    
    // The underlying restriction of 18 is due to the grid position
    // allocator's configuration; the grid layout will exceed the
    // bounding box if more than 18 nodes are provided.
    if (nWifi > 18)
    {
        std::cout << "nWifi should be 18 or less; otherwise grid layout exceeds the bounding box"
                  << std::endl;
        return 1;
    }

    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }
    
    
    if (useRts)
    {
    Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",
    StringValue ("0"));
    }
    
    NodeContainer wifiApNode;  //added to create ApNode
    wifiApNode.Create (1);
    
    NodeContainer wifiNodes;
    wifiNodes.Create(nWifi);
    
    
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");
    
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    
    
    WifiMacHelper mac;
    Ssid ssid = Ssid("PNET");
       
    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(phy, mac, wifiNodes);

    NetDeviceContainer apDevice;
    mac.SetType ("ns3::ApWifiMac",
    "Ssid", SsidValue (ssid));
    apDevice = wifi.Install (phy, mac, wifiApNode);
    
    
    
    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(0.0),
                                  "MinY",
                                  DoubleValue(0.0),
                                  "DeltaX",
                                  DoubleValue(5.0),
                                  "DeltaY",
                                  DoubleValue(10.0),
                                  "GridWidth",
                                  UintegerValue(3),
                                  "LayoutType",
                                  StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds",
                              RectangleValue(Rectangle(-100, 100, -100, 100)));
                              
    mobility.Install(wifiNodes);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    InternetStackHelper stack;
    
    stack.Install(wifiNodes);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;

    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces=address.Assign(staDevices);
    Ipv4InterfaceContainer wifiApInterfaces= address.Assign (apDevice);
     
    UdpEchoServerHelper echoServer(20);
    ApplicationContainer serverApps = echoServer.Install(wifiNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));
    
    // Install UDP echo client at node 5
    UdpEchoClientHelper echoClient(wifiInterfaces.GetAddress(0), 20);
    echoClient.SetAttribute("MaxPackets", UintegerValue(2));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(wifiNodes.Get(5));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Install UDP echo client at node 4
    UdpEchoClientHelper echoClient2(wifiInterfaces.GetAddress(0), 20);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(2));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps2 = echoClient2.Install(wifiNodes.Get(4));
    clientApps2.Start(Seconds(3.0));
    clientApps2.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    
    phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap (useRts ? "rtscts-AT22" : "nortscts-AT22", staDevices.Get (5), true);
    std::ostringstream oss;
    oss <<"/NodeList/" << wifiNodes.Get (5)->GetId ()
    <<"/$ns3::MobilityModel/CourseChange";
    phy.EnablePcap (useRts ? "rtscts-AT22" : "nortscts-AT22", apDevice.Get(0), true);
    Config::Connect (oss.str (), MakeCallback (&CourseChange));
    
    Simulator::Stop(Seconds(10.0));

    
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
