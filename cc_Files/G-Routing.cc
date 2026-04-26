// dsdv-simulation.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DSDVSimulation");

int main (int argc, char *argv[])
{
    uint32_t numNodes = 10;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // WiFi setup
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=10]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2]"),
                             "PositionAllocator",
                             StringValue("ns3::RandomRectanglePositionAllocator"));
    mobility.Install(nodes);

    // DSDV Routing
    DsdvHelper dsdv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(dsdv);
    stack.Install(nodes);

    // Assign IP
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Applications (UDP traffic)
    uint16_t port = 9;

    OnOffHelper onoff("ns3::UdpSocketFactory",
                      Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    onoff.SetConstantRate(DataRate("1Mbps"));

    ApplicationContainer app = onoff.Install(nodes.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}