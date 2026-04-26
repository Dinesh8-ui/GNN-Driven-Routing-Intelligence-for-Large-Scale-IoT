#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main() {
    NodeContainer nodes;
    nodes.Create(60);

    OlsrHelper olsr;
    InternetStackHelper internet;
    internet.SetRoutingHelper(olsr);
    internet.Install(nodes);

    // Simple UDP traffic
    UdpEchoServerHelper server(9);
    ApplicationContainer apps = server.Install(nodes.Get(1));
    apps.Start(Seconds(1.0));

    UdpEchoClientHelper client(nodes.Get(1), 9);
    client.SetAttribute("MaxPackets", UintegerValue(100));
    client.SetAttribute("Interval", TimeValue(Seconds(0.1)));

    apps = client.Install(nodes.Get(0));
    apps.Start(Seconds(2.0));

    Simulator::Run();
    Simulator::Destroy();
}