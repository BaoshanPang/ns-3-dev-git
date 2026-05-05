/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

// One media server sends video-like TCP downloads to many clients.
//
// Topology:
//
//                  media server
//                       |
//                    router 1
//              to router 2   to router 3
//              router 2      router 3
//              clients       clients
//
// Each client receives one TCP flow from the server.  OnOffApplication is used
// as a simple video stream/download source with a configurable per-client
// bitrate.  The default congestion control is TcpCubic, and clients start
// slightly staggered to avoid creating all sockets at the same simulation
// instant.  The example writes aggregate receive throughput to
// tcp-cubic-media-server-throughput.dat.

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <fstream>
#include <iomanip>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpCubicMediaServerExample");

namespace
{

std::ofstream g_throughput;
uint64_t g_previousTotalRx = 0;
uint64_t g_previousR1TotalRx = 0;
Time g_previousSampleTime;
Time g_sampleInterval;
Ptr<FlowMonitor> g_monitor;
Ptr<Ipv4FlowClassifier> g_classifier;

void
TraceAggregateThroughput()
{
    const Time now = Simulator::Now();
    FlowMonitor::FlowStatsContainer stats = g_monitor->GetFlowStats();

    uint64_t totalRx = 0;
    uint64_t r1TotalRx = 0;

    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        Ipv4FlowClassifier::FiveTuple t = g_classifier->FindFlow(it->first);
        // Only count traffic originating from the server (Forward path)
        if (t.sourceAddress == Ipv4Address("10.0.0.1"))
        {
            totalRx += it->second.rxBytes;
            r1TotalRx += it->second.rxBytes;
        }
    }

    if (now > g_previousSampleTime)
    {
        const double mbps = (totalRx - g_previousTotalRx) * 8.0 /
                            (now - g_previousSampleTime).ToDouble(Time::S) / 1000000.0;
        const double r1Mbps = (r1TotalRx - g_previousR1TotalRx) * 8.0 /
                              (now - g_previousSampleTime).ToDouble(Time::S) / 1000000.0;
        g_throughput << std::fixed << std::setprecision(3) << now.GetSeconds() << " " << mbps << " "
                     << r1Mbps << std::endl;
    }

    g_previousTotalRx = totalRx;
    g_previousR1TotalRx = r1TotalRx;
    g_previousSampleTime = now;
    Simulator::Schedule(g_sampleInterval, &TraceAggregateThroughput);
}

} // namespace

int
main(int argc, char* argv[])
{
    uint32_t nClients = 10;
    uint64_t videoBytes = 0;
    uint32_t packetSize = 1448;
    std::string videoRate = "100Mbps";
    std::string tcpTypeId = "TcpCubic";
    std::string serverLinkRate = "10000Mbps";
    std::string serverLinkDelay = "2ms";
    std::string routerLinkRate = "1000Mbps";
    std::string routerLinkDelay = "5ms";
    std::string clientLinkRate = "100Mbps";
    std::string clientLinkDelay = "20ms";
    Time startTime = Seconds(1);
    Time clientStartStagger = MilliSeconds(2);
    Time stopTime = Seconds(10);
    g_sampleInterval = Seconds(1);
    bool enablePcap = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nClients", "Number of video clients", nClients);
    cmd.AddValue("videoBytes", "Bytes sent to each client; 0 means unlimited", videoBytes);
    cmd.AddValue("packetSize", "Video packet payload size in bytes", packetSize);
    cmd.AddValue("videoRate", "Per-client application send rate", videoRate);
    cmd.AddValue("tcpTypeId", "TCP congestion control, e.g., TcpCubic or TcpBbr", tcpTypeId);
    cmd.AddValue("serverLinkRate", "Data rate of the media server to router 1 link", serverLinkRate);
    cmd.AddValue("serverLinkDelay", "Delay of the media server to router 1 link", serverLinkDelay);
    cmd.AddValue("routerLinkRate", "Data rate of the router 1 to router 2/3 links", routerLinkRate);
    cmd.AddValue("routerLinkDelay", "Delay of the router 1 to router 2/3 links", routerLinkDelay);
    cmd.AddValue("clientLinkRate", "Data rate of each router-to-client access link", clientLinkRate);
    cmd.AddValue("clientLinkDelay", "Delay of each router-to-client access link", clientLinkDelay);
    cmd.AddValue("startTime", "Time when the first client download starts", startTime);
    cmd.AddValue("clientStartStagger", "Delay between consecutive client starts", clientStartStagger);
    cmd.AddValue("stopTime", "Application stop time", stopTime);
    cmd.AddValue("sampleInterval", "Aggregate throughput sample interval", g_sampleInterval);
    cmd.AddValue("enablePcap", "Enable pcap tracing on the shared server link", enablePcap);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::" + tcpTypeId));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(16 * 1024 * 1024));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(16 * 1024 * 1024));
    Config::SetDefault("ns3::TcpSocketBase::UseEcn", EnumValue(TcpSocketState::On));

    NodeContainer server;
    NodeContainer routers;
    NodeContainer clients;
    server.Create(1);
    routers.Create(3);
    clients.Create(nClients);

    InternetStackHelper internet;
    internet.Install(server);
    internet.Install(routers);
    internet.Install(clients);

    PointToPointHelper serverLink;
    serverLink.SetDeviceAttribute("DataRate", StringValue(serverLinkRate));
    serverLink.SetChannelAttribute("Delay", StringValue(serverLinkDelay));

    PointToPointHelper routerLink;
    routerLink.SetDeviceAttribute("DataRate", StringValue(routerLinkRate));
    routerLink.SetChannelAttribute("Delay", StringValue(routerLinkDelay));

    PointToPointHelper clientLink;
    clientLink.SetDeviceAttribute("DataRate", StringValue(clientLinkRate));
    clientLink.SetChannelAttribute("Delay", StringValue(clientLinkDelay));

    NetDeviceContainer serverDevices = serverLink.Install(server.Get(0), routers.Get(0));
    NetDeviceContainer r1r2Devices = routerLink.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer r1r3Devices = routerLink.Install(routers.Get(0), routers.Get(2));

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::FifoQueueEcnDisc", "MarkThreshold", DoubleValue(0.1));
//    tch.Install(serverDevices.Get(1));
    tch.Install(r1r2Devices.Get(0));
    tch.Install(r1r3Devices.Get(0));

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.252");
    address.Assign(serverDevices);
    address.NewNetwork();
    address.Assign(r1r2Devices);
    address.NewNetwork();
    address.Assign(r1r3Devices);
    address.NewNetwork();

    std::vector<Ipv4Address> clientAddresses;
    clientAddresses.reserve(nClients);
    for (uint32_t i = 0; i < nClients; ++i)
    {
        Ptr<Node> accessRouter = routers.Get(i % 2 == 0 ? 1 : 2);
        NodeContainer pair(accessRouter, clients.Get(i));
        NetDeviceContainer devices = clientLink.Install(pair);
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        clientAddresses.push_back(interfaces.GetAddress(1));
        address.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    FlowMonitorHelper flowmon;
    g_monitor = flowmon.InstallAll();
    g_classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());

    ApplicationContainer sinkApps;
    ApplicationContainer sourceApps;
    const uint16_t basePort = 9000;

    for (uint32_t i = 0; i < nClients; ++i)
    {
        const uint16_t port = basePort + i;
        PacketSinkHelper sink("ns3::TcpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sink.Install(clients.Get(i));
        sinkApps.Add(sinkApp);

        OnOffHelper source("ns3::TcpSocketFactory", InetSocketAddress(clientAddresses[i], port));
        source.SetAttribute("DataRate", DataRateValue(DataRate(videoRate)));
        source.SetAttribute("PacketSize", UintegerValue(packetSize));
        source.SetAttribute("MaxBytes", UintegerValue(videoBytes));
        source.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
        source.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
        ApplicationContainer sourceApp = source.Install(server.Get(0));
        sourceApp.Start(startTime + i * clientStartStagger);
        sourceApps.Add(sourceApp);
    }

    sinkApps.Start(Seconds(0));
    sinkApps.Stop(stopTime + Seconds(1));
    sourceApps.Stop(stopTime);

    if (enablePcap)
    {
        serverLink.EnablePcap("tcp-cubic-media-server", serverDevices.Get(0), true);
    }

    g_throughput.open("tcp-cubic-media-server-throughput.dat");
    g_throughput << "# time(s) aggregate-rx-throughput(Mbps) r1-aggregate-throughput(Mbps)"
                 << std::endl;
    g_previousSampleTime = Seconds(0);
    Simulator::Schedule(g_sampleInterval, &TraceAggregateThroughput);

    Simulator::Stop(stopTime + Seconds(1));
    Simulator::Run();

    FlowMonitor::FlowStatsContainer stats = g_monitor->GetFlowStats();
    uint64_t totalRx = 0;
    uint64_t r1TotalRx = 0;

    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        Ipv4FlowClassifier::FiveTuple t = g_classifier->FindFlow(it->first);
        // Only count traffic originating from the server (Forward path)
        if (t.sourceAddress == Ipv4Address("10.0.0.1"))
        {
            totalRx += it->second.rxBytes;
            r1TotalRx += it->second.rxBytes;
        }
    }

    const double averageMbps = totalRx * 8.0 / (stopTime - startTime).ToDouble(Time::S) / 1000000.0;
    const double r1AverageMbps =
        r1TotalRx * 8.0 / (stopTime - startTime).ToDouble(Time::S) / 1000000.0;
    std::cout << "Clients: " << nClients << std::endl;
    std::cout << "Total bytes received: " << totalRx << std::endl;
    std::cout << "Average aggregate receive throughput: " << averageMbps << " Mbps" << std::endl;
    std::cout << "Total bytes seen by R1: " << r1TotalRx << std::endl;
    std::cout << "Average aggregate throughput seen by R1: " << r1AverageMbps << " Mbps"
              << std::endl;
    std::cout << "Throughput trace: tcp-cubic-media-server-throughput.dat" << std::endl;

    Simulator::Destroy();
    g_throughput.close();

    return 0;
}
