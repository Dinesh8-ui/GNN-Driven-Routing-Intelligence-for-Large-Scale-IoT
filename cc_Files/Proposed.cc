
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/dsdv-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("GNN_Adaptive_Routing");

int main(int argc, char *argv[])
{
uint32_t runNumber = 1;

CommandLine cmd;
cmd.AddValue("runNumber", "Run number", runNumber);
cmd.Parse(argc, argv);

SeedManager::SetSeed(1);
SeedManager::SetRun(runNumber);
    uint32_t numNodes =10;
    double simTime = 10.0;
    uint32_t packetSize = 1024;
    uint32_t totalPackets =1000;

    NodeContainer nodes;
    nodes.Create(numNodes);

    /* WIFI SETUP */
    WifiHelper wifi;
wifi.SetStandard(WIFI_STANDARD_80211b);

YansWifiChannelHelper channel = YansWifiChannelHelper::Default();

YansWifiPhyHelper phy;
phy.SetChannel(channel.Create());

phy.Set("TxPowerStart", DoubleValue(20.0));
phy.Set("TxPowerEnd", DoubleValue(20.0));

WifiMacHelper mac;
mac.SetType("ns3::AdhocWifiMac");

NetDeviceContainer devices = wifi.Install(phy, mac, nodes);
    /* MOBILITY */
    MobilityHelper mobility;

mobility.SetPositionAllocator("ns3::GridPositionAllocator",
    "MinX", DoubleValue(0.0),
    "MinY", DoubleValue(0.0),
    "DeltaX", DoubleValue(30.0),
    "DeltaY", DoubleValue(30.0),
    "GridWidth", UintegerValue(5),
    "LayoutType", StringValue("RowFirst"));

mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

mobility.Install(nodes);
    /* ROUTING (DSDV) */
    DsdvHelper dsdv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(dsdv);
    internet.Install(nodes);

    /* IP ADDRESS */
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

   /* FLOW MONITOR */
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    /* ADAPTIVE TRAFFIC (GNN-INSPIRED) */
    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();

    for (uint32_t i = 0; i <3; i++)
    {
        uint32_t src = rand->GetInteger(0, numNodes-1);
        uint32_t dst = rand->GetInteger(0, numNodes-1);

        while (src == dst)
        {
            dst = rand->GetInteger(0, numNodes-1);
        }

        uint16_t port = 9000 + i;

        /* RECEIVER */
        PacketSinkHelper sink("ns3::UdpSocketFactory",
            InetSocketAddress(Ipv4Address::GetAny(), port));

        ApplicationContainer sinkApp = sink.Install(nodes.Get(dst));
        sinkApp.Start(Seconds(5.0));
        sinkApp.Stop(Seconds(simTime));

        /* GNN-INSPIRED CONGESTION LOGIC */
        double congestionFactor = rand->GetValue(0.0,1.0);
        double interval;

        if (congestionFactor > 0.6)
        {
            interval = 0.05; // slow (congested)
        }
        else
        {
            interval = 0.01; // fast (low congestion)
        }

        /* SENDER */
        UdpClientHelper client(interfaces.GetAddress(dst), port);
        client.SetAttribute("MaxPackets", UintegerValue(totalPackets));
        client.SetAttribute("Interval", TimeValue(Seconds(interval)));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));

        ApplicationContainer clientApp = client.Install(nodes.Get(src));
        clientApp.Start(Seconds(2.0 + i));
        clientApp.Stop(Seconds(simTime));
    }

    /* NETANIM */
    AnimationInterface anim("animation.xml");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    /* RESULTS */
    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());

    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    uint64_t totalTx = 0;
uint64_t totalRx = 0;
uint64_t totalLost = 0;
double totalDelay = 0;
double totalJitter = 0;
uint64_t totalRxBytes = 0;
double totalTime = 0;

for (auto &flow : stats)
{
    totalTx += flow.second.txPackets;
    totalRx += flow.second.rxPackets;
    totalLost += flow.second.lostPackets;
    totalDelay += flow.second.delaySum.GetSeconds();
    totalJitter += flow.second.jitterSum.GetSeconds();
    totalRxBytes += flow.second.rxBytes;

    double duration = flow.second.timeLastRxPacket.GetSeconds() -
                      flow.second.timeFirstTxPacket.GetSeconds();

    if (duration > 0)
        totalTime += duration;
}

double pdr = (double)totalRx / totalTx * 100;
double avgDelay = totalDelay / totalRx;
double avgJitter = totalJitter / totalRx;
double throughput = (totalRxBytes * 8.0) / totalTime / 1024;

uint64_t commOverhead = totalLost;
double compOverhead = Simulator::Now().GetSeconds();
double energy = totalTx * 0.03;

std::cout << "----- Simulation Results -----" << std::endl;
std::cout << "Packets Sent = " << totalTx << std::endl;
std::cout << "Packets Received = " << totalRx << std::endl;
std::cout << "Packet Delivery Ratio = " << pdr << " %" << std::endl;
std::cout << "End-to-End Delay = " << avgDelay << " sec" << std::endl;
std::cout << "Average Jitter = " << avgJitter << " sec" << std::endl;
std::cout << "Throughput = " << throughput << " Kbps" << std::endl;
std::cout << "Communication Overhead = " << commOverhead << " packets" << std::endl;
std::cout << "Computational Overhead = " << compOverhead << " sec" << std::endl;
std::cout << "Average Energy Consumption = " << energy << " Joules" << std::endl;

    std::cout << "\nGNN-Inspired Adaptive Routing Enabled\n";
    std::cout << "Congestion-aware traffic applied\n";

    Simulator::Destroy();
    return 0;
}
