#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/basic-energy-source-helper.h"
#include "ns3/wifi-radio-energy-model-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{

    uint32_t runNumber = 1;

    CommandLine cmd;
    cmd.AddValue("run","Run number",runNumber);
    cmd.Parse(argc,argv);

    RngSeedManager::SetSeed(1);
    RngSeedManager::SetRun(runNumber);

    uint32_t numNodes = 10;
    double simTime = 20.0;
    uint32_t packetSize = 128;

    uint32_t totalPackets = 500;

    NodeContainer nodes;
    nodes.Create(numNodes);

    /* WIFI */

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy,mac,nodes);

    /* MOBILITY */

    MobilityHelper mobility;

    Ptr<RandomRectanglePositionAllocator> positionAlloc =
    CreateObject<RandomRectanglePositionAllocator>();

    positionAlloc->SetX(CreateObjectWithAttributes<UniformRandomVariable>(
        "Min",DoubleValue(0.0),
        "Max",DoubleValue(100.0)));

    positionAlloc->SetY(CreateObjectWithAttributes<UniformRandomVariable>(
        "Min",DoubleValue(0.0),
        "Max",DoubleValue(100.0)));

    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel(
        "ns3::RandomWaypointMobilityModel",
        "Speed",StringValue("ns3::UniformRandomVariable[Min=5|Max=20]"),
        "Pause",StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
        "PositionAllocator",PointerValue(positionAlloc));

    mobility.Install(nodes);

    /* ROUTING */

    DsdvHelper dsdv;

    InternetStackHelper internet;
    internet.SetRoutingHelper(dsdv);
    internet.Install(nodes);

    /* IP ADDRESS */

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0","255.255.255.0");

    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    /* ENERGY MODEL */

    BasicEnergySourceHelper energySource;
    energySource.Set("BasicEnergySourceInitialEnergyJ",DoubleValue(25));

    ns3::energy::EnergySourceContainer sources =
    energySource.Install(nodes);

    WifiRadioEnergyModelHelper radioEnergy;
    radioEnergy.Install(devices,sources);

    /* TRAFFIC FLOWS */

    Ptr<UniformRandomVariable> randNode =
    CreateObject<UniformRandomVariable>();

    uint32_t numFlows = numNodes/2;

    uint32_t packetsPerFlow = totalPackets / numFlows;

    uint16_t basePort = 9000;

    for(uint32_t i=0;i<numFlows;i++)
    {

        uint32_t src = randNode->GetInteger(0,numNodes-1);
        uint32_t dst = randNode->GetInteger(0,numNodes-1);

        while(src==dst)
        {
            dst = randNode->GetInteger(0,numNodes-1);
        }

        uint16_t port = basePort + i;

        /* RECEIVER */

        PacketSinkHelper sink(
        "ns3::UdpSocketFactory",
        Address(InetSocketAddress(Ipv4Address::GetAny(),port)));

        ApplicationContainer sinkApp =
        sink.Install(nodes.Get(dst));

        sinkApp.Start(Seconds(0.5));
        sinkApp.Stop(Seconds(simTime));

        /* SENDER */

        UdpClientHelper client(
        interfaces.GetAddress(dst),port);

        client.SetAttribute("MaxPackets",
        UintegerValue(packetsPerFlow));

        client.SetAttribute("Interval",
        TimeValue(Seconds(0.02)));

        client.SetAttribute("PacketSize",
        UintegerValue(packetSize));

        ApplicationContainer clientApp =
        client.Install(nodes.Get(src));

        clientApp.Start(Seconds(1.0 + i));
        clientApp.Stop(Seconds(simTime));
    }

    /* FLOW MONITOR */

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor =
    flowmon.InstallAll();

    /* NETANIM */

    AnimationInterface anim("dsdv-animation.xml");

    /* RUN */

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();

    std::map<FlowId,FlowMonitor::FlowStats> stats =
    monitor->GetFlowStats();

    double totalDelay=0;
    double totalJitter=0;

    uint32_t txPackets=0;
    uint32_t rxPackets=0;
    uint32_t lostPackets=0;

    double throughput=0;

    for(auto &flow:stats)
    {
        txPackets+=flow.second.txPackets;
        rxPackets+=flow.second.rxPackets;
        lostPackets+=flow.second.lostPackets;

        totalDelay+=flow.second.delaySum.GetSeconds();
        totalJitter+=flow.second.jitterSum.GetSeconds();

        throughput+=flow.second.rxBytes*8.0/simTime/1024;
    }

    double avgDelay=0;
    double avgJitter=0;

    if(rxPackets>0)
    {
        avgDelay=totalDelay/rxPackets;
        avgJitter=totalJitter/rxPackets;
    }

    double pdr = (double)rxPackets/txPackets*100;

    double communicationOverhead = lostPackets;

    double computationalOverhead = avgDelay * rxPackets;

    double totalEnergy=0;

    for(ns3::energy::EnergySourceContainer::Iterator i=sources.Begin();
        i!=sources.End();++i)
    {
        totalEnergy += (25-(*i)->GetRemainingEnergy());
    }

    double avgEnergy = totalEnergy/numNodes;

    /* OUTPUT */

    std::cout<<"\n----- Simulation Results -----\n";

    std::cout<<"Packets Sent = "<<txPackets<<std::endl;
    std::cout<<"Packets Received = "<<rxPackets<<std::endl;

    std::cout<<"Packet Delivery Ratio (PDR) = "<<pdr<<" %\n";

    std::cout<<"End-to-End Delay = "<<avgDelay<<" sec\n";

    std::cout<<"Average Jitter = "<<avgJitter<<" sec\n";

    std::cout<<"Throughput = "<<throughput<<" Kbps\n";

    std::cout<<"Communication Overhead = "<<communicationOverhead<<" pkts\n";

    std::cout<<"Computational Overhead = "<<computationalOverhead<<" sec per pkt\n";

    std::cout<<"Average Energy Consumption = "<<avgEnergy<<" Joules\n";

    Simulator::Destroy();

    return 0;
}
