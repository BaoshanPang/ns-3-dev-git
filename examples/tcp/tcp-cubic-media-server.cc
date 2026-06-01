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
std::ofstream g_router1_throughput;
uint64_t g_r1BytesTotal = 0; // Accumulator for Router 1 Rx bytes
uint64_t g_lastR1Bytes = 0;  // Router 1 bytes from previous sample
std::ofstream g_server_throughput;

uint64_t g_serverBytesTotal = 0; // Accumulator for server Tx bytes
uint64_t g_lastServerBytes = 0;  // Server bytes from previous sample

std::ofstream g_clients_throughput;
// Map to store previous Rx bytes per flow ID
std::map<FlowId, uint64_t> g_prevFlowRx;
Time g_previousSampleTime;
Time g_sampleInterval;
Ptr<FlowMonitor> g_monitor;
Ptr<Ipv4FlowClassifier> g_classifier;

void
TraceThroughput()
{
    const Time now = Simulator::Now();
    FlowMonitor::FlowStatsContainer stats = g_monitor->GetFlowStats();

    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        Ipv4FlowClassifier::FiveTuple t = g_classifier->FindFlow(it->first);

        // Filter for traffic originating from the server
        if (t.sourceAddress == Ipv4Address("10.0.0.1"))
        {
            uint64_t currentRx = it->second.rxBytes;
            uint64_t prevRx = g_prevFlowRx[it->first]; // Defaults to 0 if not found

            if (now > g_previousSampleTime)
            {
                // Calculate throughput in Mbps
                double mbps = (currentRx - prevRx) * 8.0 /
                              (now - g_previousSampleTime).ToDouble(Time::S) / 1000000.0;

                // Output format: timestamp "client ip" throughput
                g_clients_throughput << std::fixed << std::setprecision(3) << now.GetSeconds()
                                     << " \"" << t.destinationAddress << "\" " << mbps << std::endl;
            }
            g_prevFlowRx[it->first] = currentRx;
        }
    }

    // 2. Calculate aggregate server throughput using the trace source
    if (now > g_previousSampleTime)
    {
        uint64_t diff = g_serverBytesTotal - g_lastServerBytes;
        double serverMbps =
            (diff * 8.0) / (now - g_previousSampleTime).ToDouble(Time::S) / 1000000.0;

        g_server_throughput << std::fixed << std::setprecision(3) << now.GetSeconds() << " "
                            << serverMbps << std::endl;

        g_lastServerBytes = g_serverBytesTotal;
    }

    // 3. Calculate Router 1 throughput
    if (now > g_previousSampleTime)
    {
        uint64_t diff = g_r1BytesTotal - g_lastR1Bytes;
        double r1Mbps = (diff * 8.0) / (now - g_previousSampleTime).ToDouble(Time::S) / 1000000.0;

        g_router1_throughput << std::fixed << std::setprecision(3) << now.GetSeconds() << " "
                             << r1Mbps << std::endl;

        g_lastR1Bytes = g_r1BytesTotal;
    }

    g_previousSampleTime = now;
    Simulator::Schedule(g_sampleInterval, &TraceThroughput);
}

// Callback for NetDevice transmit trace
void
ServerTransmitSink(Ptr<const Packet> p)
{
    g_serverBytesTotal += p->GetSize();
}

// Callback for Router 1 receiving packets from server
void
Router1ReceiveSink(Ptr<const Packet> p)
{
    g_r1BytesTotal += p->GetSize();
}

std::ofstream g_server_rtt;

// Callback for TCP RTT changes
void
RttTracer(Time oldRtt, Time newRtt)
{
    g_server_rtt << std::fixed << std::setprecision(3) << Simulator::Now().GetSeconds() << " "
                 << newRtt.GetMilliSeconds() << std::endl;
}

void
TraceRtt()
{
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/*/RTT",
                                  MakeCallback(&RttTracer));
}

std::ofstream g_router1_queue;

void
TraceQueueSize(Ptr<QueueDisc> queueDisc)
{
    const Time now = Simulator::Now();

    // Get the number of packets in the first internal queue
    uint32_t size = queueDisc->GetInternalQueue(0)->GetNPackets();

    // Write in format: timestamp queue_size
    g_router1_queue << std::fixed << std::setprecision(3) << now.GetSeconds() << " " << size
                    << std::endl;

    // Reschedule the next sample using the same interval as throughput
    Simulator::Schedule(g_sampleInterval, &TraceQueueSize, queueDisc);
}

} // namespace

// TODO:
// 10 vs no_ecn
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
    std::string clientLinkRate = "1000Mbps";
    double clientLinkMinDelayMs = 5.0;
    double clientLinkMaxDelayMs = 50.0;
    Time startTime = Seconds(0);
    Time clientStartStagger = MilliSeconds(2);
    Time stopTime = Seconds(10);
    g_sampleInterval = Seconds(0.1);
    bool enablePcap = false;
    bool useEcn = true;
    double markThreshold = 0.1;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nClients", "Number of video clients", nClients);
    cmd.AddValue("videoBytes", "Bytes sent to each client; 0 means unlimited", videoBytes);
    cmd.AddValue("packetSize", "Video packet payload size in bytes", packetSize);
    cmd.AddValue("videoRate", "Per-client application send rate", videoRate);
    cmd.AddValue("tcpTypeId", "TCP congestion control, e.g., TcpCubic or TcpBbr", tcpTypeId);
    cmd.AddValue("serverLinkRate",
                 "Data rate of the media server to router 1 link",
                 serverLinkRate);
    cmd.AddValue("serverLinkDelay", "Delay of the media server to router 1 link", serverLinkDelay);
    cmd.AddValue("routerLinkRate", "Data rate of the router 1 to router 2/3 links", routerLinkRate);
    cmd.AddValue("routerLinkDelay", "Delay of the router 1 to router 2/3 links", routerLinkDelay);
    cmd.AddValue("clientLinkRate",
                 "Data rate of each router-to-client access link",
                 clientLinkRate);
    cmd.AddValue("clientLinkMinDelayMs",
                 "Minimum random delay of each router-to-client access link in milliseconds",
                 clientLinkMinDelayMs);
    cmd.AddValue("clientLinkMaxDelayMs",
                 "Maximum random delay of each router-to-client access link in milliseconds",
                 clientLinkMaxDelayMs);
    cmd.AddValue("startTime", "Time when the first client download starts", startTime);
    cmd.AddValue("clientStartStagger",
                 "Delay between consecutive client starts",
                 clientStartStagger);
    cmd.AddValue("stopTime", "Application stop time", stopTime);
    cmd.AddValue("sampleInterval", "Aggregate throughput sample interval", g_sampleInterval);
    cmd.AddValue("enablePcap", "Enable pcap tracing on the shared server link", enablePcap);
    cmd.AddValue("useEcn", "Enable ECN for TCP and the FIFO queue disc", useEcn);
    cmd.AddValue("markThreshold", "ECN marking threshold for the queue disc", markThreshold);

    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::" + tcpTypeId));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(16 * 1024 * 1024));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(16 * 1024 * 1024));
    Config::SetDefault("ns3::TcpSocketBase::UseEcn",
                       (useEcn ? EnumValue(TcpSocketState::On) : EnumValue(TcpSocketState::On))); // always on on tcp socket

    // Print the specific values being used for this simulation run
    std::cout << "\n========== Simulation Configuration Values ==========" << std::endl;
    std::cout << "nClients:           " << nClients << std::endl;
    std::cout << "videoBytes:         " << videoBytes << std::endl;
    std::cout << "packetSize:         " << packetSize << std::endl;
    std::cout << "videoRate:          " << videoRate << std::endl;
    std::cout << "tcpTypeId:          " << tcpTypeId << std::endl;
    std::cout << "serverLinkRate:     " << serverLinkRate << std::endl;
    std::cout << "serverLinkDelay:    " << serverLinkDelay << std::endl;
    std::cout << "routerLinkRate:     " << routerLinkRate << std::endl;
    std::cout << "routerLinkDelay:    " << routerLinkDelay << std::endl;
    std::cout << "clientLinkRate:     " << clientLinkRate << std::endl;
    std::cout << "clientLinkDelay:    " << clientLinkMinDelayMs << "ms to " << clientLinkMaxDelayMs
              << "ms (uniform random per client link)" << std::endl;
    std::cout << "startTime:          " << startTime.GetSeconds() << "s" << std::endl;
    std::cout << "clientStartStagger: " << clientStartStagger.GetMilliSeconds() << "ms"
              << std::endl;
    std::cout << "stopTime:           " << stopTime.GetSeconds() << "s" << std::endl;
    std::cout << "sampleInterval:     " << g_sampleInterval.GetSeconds() << "s" << std::endl;
    std::cout << "enablePcap:         " << (enablePcap ? "true" : "false") << std::endl;
    std::cout << "useEcn:             " << (useEcn ? "true" : "false") << std::endl;
    std::cout << "markThreshold:      " << markThreshold << std::endl;
    std::cout << "====================================================\n" << std::endl;

    g_clients_throughput.open("clients_throughput.dat");
    g_clients_throughput << "# time(s) \"client_ip\" throughput(Mbps)" << std::endl;

    g_router1_throughput.open("router1_throughput.dat");
    g_router1_throughput << "# timestamp throughput" << std::endl;

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

    Ptr<UniformRandomVariable> clientDelayRv = CreateObject<UniformRandomVariable>();
    clientDelayRv->SetAttribute("Min", DoubleValue(clientLinkMinDelayMs));
    clientDelayRv->SetAttribute("Max", DoubleValue(clientLinkMaxDelayMs));

    NetDeviceContainer serverDevices = serverLink.Install(server.Get(0), routers.Get(0));
    NetDeviceContainer r1r2Devices = routerLink.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer r1r3Devices = routerLink.Install(routers.Get(0), routers.Get(2));

    TrafficControlHelper tch;
    if (useEcn)
    {
        tch.SetRootQueueDisc("ns3::FifoQueueEcnDisc", "MarkThreshold", DoubleValue(markThreshold));
    }
    else
    {
        tch.SetRootQueueDisc("ns3::FifoQueueDisc");
    }
    //    tch.Install(serverDevices.Get(1));
    QueueDiscContainer qdc = tch.Install(r1r2Devices.Get(0));
    tch.Install(r1r3Devices.Get(0));

    Simulator::Schedule(startTime, &TraceQueueSize, qdc.Get(0));

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
        clientLink.SetChannelAttribute("Delay",
                                       TimeValue(MilliSeconds(clientDelayRv->GetValue())));
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

    // Open server throughput file and write header
    g_server_throughput.open("server_throughput.dat");
    g_server_throughput << "# timestamp throughput" << std::endl;

    g_router1_queue.open("router1_queue.dat");
    g_router1_queue << "# timestamp queue_size" << std::endl;

    g_previousSampleTime = Seconds(0);
    Simulator::Schedule(g_sampleInterval, &TraceThroughput);

    // serverDevices.Get(0) is the server's interface connected to router 1
    serverDevices.Get(0)->TraceConnectWithoutContext("MacTx", MakeCallback(&ServerTransmitSink));
    serverDevices.Get(1)->TraceConnectWithoutContext("MacRx", MakeCallback(&Router1ReceiveSink));

    g_server_rtt.open("server_rtt.dat");
    g_server_rtt << "# timestamp rtt(ms)" << std::endl;
    Simulator::Schedule(g_sampleInterval, &TraceRtt);

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

    Simulator::Destroy();
    g_clients_throughput.close();
    g_server_throughput.close();
    g_router1_throughput.close();
    g_server_rtt.close();
    g_router1_queue.close();
    return 0;
}
